/*
 * Seiko Instruments S-35390A RTC Driver
 *
 * Copyright (c) 2007 Byron Bradley
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/i2c.h>
#include <linux/bitrev.h>
#include <linux/bcd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/rtc/s35390a.h>

#define S35390A_CMD_STATUS1	0
#define S35390A_CMD_STATUS2	1
#define S35390A_CMD_TIME1	2
#define S35390A_CMD_INTR1	4
#define S35390A_CMD_INTR2	5

#define S35390A_BYTE_YEAR	0
#define S35390A_BYTE_MONTH	1
#define S35390A_BYTE_DAY	2
#define S35390A_BYTE_WDAY	3
#define S35390A_BYTE_HOURS	4
#define S35390A_BYTE_MINS	5
#define S35390A_BYTE_SECS	6

#define S35390A_FLAG_POC	0x01
#define S35390A_FLAG_BLD	0x02
#define S35390A_FLAG_INT2	0x04
#define S35390A_FLAG_INT1	0x08
#define S35390A_FLAG_24H	0x40
#define S35390A_FLAG_RESET	0x80
#define S35390A_FLAG_TEST	0x01
#define S35390A_FLAG_INT2AE	0x02
#define S35390A_FLAG_INT2ME	0x04
#define S35390A_FLAG_INT2FE	0x08
#define S35390A_FLAG_32KE	0x10
#define S35390A_FLAG_INT1AE	0x20
#define S35390A_FLAG_INT1ME	0x40
#define S35390A_FLAG_INT1FE	0x80
#define S35390A_FLAG_A12WE	0x01
#define S35390A_FLAG_A12HE	0x01
#define S35390A_FLAG_A12ME	0x01
#define ALARM_DISABLED		1

struct s35390a {
	struct i2c_client	*client[8];
	struct rtc_device	*rtc;
	int			twentyfourhour;
	struct work_struct	work1;
	struct work_struct	work2;
//	struct mutex		mutex;
};

static int s35390a_set_reg(struct s35390a *s35390a, int reg, char *buf, int len)
{
	struct i2c_client *client = s35390a->client[reg];
	struct i2c_msg msg[] = {
		{ client->addr, 0, len, buf },
	};

	if ((i2c_transfer(client->adapter, msg, 1)) != 1)
		return -EIO;

	return 0;
}

static int s35390a_get_reg(struct s35390a *s35390a, int reg, char *buf, int len)
{
	struct i2c_client *client = s35390a->client[reg];
	struct i2c_msg msg[] = {
		{ client->addr, I2C_M_RD, len, buf },
	};

	if ((i2c_transfer(client->adapter, msg, 1)) != 1)
		return -EIO;

	return 0;
}

static int s35390a_reset(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf)) < 0)
		return -EIO;

	if (!(buf[0] & (S35390A_FLAG_POC | S35390A_FLAG_BLD)))
		return 0;
	
	// Only for debug
	printk(KERN_ALERT"RTC: reset RTC[00:00].\n");

	buf[0] |= (S35390A_FLAG_RESET | S35390A_FLAG_24H);
	buf[0] &= 0xf0;
	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf));
}

static int s35390a_disable_test_mode(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf)) < 0)
		return -EIO;

	if (!(buf[0] & S35390A_FLAG_TEST))
		return 0;

	buf[0] &= ~S35390A_FLAG_TEST;
	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf));
}

static int s35390a_enable_alarm1(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf)) < 0)
		return -EIO;

	if ((buf[0] & S35390A_FLAG_INT1AE) &&
		!(buf[0] & (S35390A_FLAG_32KE | S35390A_FLAG_INT1ME |S35390A_FLAG_INT1FE)))
		return 0;

	buf[0] |= S35390A_FLAG_INT1AE;
	buf[0] &= ~(S35390A_FLAG_32KE | S35390A_FLAG_INT1ME | S35390A_FLAG_INT1FE);

	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf));
}

static int s35390a_disable_alarm1(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf)) < 0)
		return -EIO;

	if (!(buf[0] & (S35390A_FLAG_INT1AE | S35390A_FLAG_INT1ME | S35390A_FLAG_INT1FE)))
		return ALARM_DISABLED;

	buf[0] &= ~(S35390A_FLAG_INT1AE | S35390A_FLAG_32KE | S35390A_FLAG_INT1ME | S35390A_FLAG_INT1FE);

	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf));
}

static int s35390a_enable_alarm2(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf)) < 0)
		return -EIO;

	if ((buf[0] & S35390A_FLAG_INT2AE) &&
		!(buf[0] & (S35390A_FLAG_INT2ME |S35390A_FLAG_INT2FE)))
		return 0;

	buf[0] |= S35390A_FLAG_INT2AE;
	 buf[0] &= ~(S35390A_FLAG_INT2ME | S35390A_FLAG_INT2FE);

	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf));
}

static int s35390a_disable_alarm2(struct s35390a *s35390a)
{
	char buf[1];

	if (s35390a_get_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf)) < 0)
		return -EIO;

	if (!(buf[0] & (S35390A_FLAG_INT2AE | S35390A_FLAG_INT2ME | S35390A_FLAG_INT2FE)))
		return ALARM_DISABLED;

	buf[0] &= ~(S35390A_FLAG_INT2AE | S35390A_FLAG_INT2ME | S35390A_FLAG_INT2FE);

	return s35390a_set_reg(s35390a, S35390A_CMD_STATUS2, buf, sizeof(buf));
}

static char s35390a_hr2reg(struct s35390a *s35390a, int hour)
{
	if (s35390a->twentyfourhour)
		return BIN2BCD(hour);

	if (hour < 12)
		return BIN2BCD(hour);

	return 0x40 | BIN2BCD(hour - 12);
}

static int s35390a_reg2hr(struct s35390a *s35390a, char reg)
{
	unsigned hour;

	if (s35390a->twentyfourhour)
		return BCD2BIN(reg & 0x3f);

	hour = BCD2BIN(reg & 0x3f);
	if (reg & 0x40)
		hour += 12;

	return hour;
}

static int s35390a_set_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	struct s35390a	*s35390a = i2c_get_clientdata(client);
	int i, err;
	char buf[7];

	dev_dbg(&client->dev, "%s: tm is secs=%d, mins=%d, hours=%d mday=%d, "
		"mon=%d, year=%d, wday=%d\n", __FUNCTION__, tm->tm_sec,
		tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year,
		tm->tm_wday);

	buf[S35390A_BYTE_YEAR] = BIN2BCD(tm->tm_year - 100);
	buf[S35390A_BYTE_MONTH] = BIN2BCD(tm->tm_mon + 1);
	buf[S35390A_BYTE_DAY] = BIN2BCD(tm->tm_mday);
	buf[S35390A_BYTE_WDAY] = BIN2BCD(tm->tm_wday);
	buf[S35390A_BYTE_HOURS] = s35390a_hr2reg(s35390a, tm->tm_hour);
	buf[S35390A_BYTE_MINS] = BIN2BCD(tm->tm_min);
	buf[S35390A_BYTE_SECS] = BIN2BCD(tm->tm_sec);

	/* This chip expects the bits of each byte to be in reverse order */
	for (i = 0; i < 7; ++i)
		buf[i] = bitrev8(buf[i]);

	err = s35390a_set_reg(s35390a, S35390A_CMD_TIME1, buf, sizeof(buf));

	return err;
}

