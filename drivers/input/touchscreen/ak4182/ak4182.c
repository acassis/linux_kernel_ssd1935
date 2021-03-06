#include <linux/hwmon.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <asm/irq.h>
#include "ak4182.h"

#ifdef	CONFIG_ARM
#include <asm/mach-types.h>
#ifdef	CONFIG_ARCH_OMAP
#include <asm/arch/gpio.h>
#endif
#endif

#define TS_POLL_DELAY	(1 * 1000000)	/* ns delay before the first sample */
#define TS_POLL_PERIOD	(5 * 1000000)	/* ns delay between samples */

/* this driver doesn't aim at the peak continuous sample rate */
#define	SAMPLE_BITS	(8 /*cmd*/ + 16 /*sample*/ + 2 /* before, after */)

struct ts_tmp {
	u16 	old_x;
	u16 	old_y;
};

static struct ts_tmp *tp;

struct ts_event {
	u16	x;
	u16	y;
	u16	z1, z2;
	int	ignore;
};

struct ak4182 {
	struct input_dev	*input;
	char			phys[32];

	struct spi_device	*spi;

#if defined(CONFIG_HWMON) || defined(CONFIG_HWMON_MODULE)
	struct attribute_group	*attr_group;
	struct device		*hwmon;
#endif

	u16			model;
	u16			vref_delay_usecs;
	u16			x_plate_ohms;
	u16			pressure_max;

	u8			read_x, read_y, read_z1, read_z2, pwrdown;
	u16			dummy;		/* for the pwrdown read */
	struct ts_event		tc;

	struct spi_transfer	xfer[18];
	struct spi_message	msg[5];
	struct spi_message	*last_msg;
	int			msg_idx;
	int			read_cnt;
	int			read_rep;
	int			last_read;

	u16			debounce_max;
	u16			debounce_tol;
	u16			debounce_rep;

	spinlock_t		lock;
	struct hrtimer		timer;
	unsigned		pendown:1;	/* P: lock */
	unsigned		pending:1;	/* P: lock */
// FIXME remove "irq_disabled"
	unsigned		irq_disabled:1;	/* P: lock */
	unsigned		disabled:1;

	u16			penirq_recheck_delay_usecs;

	int			(*filter)(void *data, int data_idx, int *val);
	void			*filter_data;
	void			(*filter_cleanup)(void *data);
	int			(*get_pendown_state)(void);
};

/* leave chip selected when we're done, for quicker re-select? */
#if	0
#define	CS_CHANGE(xfer)	((xfer).cs_change = 1)
#else
#define	CS_CHANGE(xfer)	((xfer).cs_change = 0)
#endif

/*--------------------------------------------------------------------------*/

/* The ak4182 has touchscreen and other sensors. */
#define	ADS_START		(1 << 7)
#define	ADS_A2A1A0_d_y		(1 << 4)	/* differential */
#define	ADS_A2A1A0_d_z1		(3 << 4)	/* differential */
#define	ADS_A2A1A0_d_z2		(4 << 4)	/* differential */
#define	ADS_A2A1A0_d_x		(5 << 4)	/* differential */
#define	ADS_A2A1A0_temp0	(0 << 4)	/* non-differential */
#define	ADS_A2A1A0_vbatt	(2 << 4)	/* non-differential */
#define	ADS_A2A1A0_vaux		(6 << 4)	/* non-differential */
#define	ADS_A2A1A0_temp1	(7 << 4)	/* non-differential */
#define	ADS_8_BIT		(1 << 3)
#define	ADS_12_BIT		(0 << 3)
#define	ADS_SER			(1 << 2)	/* non-differential */
#define	ADS_DFR			(0 << 2)	/* differential */
#define	ADS_PD10_PDOWN		(0 << 0)	/* lowpower mode + penirq */
#define	ADS_PD10_ADC_ON		(1 << 0)	/* ADC on */
#define	ADS_PD10_REF_ON		(2 << 0)	/* vREF on + penirq */
#define	ADS_PD10_ALL_ON		(3 << 0)	/* ADC + vREF on */

