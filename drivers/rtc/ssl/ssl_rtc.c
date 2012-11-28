/*
To Do:
Always count to leap year Feb 29 - to correct it everyday. rtc.c - 
#define ADJUST_LEAP_YEAR
IRQ last for 1 sec: so now no periodic alarm feature - the alarm is disabled 
in the first irq; user has to re-enable it manually for next alarm

Latest driver for MAGUS Real Time Clock (RTC) unit (Linux 2.6.20.1)
Must Read!!!
This is the low-level rtc driver interfacing with the 
middle layer: drivers/rtc/class.c
drivers/rtc/interface.c
drivers/rtc/rtc-dev.c
drivers/rtc/rtc-lib.c

This file is created based on example in drivers/rtc/rtc-s3c.c
This driver still uses the common Linux Header files for all systems:
#include <linux/rtc.h> 
and the header file just for ARM system:
#include <asm/rtc.h>

Author: Shao Wei
Credit to: (linux 2.6.20)
drivers/rtc/rtc-s3c.c
drivers/rtc/v41xx.c

linux/arch/arm/mach-integrator/time.c

Remarks
System struct tm: 
tm_year = True_yar - 1900;
tm_mon = True_month - 1;
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>	// check kernel version
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/delay.h>


#if IO_MAP == 3
#warning "using ebm interface"
#include "ebm_mem.h"
#include "io.h"
#else
#include <asm/io.h>
#endif

#if INTC == 1
#include "intck.h"
#define request_irq	intc_req
#define free_irq	intc_free
#define enable_irq	intc_enable
#define disable_irq	intc_disable
#endif

#define tm	rtc_time
#include "rtc.h"

#define DRV_NAME "rtc"

/* Set Stop-watch countdown value */
//#define RTC_SET_STOPWATCH _IOW('p', 0x13, unsigned in)


static uint32_t	clk = 1 << 15;	/* reference crystal frquency */
module_param(clk, uint, 0);

typedef struct
{
	rtc_t			hw;
	spinlock_t		lock;
	int				use;
	int				freq;			/* selected PST = 2 << freq Hz */
	struct rtc_device	*rtc_dev;
}
sslrtc;


static irqreturn_t sslrtc_interrupt(int irq, void *dev_id)
{
	int		ret, events;
	sslrtc	*t = dev_id;
	unsigned long	flags;

	spin_lock_irqsave(&t->lock, flags);

	ret = rtc_isr(&t->hw);
//printk("sslrtc: evt info - %X\n", ret);

	events  = 0;
	if (ret & RTC_INT_ALARM)
	{
		// turn off alarm interrupt, otherwise will re-enter irq within one sec
		events |= RTC_AF | RTC_IRQF;
	}
	if (ret & RTC_INT_SEC)
	{
		events |= RTC_UF | RTC_IRQF;
	}
	if (ret & (RTC_INT_2HZ | RTC_INT_PSTALL))
	{
		events |= RTC_PF | RTC_IRQF;
	}
	if (events)
	{
		rtc_update_irq(t->rtc_dev, 1, events);
	}

	spin_unlock_irqrestore(&t->lock, flags);
	return ret ? IRQ_HANDLED : IRQ_NONE;
}


/* Read/write time */

/*
 * Set the RTC time. Unfortunately, we can't accurately set
 * the point at which the counter updates.
 *
 * Also, since RTC_LR is transferred to RTC_CR on next rising
 * edge of the 1Hz clock, we must write the time one second
 * in advance.
 */
static int sslrtc_set_time(struct device *dev, struct rtc_time *_tm)
{
	int			ret;
	struct tm	time;
	sslrtc		*rtc;

	memcpy(&time, _tm, sizeof(struct tm));
	time.tm_year -= 70;
	time.tm_mon++;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	ret = rtc_settime(&rtc->hw, &time);
	spin_unlock_irq(&rtc->lock);
	return ret;
}


static int sslrtc_get_time(struct device *dev, struct rtc_time *_tm)
{
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	rtc_gettime(&rtc->hw, _tm);
	spin_unlock_irq(&rtc->lock);
	_tm->tm_mon--;
	_tm->tm_year += 70;
	return 0;
}


static int sslrtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	int		ret;
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	ret = rtc_alarm_set(&rtc->hw, &alrm->time, alrm->enabled);
	spin_unlock_irq(&rtc->lock);
	return ret;
}