static int s35390a_get_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	struct s35390a *s35390a = i2c_get_clientdata(client);
	char buf[7];
	int i, err;

	err = s35390a_get_reg(s35390a, S35390A_CMD_TIME1, buf, sizeof(buf));
	if (err < 0)
		return err;

	/* This chip returns the bits of each byte in reverse order */
	for (i = 0; i < 7; ++i)
		buf[i] = bitrev8(buf[i]);

	// Only for debug
	printk(KERN_ALERT"RTC: get datatime, H/M[0x%x:0x%x]\n", buf[S35390A_BYTE_HOURS], buf[S35390A_BYTE_MINS]);
	
	tm->tm_sec = BCD2BIN(buf[S35390A_BYTE_SECS]);
	tm->tm_min = BCD2BIN(buf[S35390A_BYTE_MINS]);
	tm->tm_hour = s35390a_reg2hr(s35390a, buf[S35390A_BYTE_HOURS]);
	tm->tm_wday = BCD2BIN(buf[S35390A_BYTE_WDAY]);
	tm->tm_mday = BCD2BIN(buf[S35390A_BYTE_DAY]);
	tm->tm_mon = BCD2BIN(buf[S35390A_BYTE_MONTH]) - 1;
	tm->tm_year = BCD2BIN(buf[S35390A_BYTE_YEAR]) + 100;

	dev_dbg(&client->dev, "%s: tm is secs=%d, mins=%d, hours=%d, mday=%d, "
		"mon=%d, year=%d, wday=%d\n", __FUNCTION__, tm->tm_sec,
		tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year,
		tm->tm_wday);

	return rtc_valid_tm(tm);
}