#define	MAX_12BIT	((1<<12)-1)

/* leave ADC powered up (disables penirq) between differential samples */
#define	READ_12BIT_DFR(x, adc, vref) (ADS_START | ADS_A2A1A0_d_ ## x \
	| ADS_12_BIT | ADS_DFR | \
	(adc ? ADS_PD10_ADC_ON : 0) | (vref ? ADS_PD10_REF_ON : 0))

#define	READ_Y(vref)	(READ_12BIT_DFR(y,  1, vref))
#define	READ_Z1(vref)	(READ_12BIT_DFR(z1, 1, vref))
#define	READ_Z2(vref)	(READ_12BIT_DFR(z2, 1, vref))

#define	READ_X(vref)	(READ_12BIT_DFR(x,  1, vref))
#define	PWRDOWN		(READ_12BIT_DFR(y,  0, 0))	/* LAST */

/* single-ended samples need to first power up reference voltage;
 * we leave both ADC and VREF powered
 */
#define	READ_12BIT_SER(x) (ADS_START | ADS_A2A1A0_ ## x \
	| ADS_12_BIT | ADS_SER)
	
#define	REF_ON	(READ_12BIT_DFR(x, 1, 1))
#define	REF_OFF	(READ_12BIT_DFR(y, 0, 0))

/*--------------------------------------------------------------------------*/

/*
 * Non-touchscreen sensors only use single-ended conversions.
 * The range is GND..vREF.ak4182 lets that pin be unconnected, to use internal vREF.
 */
static unsigned vREF_mV;
module_param(vREF_mV, uint, 0);
MODULE_PARM_DESC(vREF_mV, "external vREF voltage, in milliVolts");

struct ser_req {
	u8			ref_on;
	u8			command;
	u8			ref_off;
	u16			scratch;
	__be16			sample;
	struct spi_message	msg;
	struct spi_transfer	xfer[6];
};

static void ak4182_enable(struct ak4182 *ts);
static void ak4182_disable(struct ak4182 *ts);

static int device_suspended(struct device *dev)
{
	struct ak4182 *ts = dev_get_drvdata(dev);
	return dev->power.power_state.event != PM_EVENT_ON || ts->disabled;
}

static int ak4182_read12_ser(struct device *dev, unsigned command)
{
	struct spi_device	*spi = to_spi_device(dev);
	struct ak4182		*ts = dev_get_drvdata(dev);
	struct ser_req		*req = kzalloc(sizeof *req, GFP_KERNEL);
	int			status;
	int			sample;
	int			use_internal;

	if (!req)
		return -ENOMEM;

	spi_message_init(&req->msg);

	/* FIXME boards with ak4182 might use external vref instead ... */
	use_internal = (ts->model == 4182);

	/* maybe turn on internal vREF, and let it settle */
	if (use_internal) {
		req->ref_on = REF_ON;
		req->xfer[0].tx_buf = &req->ref_on;
		req->xfer[0].len = 1;
		spi_message_add_tail(&req->xfer[0], &req->msg);

		req->xfer[1].rx_buf = &req->scratch;
		req->xfer[1].len = 2;

		/* for 1uF, settle for 800 usec; no cap, 100 usec.  */
		req->xfer[1].delay_usecs = ts->vref_delay_usecs;
		spi_message_add_tail(&req->xfer[1], &req->msg);
	}

	/* take sample */
	req->command = (u8) command;
	req->xfer[2].tx_buf = &req->command;
	req->xfer[2].len = 1;
	spi_message_add_tail(&req->xfer[2], &req->msg);
	req->xfer[3].rx_buf = &req->sample;
	
	req->xfer[3].len = 2;
	spi_message_add_tail(&req->xfer[3], &req->msg);

	/* REVISIT:  take a few more samples, and compare ... */

	/* converter in low power mode & enable PENIRQ */
	req->ref_off = PWRDOWN;
	req->xfer[4].tx_buf = &req->ref_off;
	req->xfer[4].len = 1;
	spi_message_add_tail(&req->xfer[4], &req->msg);

	req->xfer[5].rx_buf = &req->scratch;
	req->xfer[5].len = 2;
	CS_CHANGE(req->xfer[5]);
	spi_message_add_tail(&req->xfer[5], &req->msg);

	ts->irq_disabled = 1;
	disable_irq(spi->irq);
	status = spi_sync(spi, &req->msg);
	ts->irq_disabled = 0;
	enable_irq(spi->irq);

	if (req->msg.status)
		status = req->msg.status;

	/* on-wire is a must-ignore bit, a BE12 value, then padding */
	sample = be16_to_cpu(req->sample);
	sample = sample >> 3;
	sample &= 0x0fff;

	kfree(req);
	return status ? status : sample;
}