static int sslrtc_get_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	sslrtc	*rtc;
	int		rt;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	rt = rtc_alarm_get(&rtc->hw, &alrm->time);
	alrm->enabled = rt & 1;
	alrm->pending = !!(rt & 2);
	spin_unlock_irq(&rtc->lock);
	return 0;
}


int sslrtc_set_event(struct device *dev, struct rtc_wkalrm *event)
{
	int			ret;
	struct tm	time;
	sslrtc		*rtc;

	rtc = dev_get_drvdata(dev);
	memcpy(&time, &event->time, sizeof(time));
	time.tm_mon++;
	spin_lock_irq(&rtc->lock);
	if (rtc_event_is_on(&rtc->hw))
	{
		rtc_event_off(&rtc->hw);
	}
	rtc_event_clear(&rtc->hw);
	ret = rtc_set_event(&rtc->hw, &time);
	if (!ret)
	{
		if (event->enabled)
		{
			rtc_event_on(&rtc->hw);
		}
	}

	spin_unlock_irq(&rtc->lock);
	return ret;
}
EXPORT_SYMBOL(sslrtc_set_event);


int sslrtc_get_event(struct device *dev, struct rtc_wkalrm *event)
{
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	if (rtc_event_is_on(&rtc->hw))
	{
		rtc_get_event(&rtc->hw, &event->time);
		event->time.tm_mon--;
		event->time.tm_year = 0;
	}
	else
	{
		memset(&event->time, 0xff, sizeof(struct rtc_time));
	}
	spin_unlock_irq(&rtc->lock);
	return 0;
}
EXPORT_SYMBOL(sslrtc_get_event);


int sslrtc_set_stopwatch(struct device *dev, unsigned int mins)
{
	int		ret;
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	ret = rtc_set_stopwatch(&rtc->hw, mins);
	spin_unlock_irq(&rtc->lock);
	if (ret)
	{
		ret = -EINVAL;
	}
	return ret;
}

EXPORT_SYMBOL(sslrtc_set_stopwatch);


static int sslrtc_ioctl(struct device *dev,
			 unsigned int cmd, unsigned long arg)
{
	sslrtc	*rtc;

//printk("sslrtc: ioctl info - cmd=%X\n", cmd);
	rtc = dev_get_drvdata(dev);
	switch (cmd)
	{
		case RTC_AIE_OFF:
			rtc_alarm_set(&rtc->hw, 0, 0);
			return 0;

		case RTC_AIE_ON:
			rtc_alarm_set(&rtc->hw, 0, 1);
			return 0;

		case RTC_PIE_OFF:
			rtc_pst_off(&rtc->hw);
			return 0;

		case RTC_PIE_ON:
			rtc_pst_clear(&rtc->hw);
			if (!rtc->freq)
			{
				rtc_set_intr(&rtc->hw, RTC_INT_2HZ, 1);
			}
			else
			{
				rtc_pst_on(&rtc->hw, rtc->freq);
			}
			return 0;

		case RTC_UIE_ON:
			rtc_sec_on(&rtc->hw);
			return 0;

		case RTC_UIE_OFF:
			rtc_sec_off(&rtc->hw);
			return 0;

		case RTC_IRQP_READ:
			return put_user((2 << rtc->freq), (unsigned long *)arg);

		case RTC_IRQP_SET:
		{
			int	fq;

			for (fq = 0; fq < 9; fq++)
			{
				if (arg == (2 << fq))
				{
					rtc->freq = fq;
					return 0;
				}
			}
			return -EINVAL;
		}
	}
	return -ENOIOCTLCMD;
}


static int sslrtc_proc(struct device *dev, struct seq_file *seq)
{
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	seq_printf(seq, "alarm_IRQ\t: %s\n",
		   rtc_alarm_is_on(&rtc->hw) ? "yes" : "no");
	seq_printf(seq, "calender_IRQ\t: %s\n", 
		   rtc_event_is_on(&rtc->hw) ? "yes" : "no");
	seq_printf(seq, "sec_IRQ\t: %s\n", 
		   rtc_sec_is_on(&rtc->hw) ? "yes" : "no");
	return 0;
}



static int sslrtc_open(struct device *dev)
{
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	if (rtc->use) 
	{
		spin_unlock_irq(&rtc->lock);
		return -EBUSY;
	}
	rtc->use = 1;
	spin_unlock_irq(&rtc->lock);
	return 0;
}


static void sslrtc_release(struct device *dev)
{
	sslrtc	*rtc;

	rtc = dev_get_drvdata(dev);
	spin_lock_irq(&rtc->lock);
	rtc->use = 0;
	spin_unlock_irq(&rtc->lock);
}