static int s35390a_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return s35390a_get_datetime(to_i2c_client(dev), tm);
}

static int s35390a_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return s35390a_set_datetime(to_i2c_client(dev), tm);
}

#if defined(CONFIG_RTC_INTF_DEV) || defined(CONFIG_RTC_INTF_DEV_MODULE)

static int s35390a_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct i2c_client	*client = to_i2c_client(dev);
	struct s35390a		*s35390a = i2c_get_clientdata(client);

	switch (cmd) {
	case RTC_AIE_OFF:	/* alarm off */
		s35390a_disable_alarm1(s35390a);
		s35390a_disable_alarm2(s35390a);
		break;
	case RTC_AIE_ON:	/* alarm on */
		s35390a_enable_alarm1(s35390a);
		s35390a_enable_alarm2(s35390a);
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

#else
#define	s35390a_rtc_ioctl	NULL
#endif

static int s35390a_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client	*client = to_i2c_client(dev);
	struct s35390a		*s35390a = i2c_get_clientdata(client);
	int	i;
	int	ret = 0;
	unsigned char		buf[3];
	struct rtc_time		*tm = &alrm->time;

	if (tm == NULL)
		return -1;

//mutex_lock(&s35390a->mutex);
	ret = s35390a_get_reg(s35390a, S35390A_CMD_INTR2, buf, sizeof(buf));

	if (ret < 0)
		goto out;

	buf[0] &= ~S35390A_FLAG_A12WE;
	buf[1] &= ~S35390A_FLAG_A12HE;
	buf[2] &= ~S35390A_FLAG_A12ME;

	/* This chip returns the bits of each byte in reverse order */
	for (i = 0; i < 3; ++i)
		buf[i] = bitrev8(buf[i]);

	tm->tm_wday = -1;
	tm->tm_hour = s35390a_reg2hr(s35390a, buf[1]);
	tm->tm_min = BCD2BIN(buf[2]);
	tm->tm_sec = 0;
	tm->tm_mday = -1;
	tm->tm_mon = -1;
	tm->tm_year = -1;
	tm->tm_yday = -1;
	tm->tm_isdst = -1;

	dev_dbg(&client->dev, "%s: tm is mins=%d, hours=%d, wday=%d\n",
			__FUNCTION__, tm->tm_min, tm->tm_hour, tm->tm_wday);
out:
//mutex_unlock(&s35390a->mutex);
	return ret;
}

static int s35390a_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client	*client = to_i2c_client(dev);
	struct s35390a		*s35390a = i2c_get_clientdata(client);
	int	i;
	int	ret = 0;
	unsigned char		buf[3];
	struct rtc_time		*tm = &alrm->time;

	if (tm == NULL)
		return -1;

	dev_dbg(&client->dev, "%s: tm is mins=%d, hours=%d, wday=%d\n",
			__FUNCTION__, tm->tm_min, tm->tm_hour, tm->tm_wday);

//mutex_lock(&s35390a->mutex);
	/* disable week data */
	buf[0] = 0;

	// Fix the (>12:00)Alarm only 24Hour AM/PM.
	if(tm->tm_hour <12)
		buf[1] = s35390a_hr2reg(s35390a, tm->tm_hour);
	else	
		buf[1] = s35390a_hr2reg(s35390a, tm->tm_hour) | 0x40 ;

	//buf[1] = s35390a_hr2reg(s35390a, tm->tm_hour);
	buf[2] = BIN2BCD(tm->tm_min);

	/* This chip expects the bits of each byte to be in reverse order */
	for (i = 1; i < 3; ++i)
		buf[i] = bitrev8(buf[i]);

	buf[1] |= S35390A_FLAG_A12HE;
	buf[2] |= S35390A_FLAG_A12ME;

	ret = s35390a_set_reg(s35390a, S35390A_CMD_INTR1, buf, sizeof(buf));
	ret = s35390a_set_reg(s35390a, S35390A_CMD_INTR2, buf, sizeof(buf));