#if defined(CONFIG_HWMON) || defined(CONFIG_HWMON_MODULE)
#define SHOW(name, var, adjust) static ssize_t \
name ## _show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	struct ak4182 *ts = dev_get_drvdata(dev); \
	ssize_t v = ak4182_read12_ser(dev, \
			READ_12BIT_SER(var) | ADS_PD10_ALL_ON); \
	if (v < 0) \
		return v; \
	return sprintf(buf, "%u\n", adjust(ts, v)); \
} \
static DEVICE_ATTR(name, S_IRUGO, name ## _show, NULL);

/* Sysfs conventions report temperatures in millidegrees Celcius.
 * ak4182 could use the low-accuracy two-sample scheme, but can't do the high
 * accuracy scheme without calibration data.  For now we won't try either;
 * userspace sees raw sensor values, and must scale/calibrate appropriately.
 */
static inline unsigned null_adjust(struct ak4182 *ts, ssize_t v)
{
	return v;
}

SHOW(temp0, temp0, null_adjust)		/* temp1_input */
SHOW(temp1, temp1, null_adjust)		/* temp2_input */


/* sysfs conventions report voltages in millivolts.  We can convert voltages
 * if we know vREF.  userspace may need to scale vAUX to match the board's
 * external resistors; we assume that vBATT only uses the internal ones.
 */
static inline unsigned vaux_adjust(struct ak4182 *ts, ssize_t v)
{
	unsigned retval = v;
	/* external resistors may scale vAUX into 0..vREF */
	retval *= vREF_mV;
	retval = retval >> 12;
	return retval;
}

static inline unsigned vbatt_adjust(struct ak4182 *ts, ssize_t v)
{
#if 0
	unsigned retval = vaux_adjust(ts, v);

	/* ak4182 has a resistor ladder to scale this signal down */
	if (ts->model == 4182)
		retval *= 4;
	return retval;
#endif

	unsigned retval,vbatt[10],average;
	int i,delta;
	unsigned max = 0,min = 100000,sum =0;
	if (ts->model == 4182) {
		for (i=0;i<10;i++) {
			retval = vaux_adjust(ts,v);
			retval *= 4;
			vbatt[i] = retval;
		}
	}
	for (i=0;i<10;i++) {
		max = max > vbatt[i]? max : vbatt[i];
		min = min < vbatt[i]? min : vbatt[i];
		sum += vbatt[i];
	}
	average = (sum -min -max) / 8;
	if (average <3250) delta = (-32)*average/44+2431;
	else if (average>=3250 && average <3487) delta = 36*average/237-425;
	else if (average>=3487 && average <3555) delta = (-24)*average/68+1334;
	else if (average>=3555 && average <3682) delta = 44*average/127-1151;
	else if (average>=3682 && average <3840) delta = (-100)*average/158+2454;
	else if (average>=3840 && average <4058) delta = 44*average/218-751;
	else delta=(-4)*average/108+218;

	delta = delta/2;
	
	if (abs(average - max)<delta && abs(average - min)<delta) {	
		retval = 1000*(average - 139)/959;
		return retval;
	}
	else return 0;
}

SHOW(in0_input, vaux, vaux_adjust)
SHOW(in1_input, vbatt, vbatt_adjust)

	
static struct attribute *ak4182_attributes[] = {
	&dev_attr_temp0.attr,
	&dev_attr_temp1.attr,
	&dev_attr_in0_input.attr,
	&dev_attr_in1_input.attr,
	NULL,
};

static struct attribute_group ak4182_attr_group = {
	.attrs = ak4182_attributes,
};

static int ak418x_hwmon_register(struct spi_device *spi, struct ak4182 *ts)
{
	struct device *hwmon;
	int err;
	
	if (ts->model==4182) {
		if (!vREF_mV) {
			dev_dbg(&spi->dev, "assuming 2.5V internal vREF\n");
			vREF_mV = 2500;
		}
		ts->attr_group = &ak4182_attr_group;
	}

	err = sysfs_create_group(&spi->dev.kobj, ts->attr_group);
	if (err)
		return err;

	hwmon = hwmon_device_register(&spi->dev);
	if (IS_ERR(hwmon)) {
		sysfs_remove_group(&spi->dev.kobj, ts->attr_group);
		return PTR_ERR(hwmon);
	}

	ts->hwmon = hwmon;
	return 0;
}

static void ak418x_hwmon_unregister(struct spi_device *spi,
				     struct ak4182 *ts)
{
	if (ts->hwmon) {
		sysfs_remove_group(&spi->dev.kobj, ts->attr_group);
		hwmon_device_unregister(ts->hwmon);
	}
}

#else
static inline int ak418x_hwmon_register(struct spi_device *spi,
					 struct ak4182 *ts)
{
	return 0;
}

static inline void ak418x_hwmon_unregister(struct spi_device *spi,
					    struct ak4182 *ts)
{
}
#endif

static int is_pen_down(struct device *dev)
{
	struct ak4182	*ts = dev_get_drvdata(dev);

	return ts->pendown;
}

static ssize_t ak4182_pen_down_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", is_pen_down(dev));
}

