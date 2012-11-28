#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sysfs.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/gpio.h>


#define GPIO_LED2 GPIO_NUM(5,11)   // port, pin PF11
#define GPIO_LED3 GPIO_NUM(5,22)   // port, pin PF22

struct timer_list sslled_timer;
static 	uint32_t sslled_vaddr;

static void sslled_timer_handle(unsigned long arg)
{
	*(volatile uint32_t *)(sslled_vaddr | 0x03C) ^= (1<<23);
	mod_timer(&sslled_timer, jiffies + HZ);
}

static ssize_t sslled_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int value = ((*(volatile uint32_t *)(sslled_vaddr|0x03C))&(1<<23))>>23;
	return sprintf(buf, "%u\n", value);
}

static ssize_t sslled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int value,i;
	char *endp;
	i = simple_strtol(buf, &endp, 10);
	switch(i)
	{
	case -1: 
		del_timer(&sslled_timer);
		*(volatile uint32_t *)(sslled_vaddr | 0x03C) &= ~(1<<23);
		break;
	case 1:
		mod_timer(&sslled_timer, jiffies + HZ);
		break;
	case 2:
		gpio_direction_output(GPIO_LED2, 1);
		break;
	case -2:
		gpio_direction_output(GPIO_LED2, 0);
		break;
	case 3:
		gpio_direction_output(GPIO_LED3, 1);
		break;
	case -3:
		gpio_direction_output(GPIO_LED3, 0);
		break;
	default:
		break;
	}
	/*if (i) {
		del_timer(&sslled_timer);
		*(volatile uint32_t *)(sslled_vaddr | 0x03C) &= ~(1<<23);
	} else 
		mod_timer(&sslled_timer, jiffies + HZ);
	*/
	return count; 
}

static DEVICE_ATTR(ledctl, 0664, sslled_show, sslled_store);

static struct attribute *sslled_attributes[] = {
	&dev_attr_ledctl.attr, NULL,
};

static struct attribute_group sslled_attr_group = {
	.attrs = sslled_attributes,
};

static int __devinit sslled_probe(struct platform_device *pdev)
{
	int err = sysfs_create_group(&pdev->dev.kobj, &sslled_attr_group);
	if(err)
		printk("register sysfs for sslled is failure.\n");
#if 0
	init_timer(&sslled_timer);
	sslled_timer.function = &sslled_timer_handle;
	sslled_timer.expires = jiffies + HZ;
	add_timer(&sslled_timer);
#endif
	return 0;
}

static int __devexit sslled_remove(struct platform_device *pdev)
{
//	del_timer(&sslled_timer);
	sysfs_remove_group(&pdev->dev.kobj, &sslled_attr_group);
	return 0;
}

static struct platform_driver sslled_device_driver = {
	.probe		= sslled_probe,
	.remove		= __devexit_p(sslled_remove),
	.driver		= {
		.name	= "ssl_led",
		.owner 	= THIS_MODULE,
	},
};

static int __init sslled_init(void)
{
	sslled_vaddr = (uint32_t)ioremap_nocache(0x0810f500, 0x1000);
	*(volatile uint32_t *)(sslled_vaddr | 0x018) |= 0x800000;
	*(volatile uint32_t *)(sslled_vaddr | 0x020) |= 0x800000;
       
	init_timer(&sslled_timer);
	sslled_timer.function = &sslled_timer_handle;
	sslled_timer.expires = jiffies + HZ;
	add_timer(&sslled_timer);

	return platform_driver_register(&sslled_device_driver);
}

static void __exit sslled_exit(void)
{
	del_timer(&sslled_timer);
	iounmap(sslled_vaddr);
	platform_driver_unregister(&sslled_device_driver);
}

module_init(sslled_init);
module_exit(sslled_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("blue led driver for Acccio");
