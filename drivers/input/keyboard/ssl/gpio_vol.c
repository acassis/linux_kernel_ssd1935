#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/irq.h>
#include "gpio_vol.h"

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <asm/hardware.h>

#define GPIOVOLMAJOR	240

struct gpiovol_dev {                                                        
	struct cdev cdev;                         
	struct fasync_struct *async_queue;
};

struct gpiovol_dev *gpiovol_devp;
struct work_struct detect_worker_tmp; 
volatile struct key_detect key_status;

static int gpiovol_major = GPIOVOLMAJOR;
static int gpio[2]= {
	MAGUS_GPIO_VOL_UP,MAGUS_GPIO_VOL_DOWN,
};
static int gvalue;

static int gpiovol_fasync(int fd, struct file *filp, int mode)
{
        struct gpiovol_dev *dev = filp->private_data;
        return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static void detect_worker_handle(void)
{
	int  gpio_val;
	struct gpiovol_dev *dev = gpiovol_devp;
	msleep(300);
	switch(key_status.status) {
		case UPX:
			gpio_val = gpio_get_value(gpio[0]);
			gvalue = 5;
			if (dev->async_queue)
				kill_fasync(&dev->async_queue, SIGIO, POLL_IN);	
			if (gpio_val == 0) {
				enable_irq(key_status.irq);
				return;
			}
			key_status.status = UP;
			schedule_work(&detect_worker_tmp);		
			break;

		case DOWNX:
			gpio_val = gpio_get_value(gpio[1]);
			gvalue = -5;
			if (dev->async_queue)
				kill_fasync(&dev->async_queue, SIGIO, POLL_IN);			
			if(gpio_val == 0) {
				enable_irq(key_status.irq);
				return;
			}
			key_status.status = DOWN;		
			schedule_work(&detect_worker_tmp);
			break;

		case UP:
			gpio_val = gpio_get_value(gpio[0]);
			gvalue = 5;
			if (dev->async_queue)
				kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			if (gpio_val == 0) {
				enable_irq(key_status.irq);
				return;
			}			
			key_status.status = UP;
			schedule_work(&detect_worker_tmp);
			break;
			
		case DOWN:
			gpio_val = gpio_get_value(gpio[1]);
			gvalue = -5;
			if (dev->async_queue) 
				kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			if (gpio_val == 0) {
				enable_irq(key_status.irq);
				return;
			}	
			key_status.status = DOWN;
			schedule_work(&detect_worker_tmp);
			break;
	}
}

static irqreturn_t gpio_vol_isr(int irq, void *dev_id)
{
	int i,gpio_val;
	mdelay(5);
 	for (i=0;i<2;i++) {
		gpio_val = gpio_get_value(gpio[i]);
		if (gpio_val) {
			if (irq == gpio_to_irq(gpio[i])) {
				disable_irq(gpio_to_irq(irq));
				if(i == 0) {
					key_status.status = UPX;
					key_status.irq = irq;
				}
				if(i == 1) {
					key_status.status = DOWNX;
					key_status.irq = irq;
				}
				schedule_work(&detect_worker_tmp);
			}
		}
	}
	return IRQ_HANDLED;
			
}
static int gpiovol_release(struct inode *inode, struct file *filp)
{	
	gpiovol_fasync(-1, filp, 0);
	return 0;
}

static int gpiovol_open(struct inode *inode, struct file *filp)
{
	filp->private_data = gpiovol_devp;
	return 0;
}

static int gpiovol_ioctl(struct inode *inode,struct file *filp,unsigned
	int cmd, unsigned long arg)
{
	int value = gvalue;
	switch(cmd) {
		case GPIOVOL_GET:
			put_user(value,(int __user *)arg);
			break;
		default:
			return -ENOTTY;
	}
	return 0;
}

static int gpio_vol_suspend(struct platform_device *dev, pm_message_t state);
static int gpio_vol_resume(struct platform_device *dev);
static ssize_t gpio_vol_show(struct device *dev,
							   struct device_attribute *attr, char *buf) 
{
	return sprintf(buf, "%d\n", dev->power.power_state.event);
}

static ssize_t gpio_vol_store(struct device *dev,
							   struct device_attribute *attr,
							   const char *buf, size_t count) 
{
	char *endp;
	int i;
	struct platform_device *pdev;
	pdev = container_of(dev, struct platform_device, dev);

	i = simple_strtol(buf, &endp, 10);

	if (i == dev->power.power_state.event)
	{
		printk("the same power state. \n");
		return count;
	}
	dev->power.power_state.event = i;
	if (i == PM_EVENT_SUSPEND)
		
	{
		gpio_vol_suspend(pdev, PMSG_SUSPEND);
	}
	else if (i == PM_EVENT_ON)
	{
		gpio_vol_resume(pdev);
	}
	else
	{
		dev->power.power_state.event = 0;
	}
	return count;
}

static DEVICE_ATTR(power, 0664, gpio_vol_show, gpio_vol_store);

static struct attribute *gpio_vol_attributes[] =
{ 
	&dev_attr_power.attr, NULL, 
};

static struct attribute_group gpio_vol_attr_group = { 
	.attrs = gpio_vol_attributes, 
};

static int gpio_vol_suspend(struct platform_device *pdev, pm_message_t state) 
{
#if 0
	int i;
	gpio_vol_data * pdata = pdev->dev.platform_data;
	for (i = 0; i < 2; i++)
		
	{
		printk(KERN_ERR"suspend gpio.....\n");
		t irq = gpio_to_irq(pdata->gpio[i]);
		disable_irq(irq);
	} 
#endif
return 0;
}

static int gpio_vol_resume(struct platform_device *pdev)
{
#if 0
	int i;
	gpio_vol_data * pdata = pdev->dev.platform_data;
	for (i = 0; i < 2; i++)
		
	{
		printk(KERN_ERR"resume gpio....\n");
		int irq = gpio_to_irq(pdata->gpio[i]);
		enable_irq(irq);
	}
#endif
 return 0;
}


static struct file_operations gpiovol_fops = {
	.owner 		= THIS_MODULE,
	.ioctl		= gpiovol_ioctl,
	.fasync		= gpiovol_fasync,
	.open 		= gpiovol_open,
	.release 		= gpiovol_release,
};

static void gpiovol_setup_cdev(struct gpiovol_dev *dev,int index)
{
	int devno=MKDEV(gpiovol_major,index);
	cdev_init(&dev->cdev, &gpiovol_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops	= &gpiovol_fops;
	cdev_add(&dev->cdev, devno, 1);
}

static void register_gpiovol(void)
{
	dev_t devno = MKDEV(gpiovol_major, 0);
	register_chrdev_region(devno, 1, "gpio_vol");
	gpiovol_devp = kmalloc(sizeof(struct gpiovol_dev),GFP_KERNEL);
	memset(gpiovol_devp, 0, sizeof(struct gpiovol_dev));
	gpiovol_setup_cdev(gpiovol_devp, 0);
}

static int __devinit gpio_vol_probe(struct platform_device *pdev)
{
	gpio_vol_data *pdata = pdev->dev.platform_data;
	int error,i;
	
	INIT_WORK(&detect_worker_tmp,detect_worker_handle);
	register_gpiovol();
	for (i=0;i<2;i++) {
		int irq = gpio_to_irq(pdata->gpio[i]);
		set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
		error = request_irq(irq, gpio_vol_isr, IRQF_SAMPLE_RANDOM,
				     pdata->desc ? pdata->desc : "gpio_vol",
				     pdev);
		if (error) {
			printk(KERN_ERR "gpio_vol: unable to claim irq %d; error %d\n",
				irq, error);
			goto fail;
		}
	}
	return 0;

 fail:
	for (i=0;i<2;i++) {
		free_irq(gpio_to_irq(pdata->gpio[i]), pdev);
	}
	return error;
}

static int __devexit gpio_vol_remove(struct platform_device *pdev)
{
	gpio_vol_data *pdata = pdev->dev.platform_data;
	int i;
	
	for (i=0;i<2;i++) {
		int irq = gpio_to_irq(pdata->gpio[i]);
		free_irq(irq, pdev);
	}	
	return 0;
}

struct platform_driver gpio_vol_device_driver = {
	.probe		= gpio_vol_probe,
	.remove		= __devexit_p(gpio_vol_remove),
	.driver		= {
		.name	= "gpio_vol",
	}
};

static int __init gpio_vol_init(void)
{
	return platform_driver_register(&gpio_vol_device_driver);
}

static void __exit gpio_vol_exit(void)
{
	platform_driver_unregister(&gpio_vol_device_driver);
}

module_init(gpio_vol_init);
module_exit(gpio_vol_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("williamzhou <williamzhou@solomon-systech.com>");
MODULE_DESCRIPTION("gpio_vol driver for MAGUS");