static DEVICE_ATTR(pen_down, S_IRUGO, ak4182_pen_down_show, NULL);

static ssize_t ak4182_disable_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ak4182	*ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->disabled);
}

static ssize_t ak4182_disable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct ak4182 *ts = dev_get_drvdata(dev);
	char *endp;
	int i;

	i = simple_strtoul(buf, &endp, 10);
	spin_lock_irq(&ts->lock);

	if (i)
		ak4182_disable(ts);
	else
		ak4182_enable(ts);

	spin_unlock_irq(&ts->lock);
	
	return count;
}

static DEVICE_ATTR(pm_ctrl, 0664, ak4182_disable_show, ak4182_disable_store);

static struct attribute *ak418x_attributes[] = {
	&dev_attr_pen_down.attr,
	&dev_attr_pm_ctrl.attr,
	NULL,
};

static struct attribute_group ak418x_attr_group = {
	.attrs = ak418x_attributes,
};

/*--------------------------------------------------------------------------*/

static void ak4182_rx(void *ads)
{
	struct ak4182		*ts = ads;
	unsigned		Rt;
	u16			x, y, z1, z2;

	/* ak4182_rx_val() did in-place conversion (including byteswap) from
	 * on-the-wire format as part of debouncing to get stable readings.
	 */
	x = ts->tc.x;
	y = ts->tc.y;
	z1 = ts->tc.z1;
	z2 = ts->tc.z2;
	
	/* range filtering */
	if (x == MAX_12BIT)
		x = 0;

	if (likely(x && z1)) {
		/* compute touch pressure resistance using equation #2 */
		Rt = z2;
		Rt -= z1;
		Rt *= x;
		Rt *= ts->x_plate_ohms;
		Rt /= z1;
		Rt = (Rt + 2047) >> 17;
	} else
		Rt = 0;

	if (ts->model == 7843)
		Rt = ts->pressure_max / 2;
/*	
	if ((abs(x - tp->old_x)>25)||(abs(y - tp->old_y)>25))
		ts->tc.ignore = 0;
	tp->old_x = x;
	tp->old_y = y;
*/	
	/* Sample found inconsistent by debouncing or pressure is beyond
	 * the maximum. Don't report it to user space, repeat at least
	 * once more the measurement
	 */
	if (ts->tc.ignore || Rt > ts->pressure_max) {
#ifdef VERBOSE
		pr_debug("%s: ignored %d pressure %d\n",
			ts->spi->dev.bus_id, ts->tc.ignore, Rt);
#endif
		hrtimer_start(&ts->timer, ktime_set(0, TS_POLL_PERIOD),
			      HRTIMER_MODE_REL);
		return;
	}


	if (Rt) {
		struct input_dev *input = ts->input;

		if (!ts->pendown) {
			input_report_key(input, BTN_TOUCH, 1);
			ts->pendown = 1;
#ifdef VERBOSE
			dev_dbg(&ts->spi->dev, "DOWN\n");
#endif
		}

		if((abs(x-tp->old_x)>5)&&(abs(y-tp->old_y)>5)){	
			input_report_abs(input, ABS_X, x);
			input_report_abs(input, ABS_Y, y);
			input_report_abs(input, ABS_PRESSURE, Rt);
			input_sync(input);
		}
		tp->old_x = ts->tc.x;
		tp->old_y = ts->tc.y;
#ifdef VERBOSE
		dev_dbg(&ts->spi->dev, "%4d/%4d/%4d\n", x, y, Rt);
#endif
	}


	hrtimer_start(&ts->timer, ktime_set(0, TS_POLL_PERIOD),
			HRTIMER_MODE_REL);
}

