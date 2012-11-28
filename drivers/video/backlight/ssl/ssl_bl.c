#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>

#include <asm/arch/hardware.h>
#include "ssl_bl.h"
#include "os.h"

#define DEVCOUNT	4

static struct cdev *cdev_p;
static dev_t dev;
static int openCnt;

static int sslbl_ioctl(struct inode *inode, struct file *filp,
			     unsigned int cmd, unsigned int arg)
{	
	uint32_t ctr;
	unsigned int data;
	ctr = io_rd32(PWM_CTR);
	
	switch (cmd) 
	{
		case SSLBL_INTENSITY_SET:
		{
		    get_user(data,(int __user *)arg);
	            ctr = ctr & 0xff00ffff;
                    ctr |= data<<16;
		    io_wr32(PWM_CTR,ctr);
		    break;
		}
		case SSLBL_INTENSITY_GET:
		{
		    ctr = ctr & 0x00ff0001;
		    ctr = ctr>>16;
		    put_user(ctr,(int __user *)arg);
		    break;
		}		
		default:
		    return -ENOTTY;	
	}
	return 0;
}


static int sslbl_open(struct inode *inode, struct file *filp)
{
	int minor;
	uint32_t cfg;
	uint32_t ctr;
	minor = MINOR(inode->i_rdev);
	if (minor < DEVCOUNT) 
	{
		if (openCnt > 0) 
		{
			return -EALREADY;
		} 
		else 
		{
			openCnt++;
			if ( openCnt == 1 )
			{
				cfg = io_rd32(PWM_CFG);
				//cfg |= PWM_CFG_RST | PWM_CFG_EN;
				cfg |= PWM_CFG_EN;
				io_wr32(PWM_CFG,cfg);
				io_wr32(PWM_CFG,0x00000001);
				
				ctr = io_rd32(PWM_CTR);
				ctr |= PWM_CTR_HOST | PWM_CTR_EN;
#if defined CONFIG_ACCIO_A2818T || CONFIG_ACCIO_CM5210 || CONFIG_ACCIO_P1_SK01
				ctr |= (3<<24) | PWM_CTR_APB; //usr apb system clock, apb = 60Hz , so we will reach 200HZ, must 60*4
#endif
				io_wr32(PWM_CTR,ctr);
			}
			return 0;
		}
	}
	return -ENOENT;
}

static int sslbl_close(struct inode *inode, struct file *filp)
{
	int minor;

	minor = MINOR(inode->i_rdev);
	
	if (minor < DEVCOUNT) 
	{
		openCnt--;
		printk("backlight_clos,default the register\n");		
	}
	return 0;
}

static struct file_operations sslbl_fops = {
	.owner = THIS_MODULE,
	.open = sslbl_open,
	.release = sslbl_close,
	.ioctl = sslbl_ioctl,
};

static int __init sslbl_init(void)
{
	int ret;
	openCnt = 0;
	if ((ret = alloc_chrdev_region(&dev, 0, DEVCOUNT, "backlight")) < 0) 
	{
		printk(KERN_ERR \
				"backlight: Couldn't alloc_chrdev_region, ret=%d\n",ret);
		return 1;
	}
	cdev_p = cdev_alloc();
	cdev_p->ops = &sslbl_fops;
	ret = cdev_add(cdev_p, dev, DEVCOUNT);
	if (ret) 
	{
		printk(KERN_ERR \
				"backlight: Couldn't cdev_add, ret=%d\n", ret);
		return 1;
	}

	return 0;
}

static void __exit sslbl_exit(void)
{
	cdev_del(cdev_p);
	unregister_chrdev_region(dev, DEVCOUNT);
}

module_init(sslbl_init);
module_exit(sslbl_exit);

MODULE_LICENSE("GPL");
