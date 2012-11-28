/* linux/drivers/char/watchdog/magus_wdt.c
 *
 * Copyright (c) 2008 Solomon Systech 
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Based on, softdog.c by Alan Cox,
 *     (c) Copyright 1996 Alan Cox <alan@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Changelog:
 *	03-July-2008 Initial	
 *
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include "io.h"
#include "magus_wdt.h"

#define PFX "magus-wdt: "


#define CONFIG_MAGUS_WATCHDOG_ATBOOT		(0)
#define CONFIG_MAGUS_WATCHDOG_DEFAULT_TIME	(70)
//#define CONFIG_MAGUS_WATCHDOG_DEFAULT_COUNT (20)

static int nowayout	= WATCHDOG_NOWAYOUT;
static int tmr_margin	= CONFIG_MAGUS_WATCHDOG_DEFAULT_TIME;
static int tmr_atboot	= CONFIG_MAGUS_WATCHDOG_ATBOOT;
static int soft_noboot	= 0;
static int debug	= 1;

module_param(tmr_margin,  int, 0);
module_param(tmr_atboot,  int, 0);
module_param(nowayout,    int, 0);
module_param(soft_noboot, int, 0);
module_param(debug,	  int, 0);

MODULE_PARM_DESC(tmr_margin, "Watchdog tmr_margin in seconds. default=" __MODULE_STRING(CONFIG_MAGUS_WATCHDOG_DEFAULT_TIME) ")");

MODULE_PARM_DESC(tmr_atboot, "Watchdog is started at boot time if set to 1, default=" __MODULE_STRING(CONFIG_MAGUS_WATCHDOG_ATBOOT));

MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_PARM_DESC(soft_noboot, "Watchdog action, set to 1 to ignore reboots, 0 to reboot (default depends on ONLY_TESTING)");

MODULE_PARM_DESC(debug, "Watchdog debug, set to >1 for debug, (default 0)");


typedef enum close_state {
	CLOSE_STATE_NOT,
	CLOSE_STATE_ALLOW=0x4021
} close_state_t;

static DECLARE_MUTEX(open_lock);

static struct device    *wdt_dev;	/* platform device attached to */
static struct resource	*wdt_mem;
static struct resource	*wdt_irq;
static struct clk	*wdt_clock;
static void __iomem	*wdt_base;
static unsigned int	 wdt_count; //= CONFIG_MAGUS_WATCHDOG_DEFAULT_COUNT;
static close_state_t	 allow_close;

/* watchdog control routines */
#if 0
#define DBG(msg...) do { \
	if (debug) \
		printk(KERN_INFO msg); \
	} while(0)
#else
#define DBG(msg...) 
#endif

/* functions */
/*
 *	return 1< enable; 0< disable
 * */
static int maguswdt_state(void)
{
	unsigned long wtcon;
	wtcon = IO_RD32(wdt_base + MAGUSWDT_TCNTR);
	return wtcon&MAGUSWDT_CFGR_EN?1:0;
}

static int maguswdt_keepalive(void)
{

/* Write 0x28 followed by write 0x78 to generate the restar sequence.*/
	IO_WR32(wdt_base + MAGUSWDT_RSTR, 0x028);	
	IO_WR32(wdt_base + MAGUSWDT_RSTR, 0x078);	
	return 0;
}

static int maguswdt_stop(void)
{
	unsigned long wtcon;

	//wtcon = readl(wdt_base + MAGUSWDT_TCNTR);
	wtcon = IO_RD32(wdt_base + MAGUSWDT_TCNTR);
	BIT_CLEAN32(wdt_base + MAGUSWDT_CFGR, BIT_MAGUSWDT_CFGR_EN);
	DBG("WDT: stop...\n");	

	return 0;
}

static int maguswdt_start(void)
{
	unsigned long wtcon;

	maguswdt_stop();
	wtcon = IO_RD32(wdt_base + MAGUSWDT_CFGR);
	wtcon |= MAGUSWDT_CFGR_EN;
	
	if (soft_noboot){
		BIT_SET32(wdt_base + MAGUSWDT_INTR, BIT_MAGUSWDT_INTR_INTEN);
		wtcon &= ~MAGUSWDT_CFGR_RSTEN;	
	} else {
		BIT_CLEAN32(wdt_base + MAGUSWDT_INTR, BIT_MAGUSWDT_INTR_INTEN);
		wtcon |= MAGUSWDT_CFGR_RSTEN;	
	}
	IO_WR32(wdt_base + MAGUSWDT_TCNTR, wdt_count);
	IO_WR32(wdt_base + MAGUSWDT_CFGR, wtcon);
	
	maguswdt_keepalive();
	DBG("WDT: starting...\n");	
	return 0;
}