static int ak4182_debounce(void *ads, int data_idx, int *val)
{
	struct ak4182		*ts = ads;

	int num = abs(ts->last_read - *val);
	//if (!ts->read_cnt || (abs(ts->last_read - *val) > ts->debounce_tol)) {
	if (!ts->read_cnt || (num > ts->debounce_tol)) {
		/* Start over collecting consistent readings. */
		ts->read_rep = 0;
		/* Repeat it, if this was the first read or the read
		 * wasn't consistent enough. */
		if (ts->read_cnt < ts->debounce_max) {
			ts->last_read = *val;
			ts->read_cnt++;
			return ak4182_FILTER_REPEAT;
		} else {
			ts->read_cnt = 0;
			return ak4182_FILTER_IGNORE;
		}
	} else {
		if (++ts->read_rep > ts->debounce_rep) {
			/* Got a good reading for this coordinate,
			 * go for the next one. */
			ts->read_cnt = 0;
			ts->read_rep = 0;
			return ak4182_FILTER_OK;
		} else {
			/* Read more values that are consistent. */
			ts->read_cnt++;
			return ak4182_FILTER_REPEAT;
		}
	}
}

static int ak4182_no_filter(void *ads, int data_idx, int *val)
{
	return ak4182_FILTER_OK;
}

static void ak4182_rx_val(void *ads)
{
	struct ak4182 *ts = ads;
	struct spi_message *m;
	struct spi_transfer *t;
	u16 *rx_val;
	int val;
	int action;
	int status;

	m = &ts->msg[ts->msg_idx];
	t = list_entry(m->transfers.prev, struct spi_transfer, transfer_list);
	rx_val = t->rx_buf;

	val = be16_to_cpu(*rx_val) >> 3;
// sasi - bit15 is 1 instead of 0 !!!
val &= 0xfff;

	action = ts->filter(ts->filter_data, ts->msg_idx, &val);
	switch (action) {
	case ak4182_FILTER_REPEAT:
		break;
	case ak4182_FILTER_IGNORE:
		ts->tc.ignore = 1;
		/* Last message will contain ak4182_rx() as the
		 * completion function.
		 */
		m = ts->last_msg;
		break;
	case ak4182_FILTER_OK:
		*rx_val = val;
		ts->tc.ignore = 0;
		m = &ts->msg[++ts->msg_idx];
		break;
	default:
		BUG();
	}
	status = spi_async(ts->spi, m);
	if (status)
		dev_err(&ts->spi->dev, "spi_async --> %d\n",
				status);
}

