#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/i2c.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "i2c.h"

#if IO_MAP == 3
#warning "using ebm"
#include "ebm_mem.h"
#include "io.h"
#endif

#if INTC
#warning "using intc as a module"
#include "intck.h"
#define request_irq	intc_req
#define free_irq	intc_free
#define enable_irq	intc_enable
#define disable_irq	intc_disable
#endif

#define I2C_SSL_ID 0x0001
#define DRV_DESC	"i2c"

#define I2C_CLK 115200
#define PER_CLK	32000000

static const char	zdrv[] = DRV_DESC;

//#define I2C_SIMPNP
#ifdef I2C_SIMPNP
uint32_t	addr = 0x8102000;
int			irq = 0x1A;
module_param(addr, uint, 0);
module_param(irq, int, 0);

static struct resource _rc[2];
static struct platform_device pdv = 
{
	.name = DRV_DESC,
#if IO_MAP == 1
	.num_resources = 2,
	.resource = _rc,
#endif
};
#endif

struct sslctx
{
	struct i2c_adapter	adapter;
	struct completion	comp;
	i2c_t				hw;
};


static void i2c_evt(void *ctx)
{
	complete(&((struct sslctx *)ctx)->comp);
}


static int ssli2c_do(struct sslctx *ssl, struct i2c_msg *msg, int f)
{
	i2c_cfg_t		cfg;
	i2c_cmd_t		cmd;
	i2c_td_p		td;

	cfg.addr = msg->addr;
	cfg.ack = !(msg->flags & I2C_M_IGNORE_NAK);
	cfg.ackhi = 0;
	cfg.i2c_clk_hz = I2C_CLK;
	cfg.per_clk_hz = PER_CLK;
	cmd.wr.len = cmd.rd.len = 0;

	cmd.cmd = f;

	td = (msg->flags & I2C_M_RD) ? &cmd.rd : &cmd.wr;
	td->buf = msg->buf;
	td->len = msg->len;

	init_completion(&ssl->comp);
	i2c_cfg(&ssl->hw, &cfg);
	i2c_rw(&ssl->hw, &cmd);
	wait_for_completion(&ssl->comp);
	if (td->actual != msg->len)
	{
		printk("ssli2c: do err - actual=%d len=%d\n", td->actual, msg->len);
		return -EREMOTEIO;
	}

	return 0;
}


static int ssli2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int n)
{
	struct sslctx *ssl = i2c_get_adapdata(adap);
	int	i, rc;

	for (i = 0; i < n; i++)
	{
		int	f, nstart;

		nstart = msgs[i].flags & I2C_M_NOSTART;
		if (!i)
		{
			f = (n == 1) ? I2C_CMD_DONE : I2C_CMD_START;
		}
		else if (i == n - 1)
		{
			f = nstart ? I2C_CMD_END : I2C_CMD_DONE;
		}
		else
		{
			f = nstart ? I2C_CMD_MID : I2C_CMD_START;
		}
		rc = ssli2c_do(ssl, &msgs[i], f);
//printk("ssli2c: xfer info - cmd=%d rt=%d\n", f, rc);
		if (rc < 0)
		{
			return rc;
		}
	}
	return n;
}


static u32 ssli2c_caps(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
	//return I2C_FUNC_I2C;
}


static const struct i2c_algorithm ssli2c_algo = 
{
	.master_xfer = ssli2c_xfer,
	.functionality = ssli2c_caps,
};


static int ssli2c_intr(int irq, void *dev_id)
{
	return !i2c_isr(&((struct sslctx *)dev_id)->hw);
}