static int maguswdt_set_heartbeat(int timeout)
{
	wdt_count = timeout;	
	IO_WR32(wdt_base + MAGUSWDT_TCNTR, wdt_count);
	return 0;
}

/*
 *	/dev/watchdog handling
 */

static int maguswdt_open(struct inode *inode, struct file *file)
{
	if(down_trylock(&open_lock))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	allow_close = CLOSE_STATE_NOT;

	/* start the timer */
	if(! maguswdt_start()){
		printk("MAGUS Watchdog start...\n");
	}
	return nonseekable_open(inode, file);
}

static int maguswdt_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we set nowayout
	 */

	if (allow_close == CLOSE_STATE_ALLOW) {
		maguswdt_stop();
	} else {
		dev_err(wdt_dev, "Unexpected close, not stopping watchdog\n");
		maguswdt_keepalive();
	}

	allow_close = CLOSE_STATE_NOT;
	up(&open_lock);
	return 0;
}

static ssize_t maguswdt_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	/*
	 *	Refresh the timer.
	 */
	if(len) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			allow_close = CLOSE_STATE_NOT;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					allow_close = CLOSE_STATE_ALLOW;
			}
		}

		maguswdt_keepalive();
	}
	return len;
}

#define OPTIONS WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE

static struct watchdog_info magus_wdt_ident = {
	.options          =     OPTIONS,
	.firmware_version =	0,
	.identity         =	"MAGUS Watchdog",
};


static int maguswdt_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_margin;
	int options;

	switch (cmd) {
		case WDIOC_GETSUPPORT:
			return copy_to_user(argp, &magus_wdt_ident,
				sizeof(magus_wdt_ident)) ? -EFAULT : 0;

		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:
			return put_user(0, p);

		case WDIOC_KEEPALIVE:
			maguswdt_keepalive();
			return 0;

		case WDIOC_SETTIMEOUT:
			if (get_user(new_margin, p))
				return -EFAULT;

			if (maguswdt_set_heartbeat(new_margin))
				return -EINVAL;

			maguswdt_keepalive();
			return put_user(tmr_margin, p);

		case WDIOC_GETTIMEOUT:
			return put_user(tmr_margin, p);
		
		case WDIOC_SETOPTIONS:
			if (get_user(options, (int *)arg)){
				printk("WDT: get_user fail.\n");
				return -EFAULT;
			}
			DBG("WDIOC_SETOPTIONS: options = 0x%x\n", options);
			if (options & WDIOS_ENABLECARD){
				allow_close = CLOSE_STATE_NOT;
				maguswdt_start();	
			}else if (options & WDIOS_DISABLECARD){
				allow_close = CLOSE_STATE_ALLOW; 
				maguswdt_stop();	
			}else
				printk("WDIOC_SETOPTIONS: options = 0x%x NOT support.\n", options);
			return 0;
		default:
			return -ENOTTY;

		}
}

/* kernel interface */

static const struct file_operations maguswdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= maguswdt_write,
	.ioctl		= maguswdt_ioctl,
	.open		= maguswdt_open,
	.release	= maguswdt_release,
};

static struct miscdevice maguswdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &maguswdt_fops,
};

/* interrupt handler code */

static irqreturn_t maguswdt_irq(int irqno, void *param)
{
	dev_info(wdt_dev, "watchdog timer expired (irq)\n");
	
	BIT_SET32(wdt_base + MAGUSWDT_STSR, BIT_MAGUSWDT_STSR_INT);
	maguswdt_keepalive();
	return IRQ_HANDLED;
}
/* device interface */