static enum hrtimer_restart ak4182_timer(struct hrtimer *handle)
{
	struct ak4182	*ts = container_of(handle, struct ak4182, timer);
	int		status = 0;

	spin_lock_irq(&ts->lock);

	if (unlikely(!ts->get_pendown_state() ||
		     device_suspended(&ts->spi->dev))) {
		if (ts->pendown) {
			struct input_dev *input = ts->input;

			input_report_key(input, BTN_TOUCH, 0);
			input_report_abs(input, ABS_PRESSURE, 0);
			input_sync(input);

			ts->pendown = 0;
#ifdef VERBOSE
			dev_dbg(&ts->spi->dev, "UP\n");
#endif
		}

		/* measurement cycle ended */
		if (!device_suspended(&ts->spi->dev)) {
			ts->irq_disabled = 0;
			enable_irq(ts->spi->irq);
		}
		ts->pending = 0;
	} else {
		/* pen is still down, continue with the measurement */
		ts->msg_idx = 0;
		status = spi_async(ts->spi, &ts->msg[0]);
		if (status)
			dev_err(&ts->spi->dev, "spi_async --> %d\n", status);
	}

	spin_unlock_irq(&ts->lock);
	return HRTIMER_NORESTART;
}

static irqreturn_t ak4182_irq(int irq, void *handle)
{
	struct ak4182 *ts = handle;
	unsigned long flags;

	spin_lock_irqsave(&ts->lock, flags);
	if (likely(ts->get_pendown_state())) {
		if (!ts->irq_disabled) {
			ts->irq_disabled = 1;
			disable_irq(ts->spi->irq);
			ts->pending = 1;
			hrtimer_start(&ts->timer, ktime_set(0, TS_POLL_DELAY),
					HRTIMER_MODE_REL);
		}
	}
	spin_unlock_irqrestore(&ts->lock, flags);

	return IRQ_HANDLED;
}

/*--------------------------------------------------------------------------*/

/* Must be called with ts->lock held */
static void ak4182_disable(struct ak4182 *ts)
{
	if (ts->disabled)
		return;

	ts->disabled = 1;

	/* are we waiting for IRQ, or polling? */
	if (!ts->pending) {
		ts->irq_disabled = 1;
		disable_irq(ts->spi->irq);
	} else {
		/* the timer will run at least once more, and
		 * leave everything in a clean state, IRQ disabled
		 */
		while (ts->pending) {
			spin_unlock_irq(&ts->lock);
			msleep(1);
			spin_lock_irq(&ts->lock);
		}
	}

}

/* Must be called with ts->lock held */
static void ak4182_enable(struct ak4182 *ts)
{
	if (!ts->disabled)
		return;

	ts->disabled = 0;
	ts->irq_disabled = 0;
	enable_irq(ts->spi->irq);
}

static int ak4182_suspend(struct spi_device *spi, pm_message_t message)
{
	struct ak4182 *ts = dev_get_drvdata(&spi->dev);

	spin_lock_irq(&ts->lock);

	spi->dev.power.power_state = message;
	ak4182_disable(ts);

	spin_unlock_irq(&ts->lock);

	return 0;

}

static int ak4182_resume(struct spi_device *spi)
{
	struct ak4182 *ts = dev_get_drvdata(&spi->dev);

	spin_lock_irq(&ts->lock);

	spi->dev.power.power_state = PMSG_ON;
	ak4182_enable(ts);

	spin_unlock_irq(&ts->lock);

	return 0;
}