static int ssli2c_probe(struct platform_device *dev)
{
	int	ret;
#if IO_MAP == 1
	struct resource			*rc = dev->resource;
#else
	struct resource			*rc = _rc;
#endif
	struct sslctx	*sslctx;

	sslctx = kmalloc(sizeof(struct sslctx), GFP_KERNEL);
	if (!sslctx)
	{
		printk("ssli2c: probe err - alloc\n");
		return -ENOMEM;
	}
	memset(sslctx, 0, sizeof(*sslctx));

#if IO_MAP == 1
	if (!request_mem_region(rc[0].start, rc[0].end - rc[0].start + 1, zdrv))
	{
		printk("ssli2c: probe err - request_mem_region\n");
		ret = -EBUSY;
		goto exit_unmap_regs;
	}
	sslctx->hw.r = ioremap_nocache(rc[0].start, 
								rc[0].end - rc[0].start + 1);
	if (!sslctx->hw.r)
	{
		printk("ssli2c: probe err - remap\n");
		ret = -EBUSY;
		goto exit_kfree;
	}
#else
	sslctx->hw.r = (void *) rc[0].start;
#endif

	sslctx->adapter.id = I2C_SSL_ID;
	sslctx->adapter.algo = &ssli2c_algo;
	sslctx->adapter.owner = THIS_MODULE;
	sslctx->adapter.class = I2C_CLASS_HWMON;
	strlcpy(sslctx->adapter.name, dev->dev.driver->name, I2C_NAME_SIZE);
	sslctx->adapter.dev.parent = &dev->dev;
	platform_set_drvdata(dev, sslctx);
	i2c_set_adapdata(&sslctx->adapter, sslctx);

	if (request_irq(rc[1].start, ssli2c_intr, 0, zdrv, sslctx)) 
	{
		ret = -EINVAL;
		printk("ssli2c: probe err - request_irq\n");
		goto exit_unmap_regs;
	}
	sslctx->adapter.nr = (int)dev->id >= 0 ? dev->id : 0;
	ret = i2c_add_numbered_adapter(&sslctx->adapter);
	//ret = i2c_add_adapter(&sslctx->adapter);
	if (ret) {
		printk("add adapter error\n");
		goto exit_free_irq;
	}

	sslctx->hw.evt = i2c_evt;
	sslctx->hw.ctx = sslctx;
	ret = i2c_init(&sslctx->hw);
	if (ret)
	{
		ret = -ENODEV;
		printk("ssli2c: probe err - i2c_init\n");
		goto exit_free_irq;
	}
	return 0;

exit_free_irq:
	free_irq(rc[1].start, sslctx);

exit_unmap_regs:
#if IO_MAP == 1
	iounmap(sslctx->hw.r);
exit_kfree:
#endif
	kfree(sslctx);
	return ret;
}


static int ssli2c_remove(struct platform_device *dev)
{
#if IO_MAP == 1
	struct resource	*rc = dev->resource;
#else
	struct resource	*rc = _rc;
#endif
	struct sslctx	*sslctx;
	int	ret;

	sslctx = platform_get_drvdata(dev);
	i2c_exit(&sslctx->hw);
	ret = i2c_del_adapter(&sslctx->adapter);
	if (ret)
	{
		printk("ssli2c: remove err - i2c_del_adapter\n");
	}

	disable_irq(rc[1].start);
	free_irq(rc[1].start, sslctx);
#if IO_MAP == 1 
	iounmap(sslctx->hw.r);
	release_mem_region(rc[0].start, rc[0].end - rc[0].start + 1);
#endif
	kfree(sslctx);
	return ret;
}


static struct platform_driver ssli2c_drv = 
{
	.probe	= ssli2c_probe,
	.remove	= ssli2c_remove,
	.driver =
	{
		.name	= (char *)zdrv,
		.owner	= THIS_MODULE,
	}
};


static int __init ssli2c_init(void)
{
	int ret;

#ifdef I2C_SIMPNP
	_rc[0].start = addr;
	_rc[0].end = addr + 0x0020;
	_rc[0].flags = IORESOURCE_MEM;
	_rc[1].start = irq;
	_rc[1].end = irq;
	_rc[1].flags = IORESOURCE_IRQ;

	ret = platform_device_register(&pdv);
	if (ret)
	{
		printk("ssli2c: init err - plat_dev_reg\n");
		return ret;
	}
//	printk("addr=%08X irq=%d flag=%08X\n", addr, irq);
#endif

	ret = platform_driver_register(&ssli2c_drv);
	if (ret)
	{
		printk("ssli2c: init err - plt_drv_reg %d\n", ret);
#ifdef I2C_SIMPNP
		platform_device_unregister(&pdv);
#endif
	}
	return ret;
}


static void __exit ssli2c_exit(void)
{
	platform_driver_unregister(&ssli2c_drv);
#ifdef I2C_SIMPNP
	platform_device_unregister(&pdv);
#endif
}


module_init(ssli2c_init);
module_exit(ssli2c_exit);

MODULE_AUTHOR("luanxu");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");