//mutex_unlock(&s35390a->mutex);

	return ret;
}

#if 0
static int s35390a_access_intr1(struct s35390a *s35390a)
{
	char buf[3];

	s35390a_get_reg(s35390a, S35390A_CMD_INTR1, buf, sizeof(buf));

	buf[0] &= ~S35390A_FLAG_A12WE;
	buf[1] |= S35390A_FLAG_A12HE;
	buf[2] |= S35390A_FLAG_A12ME;

	return s35390a_set_reg(s35390a, S35390A_CMD_INTR1, buf, sizeof(buf));
}
#endif

static void s35390a_work1(struct work_struct *_s35390a)
{
	char buf[1];
	struct s35390a *s35390a =
			container_of(_s35390a, struct s35390a, work1);
	struct s35390a_platform_data * sdata = s35390a->client[0]->dev.platform_data;

	printk("\ninfo: -------- alarm1!\n");
	s35390a_disable_alarm1(s35390a);

	/* FIXME: touch the reg, clear int status. unsure */
	s35390a_get_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf));

	local_irq_disable();
	rtc_update_irq(s35390a->rtc, 1, RTC_AF | RTC_IRQF);
	local_irq_enable();

	s35390a_enable_alarm1(s35390a);
	enable_irq(sdata->irq1);
}

static irqreturn_t s35390a_irq1(int irq, void *_s35390a)
{
	struct s35390a *s35390a = _s35390a;

	disable_irq_nosync(irq);
	(void)schedule_work(&s35390a->work1);

	return IRQ_HANDLED;
}

static void s35390a_work2(struct work_struct *_s35390a)
{
	char buf[1];
	struct s35390a *s35390a =
			container_of(_s35390a, struct s35390a, work2);
	struct s35390a_platform_data * sdata = s35390a->client[0]->dev.platform_data;

	printk("\ninfo: -------- alarm2!\n");
	s35390a_disable_alarm2(s35390a);

	/* FIXME: touch the reg, clear int status. unsure */
	s35390a_get_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf));

	local_irq_disable();
	rtc_update_irq(s35390a->rtc, 1, RTC_AF | RTC_IRQF);
	local_irq_enable();

	s35390a_enable_alarm2(s35390a);
	enable_irq(sdata->irq2);
}

static irqreturn_t s35390a_irq2(int irq, void *_s35390a)
{
	struct s35390a *s35390a = _s35390a;

	disable_irq_nosync(irq);
	(void)schedule_work(&s35390a->work2);

	return IRQ_HANDLED;
}

static const struct rtc_class_ops s35390a_rtc_ops = {
	.ioctl		= s35390a_rtc_ioctl,
	.read_time	= s35390a_rtc_read_time,
	.set_time	= s35390a_rtc_set_time,
	.read_alarm	= s35390a_rtc_read_alarm,
	.set_alarm	= s35390a_rtc_set_alarm,
};

static struct i2c_driver s35390a_driver;

static int s35390a_probe(struct i2c_client *client)
{
	int err, ret1, ret2;
	unsigned int i;
	struct s35390a *s35390a;
	struct rtc_time tm;
	char buf[1];
	struct s35390a_platform_data *sdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit;
	}

	s35390a = kzalloc(sizeof(struct s35390a), GFP_KERNEL);
	if (!s35390a) {
		err = -ENOMEM;
		goto exit;
	}

	s35390a->client[0] = client;
	i2c_set_clientdata(client, s35390a);

	/* This chip uses multiple addresses, use dummy devices for them */
	for (i = 1; i < 8; ++i) {
		s35390a->client[i] = i2c_new_dummy(client->adapter,
					client->addr + i, "rtc-s35390a");
		if (!s35390a->client[i]) {
			dev_err(&client->dev, "Address %02x unavailable\n",
						client->addr + i);
			err = -EBUSY;
			goto exit_dummy;
		}
	}

	err = s35390a_reset(s35390a);
	if (err < 0) {
		dev_err(&client->dev, "error resetting chip\n");
		goto exit_dummy;
	}