static int __devinit ak4182_probe(struct spi_device *spi)
{
	struct ak4182			*ts;
	struct input_dev		*input_dev;
	struct ak4182_platform_data	*pdata = spi->dev.platform_data;
	struct spi_message		*m;
	struct spi_transfer		*x;
	int				vref;
	int				err;
	
//	static struct ts_tmp		*tp;
	tp = kmalloc(sizeof(tp),GFP_KERNEL);
	
	if (!spi->irq) {
		dev_dbg(&spi->dev, "no IRQ?\n");
		return -ENODEV;
	}

	if (!pdata) {
		dev_dbg(&spi->dev, "no platform data?\n");
		return -ENODEV;
	}

	/* don't exceed max specified sample rate */
	if (spi->max_speed_hz > (125000 * SAMPLE_BITS)) {
		dev_dbg(&spi->dev, "f(sample) %d KHz?\n",
				(spi->max_speed_hz/SAMPLE_BITS)/1000);
		return -EINVAL;
	}

	if (pdata->get_pendown_state == NULL) {
		dev_dbg(&spi->dev, "no get_pendown_state function?\n");
		return -EINVAL;
	}

	/* We'd set TX wordsize 8 bits and RX wordsize to 13 bits ... except
	 * that even if the hardware can do that, the SPI controller driver
	 * may not.  So we stick to very-portable 8 bit words, both RX and TX.
	 */
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	err = spi_setup(spi);
	if (err < 0)
		return err;

	ts = kzalloc(sizeof(struct ak4182), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	dev_set_drvdata(&spi->dev, ts);
	spi->dev.power.power_state = PMSG_ON;

	ts->spi = spi;
	ts->input = input_dev;

	hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->timer.function = ak4182_timer;

	spin_lock_init(&ts->lock);

	ts->model = pdata->model ? : 4182;
	ts->vref_delay_usecs = pdata->vref_delay_usecs ? : 100;
	ts->x_plate_ohms = pdata->x_plate_ohms ? : 400;
	ts->pressure_max = pdata->pressure_max ? : ~0;

	if (pdata->filter != NULL) {
		if (pdata->filter_init != NULL) {
			err = pdata->filter_init(pdata, &ts->filter_data);
			if (err < 0)
				goto err_free_mem;
		}
		ts->filter = pdata->filter;
		ts->filter_cleanup = pdata->filter_cleanup;
	} else if (pdata->debounce_max) {
		ts->debounce_max = pdata->debounce_max;
		if (ts->debounce_max < 2)
			ts->debounce_max = 2;
		ts->debounce_tol = pdata->debounce_tol;
		ts->debounce_rep = pdata->debounce_rep;
		ts->filter = ak4182_debounce;
		ts->filter_data = ts;
	} else
		ts->filter = ak4182_no_filter;
	ts->get_pendown_state = pdata->get_pendown_state;

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", spi->dev.bus_id);

	input_dev->name = "ak418x Touchscreen";
	input_dev->phys = ts->phys;
	input_dev->dev.parent = &spi->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X,
			pdata->x_min ? : 0,
			pdata->x_max ? : MAX_12BIT,
			0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			pdata->y_min ? : 0,
			pdata->y_max ? : MAX_12BIT,
			0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			pdata->pressure_min, pdata->pressure_max, 0, 0);

	vref = pdata->keep_vref_on;

	/* set up the transfers to read touchscreen state; this assumes we
	 * use formula #2 for pressure, not #3.
	 */
	m = &ts->msg[0];
	x = ts->xfer;

	spi_message_init(m);

	/* y- still on; turn on only y+ (and ADC) */
	ts->read_y = READ_Y(vref);
	x->tx_buf = &ts->read_y;
	x->len = 1;
	spi_message_add_tail(x, m);

	x++;
	x->rx_buf = &ts->tc.y;
	x->len = 2;
	spi_message_add_tail(x, m);

	m->complete = ak4182_rx_val;
	m->context = ts;

	m++;
	spi_message_init(m);

	/* turn y- off, x+ on, then leave in lowpower */
	x++;
	ts->read_x = READ_X(vref);
	x->tx_buf = &ts->read_x;
	x->len = 1;
	spi_message_add_tail(x, m);

	x++;
	x->rx_buf = &ts->tc.x;
	x->len = 2;
	spi_message_add_tail(x, m);

	m->complete = ak4182_rx_val;
	m->context = ts;

	/* turn y+ off, x- on; we'll use formula #2 */
	if (ts->model == 4182) {
		m++;
		spi_message_init(m);

		x++;
		ts->read_z1 = READ_Z1(vref);
		x->tx_buf = &ts->read_z1;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &ts->tc.z1;
		x->len = 2;
		spi_message_add_tail(x, m);

		m->complete = ak4182_rx_val;
		m->context = ts;

		m++;
		spi_message_init(m);

		x++;
		ts->read_z2 = READ_Z2(vref);
		x->tx_buf = &ts->read_z2;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &ts->tc.z2;
		x->len = 2;
		spi_message_add_tail(x, m);

		m->complete = ak4182_rx_val;
		m->context = ts;
	}

	/* power down */
	m++;
	spi_message_init(m);

	x++;
	ts->pwrdown = PWRDOWN;
	x->tx_buf = &ts->pwrdown;
	x->len = 1;
	spi_message_add_tail(x, m);

	x++;
	x->rx_buf = &ts->dummy;
	x->len = 2;
	CS_CHANGE(*x);
	spi_message_add_tail(x, m);

	m->complete = ak4182_rx;
	m->context = ts;

	ts->last_msg = m;

	if (request_irq(spi->irq, ak4182_irq, IRQF_TRIGGER_LOW,
			spi->dev.driver->name, ts)) {
		dev_dbg(&spi->dev, "irq %d busy?\n", spi->irq);
		err = -EBUSY;
		goto err_cleanup_filter;
	}

	err = ak418x_hwmon_register(spi, ts);
	if (err)
		goto err_free_irq;

	dev_info(&spi->dev, "touchscreen, irq %d\n", spi->irq);

	/* take a first sample, leaving nPENIRQ active and vREF off; avoid
	 * the touchscreen, in case it's not connected.
	 */
	(void) ak4182_read12_ser(&spi->dev,
			  READ_12BIT_SER(vaux) | ADS_PD10_ALL_ON);

	err = sysfs_create_group(&spi->dev.kobj, &ak418x_attr_group);
	if (err)
		goto err_remove_hwmon;

	err = input_register_device(input_dev);
	if (err)
		goto err_remove_attr_group;

	return 0;

 err_remove_attr_group:
	sysfs_remove_group(&spi->dev.kobj, &ak418x_attr_group);
 err_remove_hwmon:
	ak418x_hwmon_unregister(spi, ts);
 err_free_irq:
	free_irq(spi->irq, ts);
 err_cleanup_filter:
	if (ts->filter_cleanup)
		ts->filter_cleanup(ts->filter_data);
 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
	return err;
}