static const struct rtc_class_ops sslrtc_ops =
{
	.open		= sslrtc_open,
	.release	= sslrtc_release,
	.ioctl		= sslrtc_ioctl,
	.read_time	= sslrtc_get_time,
	.set_time	= sslrtc_set_time,
	.read_alarm	= sslrtc_get_alarm,
	.set_alarm	= sslrtc_set_alarm,
	.proc	    = sslrtc_proc,
};


static int __devinit sslrtc_probe(struct platform_device *pdev)
{
	struct rtc_device	*rtc_dev;
	struct resource	*rc;
	sslrtc			*rtc;
	uint32_t		vaddr;
	int				irq;
	int				ret;

	rtc = kzalloc(sizeof(sslrtc), GFP_KERNEL);
	if (!rtc)
	{
		printk("sslrtc: probe err - kmalloc\n");
		return -ENOMEM;
	}

	irq = platform_get_irq(pdev, 0);
	rc = platform_get_resource(pdev, IORESOURCE_MEM, 0);

/*
	if (!request_mem_region(start, 
		end - start + 1, 
		pdev->name))
	{
		printk("sslrtc: init err - req_mem_rgn\n");
		ret = -EBUSY;
		goto l_free;
	}
printk("request_mem_region\n");
*/

#if IO_MAP == 1
	vaddr = (uint32_t)ioremap_nocache(rc->start, rc->end - rc->start + 1);
	if (!vaddr)
	{
		printk("sslrtc: probe err - remap\n");
		ret = -EBUSY;
		goto l_free;
	}
	rtc->hw.r = (void *)vaddr;
#else
	rtc->hw.r = (void *)start;
#endif
	ret = request_irq(irq, sslrtc_interrupt, IRQF_DISABLED, DRV_NAME, rtc);
	if (ret)
	{
		printk(KERN_ERR "IRQ%d already in use\n", irq);
		goto l_unmap;
	}

	rtc->hw.clk = clk;

	spin_lock_init(&rtc->lock);
	spin_lock_irq(&rtc->lock);
	ret = rtc_init(&rtc->hw);
	spin_unlock_irq(&rtc->lock);
	if (ret)
	{
		printk("sslrtc: probe err - rtc_init\n");
		ret = -ENODEV;
		goto l_firq;
	}
//	rtc_enable(&rtc->hw, 1);

	rtc->use = 0;

	rtc_dev = rtc_device_register(DRV_NAME, &pdev->dev, &sslrtc_ops,
				 THIS_MODULE);
	if (IS_ERR(rtc_dev)) 
	{
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(rtc_dev);
		goto l_err;
	}
	rtc_dev->max_user_freq = 128;
	rtc->rtc_dev = rtc_dev;
	platform_set_drvdata(pdev, rtc);
	return 0;

l_err:
	rtc_exit(&rtc->hw);
l_firq:
	free_irq(irq, &rtc);
l_unmap:
#if IO_MAP == 1
	iounmap((void*)vaddr);
//l_mem:
#endif
//	release_mem_region(start, end - start + 1);
l_free:
	kfree(rtc);
	dev_err(&pdev->dev, "probe err - %d\n", ret);
	return ret;
}


static int sslrtc_remove(struct platform_device *dev)
{
	sslrtc *rtc;

	rtc = platform_get_drvdata(dev);
	platform_set_drvdata(dev, 0);
	rtc_device_unregister(rtc->rtc_dev);
	rtc_exit(&rtc->hw);
	free_irq(platform_get_irq(dev, 0), rtc);
#if IO_MAP == 1
	iounmap(rtc->hw.r);
#endif
//	release_mem_region((uint32_t)r->hw.r, 0x48);
	kfree(rtc);
	return 0;
}



static struct platform_driver sslrtc_driver = 
{
	.probe		= sslrtc_probe,
	.remove		= sslrtc_remove,	// __devexit_p(sslrtc_remove)
	.driver		= 
	{
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},

};


static int __init sslrtc_init(void)
{
	return  platform_driver_register(&sslrtc_driver);
}


static void __exit sslrtc_exit(void)
{
	platform_driver_unregister(&sslrtc_driver);
}


module_init(sslrtc_init);
module_exit(sslrtc_exit);


MODULE_DESCRIPTION(DRV_NAME);
MODULE_AUTHOR("Shao Wei, Solomon Systech Ltd");
MODULE_LICENSE("GPL");