//	/* FIXME: the first r/w intr1 looks like improper, bypass */
//	s35390a_access_intr1(s35390a);

	err = s35390a_disable_test_mode(s35390a);
	if (err < 0) {
		dev_err(&client->dev, "error disabling test mode\n");
		goto exit_dummy;
	}

	err = s35390a_get_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf));
	if (err < 0) {
		dev_err(&client->dev, "error checking 12/24 hour mode\n");
		goto exit_dummy;
	}
	if (buf[0] & S35390A_FLAG_24H)
		s35390a->twentyfourhour = 1;
	else
		s35390a->twentyfourhour = 0;

	if (s35390a_get_datetime(client, &tm) < 0)
		dev_warn(&client->dev, "clock needs to be set\n");


	INIT_WORK(&s35390a->work1, s35390a_work1);
	INIT_WORK(&s35390a->work2, s35390a_work2);
//	mutex_init(&s35390a->mutex);

	if (sdata != NULL) {
		if (sdata->irq1 > 0) {
			err = request_irq(sdata->irq1, s35390a_irq1, IRQF_DISABLED | IRQF_TRIGGER_FALLING,
					s35390a_driver.driver.name, s35390a);
			if (err) {
				dev_dbg(&client->dev,  "can't get IRQ %d, err %d\n",
						 sdata->irq1, err);
				goto exit_dummy;
			}
		}

		if (sdata->irq2 > 0) {
			err = request_irq(sdata->irq2, s35390a_irq2, IRQF_DISABLED | IRQF_TRIGGER_FALLING,
					s35390a_driver.driver.name, s35390a);
			if (err) {
				dev_dbg(&client->dev,  "can't get IRQ %d, err %d\n",
						sdata->irq2, err);
				goto exit_irq1;
			}
		}
	}

	s35390a->rtc = rtc_device_register(s35390a_driver.driver.name,
				&client->dev, &s35390a_rtc_ops, THIS_MODULE);

	if (IS_ERR(s35390a->rtc)) {
		err = PTR_ERR(s35390a->rtc);
		goto exit_irq2;
	}

	ret1 = s35390a_disable_alarm1(s35390a);
	ret2 = s35390a_disable_alarm2(s35390a);

	/* FIXME: touch the reg, clear int status. unsure */
	s35390a_get_reg(s35390a, S35390A_CMD_STATUS1, buf, sizeof(buf));

	if (ret1 == 0 || ret2 == 0) {
		s35390a_enable_alarm1(s35390a);
		s35390a_enable_alarm2(s35390a);
	}

	return 0;

exit_irq2:
	free_irq(sdata->irq1, s35390a);

exit_irq1:
	free_irq(sdata->irq2, s35390a);
	flush_scheduled_work();

exit_dummy:
	for (i = 1; i < 8; ++i)
		if (s35390a->client[i])
			i2c_unregister_device(s35390a->client[i]);
	kfree(s35390a);
	i2c_set_clientdata(client, NULL);

exit:
	return err;
}

static int s35390a_remove(struct i2c_client *client)
{
	unsigned int i;

	struct s35390a *s35390a = i2c_get_clientdata(client);
	struct s35390a_platform_data *sdata = client->dev.platform_data;

	for (i = 1; i < 8; ++i)
		if (s35390a->client[i])
			i2c_unregister_device(s35390a->client[i]);

	if (sdata != NULL) {
		if (sdata->irq1 > 0)
			free_irq(sdata->irq1, s35390a);
		if (sdata->irq2 > 0)
			free_irq(sdata->irq2, s35390a);
	}

	rtc_device_unregister(s35390a->rtc);
	i2c_set_clientdata(client, NULL);
	kfree(s35390a);
	return 0;
}

static struct i2c_driver s35390a_driver = {
	.driver		= {
		.name	= "rtc-s35390a",
	},
	.probe		= s35390a_probe,
	.remove		= s35390a_remove,
};

static int __init s35390a_rtc_init(void)
{
	return i2c_add_driver(&s35390a_driver);
}

static void __exit s35390a_rtc_exit(void)
{
	i2c_del_driver(&s35390a_driver);
}

MODULE_AUTHOR("Byron Bradley <byron.bbradley@gmail.com>");
MODULE_DESCRIPTION("S35390A RTC driver");
MODULE_LICENSE("GPL");

module_init(s35390a_rtc_init);
module_exit(s35390a_rtc_exit);