static int __devexit ak4182_remove(struct spi_device *spi)
{
	struct ak4182		*ts = dev_get_drvdata(&spi->dev);

	ak418x_hwmon_unregister(spi, ts);
	input_unregister_device(ts->input);

	ak4182_suspend(spi, PMSG_SUSPEND);

	sysfs_remove_group(&spi->dev.kobj, &ak418x_attr_group);

	free_irq(ts->spi->irq, ts);
	/* suspend left the IRQ disabled */
	enable_irq(ts->spi->irq);

	if (ts->filter_cleanup)
		ts->filter_cleanup(ts->filter_data);

	kfree(ts);
	kfree(tp);

	dev_dbg(&spi->dev, "unregistered touchscreen\n");
	return 0;
}

static struct spi_driver ak4182_driver = {
	.driver = {
		.name	= "ak4182",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= ak4182_probe,
	.remove		= __devexit_p(ak4182_remove),
	.suspend	= ak4182_suspend,
	.resume		= ak4182_resume,
};

static int __init ak4182_init(void)
{
	return spi_register_driver(&ak4182_driver);
}
module_init(ak4182_init);

static void __exit ak4182_exit(void)
{
	spi_unregister_driver(&ak4182_driver);

}
module_exit(ak4182_exit);

MODULE_DESCRIPTION("ak4182 TouchScreen Driver");
MODULE_LICENSE("GPL");