static int maguswdt_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev;
	unsigned int wtcon;
	int started = 0;
	int ret;
	int size;

	DBG("%s: probe=%p\n", __FUNCTION__, pdev);

	dev = &pdev->dev;
	wdt_dev = &pdev->dev;

	/* get the memory region for the watchdog timer */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "no memory resource specified\n");
		return -ENOENT;
	}

	size = (res->end-res->start)+1;
	//printk("WDT: men-start:0x%x  end: 0x%x\n", res->start, res->end);
	wdt_mem = request_mem_region(res->start, size, pdev->name);
	if (wdt_mem == NULL) {
		dev_err(dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req;
	}

	wdt_base = ioremap_nocache(res->start, size);
	//wdt_base = ioremap(res->start, size);
	if (wdt_base == 0) {
		dev_err(dev, "failed to ioremap() region\n");
		ret = -EINVAL;
		goto err_req;
	}


	wdt_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (wdt_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_map;
	}

	ret = request_irq(wdt_irq->start, maguswdt_irq, 0, pdev->name, pdev);
	if (ret != 0) {
		dev_err(dev, "failed to install irq (%d)\n", ret);
		goto err_map;
	}

	if (maguswdt_set_heartbeat(tmr_margin)) {
		started = maguswdt_set_heartbeat(CONFIG_MAGUS_WATCHDOG_DEFAULT_TIME);

		if (started == 0) {
			dev_info(dev,"tmr_margin value out of range, default %d used\n",
			       CONFIG_MAGUS_WATCHDOG_DEFAULT_TIME);
		} else {
			dev_info(dev, "default timer value is out of range, cannot start\n");
		}
	}

	ret = misc_register(&maguswdt_miscdev);
	if (ret) {
		dev_err(dev, "cannot register miscdev on minor=%d (%d)\n",
			WATCHDOG_MINOR, ret);
		goto err_clk;
	}

	if (tmr_atboot && started == 0) {
		dev_info(dev, "starting watchdog timer\n");
		maguswdt_start();
	} else if (!tmr_atboot) {
		/* if we're not enabling the watchdog, then ensure it is
		 * disabled if it has been left running from the bootloader
		 * or other source */

		maguswdt_stop();
	}

	/* print out a statement of readiness */

	wtcon = IO_RD32(wdt_base + MAGUSWDT_CFGR);
	
	dev_info(dev, "watchdog %sactive, reset %sabled.\n",
		 (wtcon & MAGUSWDT_CFGR_EN) ?  "" : "in",
		 (wtcon & MAGUSWDT_CFGR_RSTEN) ? "" : "dis");
	
	return 0;

 err_clk:
	//clk_disable(wdt_clock);
	//clk_put(wdt_clock);

 err_irq:
	free_irq(wdt_irq->start, pdev);

 err_map:
	iounmap(wdt_base);

 err_req:
	release_resource(wdt_mem);
	kfree(wdt_mem);

	return ret;
}

static int maguswdt_remove(struct platform_device *dev)
{
	release_resource(wdt_mem);
	kfree(wdt_mem);
	wdt_mem = NULL;

	free_irq(wdt_irq->start, dev);
	wdt_irq = NULL;

	/*
	clk_disable(wdt_clock);
	clk_put(wdt_clock);
	wdt_clock = NULL;
	*/
	iounmap(wdt_base);
	misc_deregister(&maguswdt_miscdev);
	return 0;
}

static void maguswdt_shutdown(struct platform_device *dev)
{
	DBG("WDT: shutdown...\n");	
	maguswdt_stop();	
}

#ifdef CONFIG_PM

static unsigned long wdt_state_save = 0;

static int maguswdt_suspend(struct platform_device *dev, pm_message_t state)
{
	/* Save watchdog state, and turn it off. */

	/* Note that WTCNT doesn't need to be saved. */
	DBG("WDT: suspend...\n");	
	wdt_state_save = maguswdt_state();
	maguswdt_stop();

	return 0;
}

static int maguswdt_resume(struct platform_device *dev)
{
	/* Restore watchdog state. */

	if (wdt_state_save)
		maguswdt_start();
	else
		maguswdt_stop();
	DBG("WDT: resume to %d mode\n", wdt_state_save);	
	return 0;
}

#else
#define maguswdt_suspend NULL
#define maguswdt_resume  NULL
#endif /* CONFIG_PM */


static struct platform_driver maguswdt_driver = {
	.probe		= maguswdt_probe,
	.remove		= maguswdt_remove,
	.shutdown	= maguswdt_shutdown,
	.suspend	= maguswdt_suspend,
	.resume		= maguswdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "wdog",
	},
};


static char banner[] __initdata = KERN_INFO "MAGUS Watchdog Timer, (c) 2008 Solomon Systech\n";

static int __init watchdog_init(void)
{
	printk(banner);
	return platform_driver_register(&maguswdt_driver);
}

static void __exit watchdog_exit(void)
{
	platform_driver_unregister(&maguswdt_driver);
}

module_init(watchdog_init);
module_exit(watchdog_exit);

MODULE_AUTHOR("Spiritwen@solomon-systech.com");
MODULE_DESCRIPTION("MAGUS Watchdog Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
