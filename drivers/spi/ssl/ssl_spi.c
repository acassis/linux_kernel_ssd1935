#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/spi/spi.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define SSLSPIDMA
#ifdef SSLSPIDMA
#include <linux/dma-mapping.h>
#include <asm/arch/dma.h>
#endif

#include "spi.h"


#if IO_MAP == 3
#warning "using ebm interface"
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


#define SSLSPI_TXDUMMY
#define SSLSPI_SWAP


typedef struct
{
	spi_t		hw;
	int			sta;
	int			busy;
	int			cs;
	spinlock_t				lock;
	struct completion		done;
	struct workqueue_struct	*wq;
	struct work_struct		work;
	struct list_head		q;
	struct spi_master		*master;
#ifdef SSLSPIDMA
	int			rc_dma;
	uint32_t	rc_adr;
#endif
}
sslspi_ctx;

#define DRV_DESC	"spi"
static const char driver_name [] = DRV_DESC;


static irqreturn_t sslspi_irq(int irq, void *dev_id)
/**<
SPI ISR top
*/
{
	sslspi_ctx	*ctx = dev_id;

	return spi_isr(&ctx->hw);
}


static void sslspi_event(void *usr, spi_ev_e e)
/**<
SPI event callback
@param[in]	usr		context
@param[in]	e		error
*/
{
	sslspi_ctx *ctx = usr;

	if (!ctx->sta)
	{
		ctx->sta = e;
		if (e && (ctx->busy & 2))
		{
			return;
		}
	}
	complete(&ctx->done);
}


#ifdef SSLSPIDMA

static void sslspi_dma_nop(void *usr, dma_evt_e e)
/**<
DMA completion callback for TX when RX is also active
@param[in]	usr		context
@param[in]	e		error
*/
{
//dev_info(&((sslspi_ctx *)usr)->master->dev, "dma_nop %X\n", e);
	;
}


static void sslspi_dma_done(void *usr, dma_evt_e e)
/**<
DMA completion callback, waits for SPI to finish any remaining transmission
@param[in]	usr		context
@param[in]	e		error
*/
{
	sslspi_ctx *ctx = usr;

//dev_info(&ctx->master->dev, "dma_done %X\n", e);
	if (!ctx->sta)
	{
		ctx->sta = e;
	}
#if 1
	if (spi_dmawait(&ctx->hw))
	{
		/* wait for SPI tx to finish */
	//	dev_info(&ctx->master->dev, "wait for spi tx to finish\n");
		ctx->busy = 1;
	}
	else
	{
	//	dev_info(&ctx->master->dev,"dma done\n");
		complete(&ctx->done);
	}
#endif
}

#endif


#ifdef SSLSPI_SWAP

static void sslspi_swap(int swap, void *buf, int len)
/**<
endian swap buffer for 8 in 16/32 or 16 in 32, for packing as SPI is LSb
@param[in]		swap	type of swap
@param[in/out]	buf		buffer to swap
@param[in]		len		byte length of buffer
*/
{
	switch (swap)
	{
		case 1:	/* 8 in 16 */
		{
			uint16_t	*p = buf;

			while (len -= 2)
			{
				uint16_t	d;

				d = *p;
				*p = (d << 8) | (d >> 8);
				p++;
			}
			break;
		}

		case 2:	/* 16 in 32 */
		{
			uint32_t	*p = buf;

			while (len -= 4)
			{
				uint32_t	d;

				d = *p;
				*p = (d << 16) | (d >> 16);
				p++;
			}
			break;
		}

		case 3:	/* 8 in 32 */
		{
			uint32_t	*p = buf;

			while (len -= 4)
			{
				uint32_t	d;

				d = *p;
				*p = (d << 24) | (d >> 24) | 
						((d >> 8) & 0xFF00) | ((d << 8) & 0xFF0000);
				p++;
			}
			break;
		}
	}
}

#endif

static void sslspi_worker(struct work_struct *work)
/**<
perform message transfers in non IRQ time
@param[in]	work	standard Linux work structure
*/
{
	sslspi_ctx		*ctx;
	unsigned long	flags;

	ctx = container_of(work, sslspi_ctx, work);

	spin_lock_irqsave(&ctx->lock, flags);
	while (!list_empty(&ctx->q))
	{
		struct spi_message	*m;
		struct spi_transfer	*t;
		spi_cfg_p			cfg;
		int					first;

		m = container_of(ctx->q.next, struct spi_message, queue);
		list_del_init(&m->queue);
		spin_unlock_irqrestore(&ctx->lock, flags);

		first = 1;
		cfg = spi_get_ctldata(m->spi);
		list_for_each_entry(t, &m->transfers, transfer_list)
		{
			spi_xfr_t	x;
			int			bits, speed;
		/* Change speed and bit per word on a per transfer */
			bits = t->bits_per_word;
			speed = t->speed_hz;
			if (speed || bits)
			{
				spi_cfg_t	cfgt;

				cfgt = *cfg;
				// if(t-> len >= 32)
				if (t-> len & ~0x1f)
				{
					cfgt.burst = 1;		
				}
				else
				{
					cfgt.burst = 0;
				}
				if (bits)
				{
					cfgt.word = bits & 0x3f;
				}
			#ifdef SSLSPI_SWAP
				else
				{
					bits = cfgt.word;
				}
			#endif
				if (speed)
				{
					cfgt.baud = speed;
				}
//printk("ssl_io cfg cs=%d word=%d baud=%d\n", cfgt.cs, cfgt.word, cfgt.baud);
				ctx->cs = cfg->cs;
				spi_cfg(&ctx->hw, &cfgt);
			}

			/* configure before use */
			if (ctx->cs != cfg->cs)
			{
				ctx->cs = cfg->cs;
				spi_cfg(&ctx->hw, cfg);
			}

		
			/* transfer */
			x.rx = t->rx_buf;
			x.tx = t->tx_buf;
			x.len = t->len;
			x.flag = 0;

			/* first xfer in msg or previously cs_change indicated - CS=1 */
			if (first)
			{
				first = 0;
				spi_cs(&ctx->hw, 1);
				/* PROPRIETARY: use controller_data  for CS delay us 
					WM8731 requires this */
#if 1
				if (m->spi->controller_data)
				{
					udelay((int)m->spi->controller_data);
				}
#endif
			}

			ctx->sta = 0;

#ifdef SSLSPI_SWAP
			bits >>= 6;
			if (x.tx && bits)
			{
				sslspi_swap(bits, (void *)x.tx, x.len);
			}
#endif

#ifdef SSLSPIDMA
{
			int	fifo;
			int	words;

			fifo = ctx->hw.fifosize;
			words = x.len >> ctx->hw.inc;
			/* use DMA if available and only for larger than FIFO size */
			if (ctx->rc_dma >= 0 && words >= fifo)
			{
				dma_cfg_t	dmac;
				int			dmarx = -1, dmatx = -1;

				fifo >>= 1;
				dmac.flag = DMA_F_ALL;
				dmac.flush = !!(words & (fifo - 1));
				dmac.count = x.len;
				dmac.evt.ctx = ctx;
//				fifo <<= ctx->hw.inc;
//printk("fifo=%d inc=%d\n", fifo, ctx->hw.inc);

				if (x.rx)
				{
					if (!m->is_dma_mapped)
					{
						t->rx_dma = dma_map_single(&ctx->master->dev, 
										(void *)t->rx_buf,
										dmac.count, DMA_FROM_DEVICE);
						if (dma_mapping_error(t->rx_dma)) 
						{
							goto l_nodma;
						}
					}
					dmac.evt.evt = sslspi_dma_done;
					dmac.daddr = t->rx_dma;
					dmac.saddr = spi_rxdma_addr(ctx->rc_adr);
					dmac.src.width = 1 << ctx->hw.inc;
					dmac.src.depth = fifo;
					dmac.src.req = spi_rxdma_req(ctx->rc_dma);
					dmac.dst.width = dmac.dst.depth = dmac.dst.req = 0;
					dmarx = ssldma_request(&dmac);
					if (dmarx < 0)
					{
						dev_info(&ctx->master->dev, 
							"worker - dma_request rx err\n");
						goto l_unmaprx;
					}
				}
				if (x.tx)
				{
					if (!m->is_dma_mapped)
					{
						t->tx_dma = dma_map_single(&ctx->master->dev, 
										(void *)t->tx_buf,
										dmac.count, DMA_TO_DEVICE);
						if (dma_mapping_error(t->tx_dma)) 
						{
							goto l_undorx;
						}
						goto l_nodma;
					}
					dmac.saddr = t->tx_dma;
#ifdef SSLSPI_TXDUMMY
				}
				else
				{
					dmac.saddr = 0;
				}
#endif
					dmac.evt.evt = (dmarx >= 0) ? 
						sslspi_dma_nop : sslspi_dma_done;
					dmac.daddr = spi_txdma_addr(ctx->rc_adr);
					dmac.dst.width = 1 << ctx->hw.inc;
#ifdef SSLSPI_TXDUMMY
					/* use single word for dummy tx to allow rx assert 
						priority more often to prevent overrun
					*/
					dmac.dst.depth = x.tx ? fifo : 1;
//					dmac.dst.depth = fifo;
#else
					dmac.dst.depth = fifo;
#endif
					dmac.dst.req = spi_txdma_req(ctx->rc_dma);
					dmac.src.width = dmac.src.depth = dmac.src.req = 0;
					dmatx = ssldma_request(&dmac);
					if (dmatx < 0)
					{
						dev_info(&ctx->master->dev, 
							"worker - dma_request tx err\n");
						goto l_unmaptx;
					}
#ifndef SSLSPI_TXDUMMY
				}
#endif
dev_info(&ctx->master->dev, "dma r%d t%d\n", dmarx, dmatx);
				if (dmarx >= 0)
				{
					ssldma_enable(dmarx);
				}
				if (dmatx >= 0)
				{
					ssldma_enable(dmatx);
				}

				/* start DMA trigger from SPI FIFO & wait */
				ctx->busy = 3;
				x.flag = SPI_XFR_DMA;
				spi_io(&ctx->hw, &x);
				if(wait_for_completion_timeout(&ctx->done,HZ) == 0)
				{
					printk(KERN_ERR"spi worker:wait dma done timeout\n");
				}

				/* unmap on completion */
				if (dmatx >= 0)
				{
					ssldma_free(dmatx);
l_unmaptx:
#ifdef SSLSPI_TXDUMMY
					if (x.tx)
#endif
					if (!m->is_dma_mapped)
					{
						dma_unmap_single(NULL, t->tx_dma,
							dmac.count, DMA_TO_DEVICE);
					}
				}
l_undorx:
				if (dmarx >= 0)
				{
					ssldma_free(dmarx);
l_unmaprx:
					if (!m->is_dma_mapped)
					{
						dma_unmap_single(NULL, t->rx_dma,
							dmac.count, DMA_FROM_DEVICE);
					}
				}
l_nodma:;
			}
}
			if (!x.flag)
			{
#endif
			ctx->busy = 1;
			spi_io(&ctx->hw, &x);
			wait_for_completion(&ctx->done);

#ifdef SSLSPIDMA
			}
#endif
			ctx->busy = 0;

#ifdef SSLSPI_SWAP
			if (bits)
			{
				sslspi_swap(bits, x.rx, x.len);
			}
#endif

#if 0
{
int i;
if (t->rx_buf)
{
printk("rx:\n");
for (i = 0; i < t->len; i++)
{
	printk("%02X ", ((char *)t->rx_buf)[i]);
	if ((i & 15) == 15) printk("\n");
}
if (i & 15) printk("\n");
}
if (t->tx_buf)
{
printk("tx:\n");
for (i = 0; i < t->len; i++)
{
	printk("%02X ", ((char *)t->tx_buf)[i]);
	if ((i & 15) == 15) printk("\n");
}
if (i & 15) printk("\n");
}
}
#endif

			if (ctx->sta)
			{
				/* exit on error */
				dev_err(&ctx->master->dev, "worker err - %X\n", ctx->sta);
				break;
			}

			/* delay between xfers */
			if (t->delay_usecs)
			{
				udelay(t->delay_usecs);
			}

			/* cs_change indicated - CS=0 */
			if (t->cs_change)
			{
				spi_cs(&ctx->hw, 0);
				first = 1;
			}
		}
		spi_cs(&ctx->hw, 0);
		m->status = ctx->sta ? -EINVAL : 0;
		m->complete(m->context);
		spin_lock_irqsave(&ctx->lock, flags);
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}


static int sslspi_xfer(struct spi_device *spi, struct spi_message *msg)
/**<
accept and queue messages for scheduled transfer
*/
{
	sslspi_ctx	*ctx;
	unsigned long flags;

	msg->actual_length = 0;
	msg->status = -EINPROGRESS;

	ctx = spi_master_get_devdata(spi->master);
	spin_lock_irqsave(&ctx->lock, flags);
	list_add_tail(&msg->queue, &ctx->q);
	queue_work(ctx->wq, &ctx->work);
	spin_unlock_irqrestore(&ctx->lock, flags);
	return 0;
}
int sslspi_uncfg(struct spi_device *spi)
{
    sslspi_ctx  *ctx;
    unsigned long flags;

    ctx = spi_master_get_devdata(spi->master);
    spin_lock_irqsave(&ctx->lock, flags);
    ctx->cs = -1;
    spin_unlock_irqrestore(&ctx->lock, flags);
    return 0;
}
EXPORT_SYMBOL(sslspi_uncfg);

static int sslspi_setup(struct spi_device *spi)
/**<
configuration for a particular device
*/
{
	sslspi_ctx		*ctx;
	spi_cfg_p		cfg;
	register int	mode;

	ctx = spi_master_get_devdata(spi->master);
	cfg = spi_get_ctldata(spi);
	if (!cfg)
	{
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg)
		{
			printk("spi: setup err - alloc\n");
			return -ENOMEM;
		}

		cfg->per_clk = MAGUS_CLK_SPI;
#if 1	/* IS THIS NECESSARY? */
		cfg->datdly = 5;
		cfg->csdly = 16;
#endif
		spi_set_ctldata(spi, cfg);
	}

	cfg->cs = spi->chip_select;
	mode = spi->bits_per_word & 0x3F;
	cfg->word = mode ? mode : 8;
	cfg->baud = spi->max_speed_hz;
	mode = spi->mode;
#if 0
	cfg->clk_phase = !!(mode & SPI_CPHA);
	cfg->clk_fall = !!(mode & SPI_CPOL);
	cfg->cs_high = !!(mode & SPI_CS_HIGH);
	cfg->lsb_first = !!(mode & SPI_LSB_FIRST);
#endif
	cfg->clk_phase = (mode & SPI_CPHA) ? 1 : 0;
	cfg->clk_fall = (mode & SPI_CPOL) ? 1 : 0;
	cfg->cs_high = (mode & SPI_CS_HIGH) ? 1 : 0;
	cfg->lsb_first = (mode & SPI_LSB_FIRST) ? 1 : 0;
//printk("cs=%d bit=%d hz=%d mode=x%X clkedge=%d pol=%d, cspol=%d lsb=%d\n", 
//cfg->cs, cfg->word, cfg->baud, mode, 
//cfg->clk_fall, cfg->clk_phase, cfg->cs_high, cfg->lsb_first);
	return 0;
}


static void sslspi_cleanup(struct spi_device *spi)
{
	spi_cfg_p	cfg;

	cfg = spi_get_ctldata(spi);
	if (cfg)
	{
		kfree(cfg);
	}
}


static int sslspi_probe(struct platform_device *dev)
{
	int					ret;
	sslspi_ctx			*ctx;
	struct spi_master	*master;
	struct resource		*rc, *rc2;
	int					irq;

	master = spi_alloc_master(&dev->dev, sizeof(sslspi_ctx));
	if (!master)
	{
		dev_err(&dev->dev, "probe - spi_alloc_master failed\n");
		return -ENOMEM;
	}

	ctx = spi_master_get_devdata(master);
	ctx->master = master;
	ctx->cs = -1;	/* no device configured */

	master->bus_num = dev->id;
	master->num_chipselect = 4;
	master->cleanup = sslspi_cleanup;
	master->setup = sslspi_setup;
	master->transfer = sslspi_xfer;

	rc = platform_get_resource(dev, IORESOURCE_MEM, 0);
#if IO_MAP == 1
	if (!request_mem_region(rc->start, rc->end - rc->start + 1, driver_name))
	{
		ret = -EBUSY;
		dev_err(&dev->dev, "probe - req_mem_reg failed\n");
		goto l_free;
	}
#ifdef SSLSPIDMA
	ctx->rc_adr = rc->start;
#endif
	ctx->hw.r = ioremap_nocache(rc->start, rc->end - rc->start + 1);
	if (!ctx->hw.r)
	{
		ret = -EBUSY;
		dev_err(&dev->dev, "probe - ioremap failed\n");
		goto l_free_rgn;
	}
#else
	ctx->hw.r = (void *)rc->start;
#ifdef SSLSPIDMA
	ctx->rc_adr = rc->start;
#endif
#endif

	rc2 = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	irq = rc2 ? rc2->start : -1;
#ifdef SSLSPIDMA
	rc2 = platform_get_resource(dev, IORESOURCE_DMA, 0);
	ctx->rc_dma = rc2 ? rc2->start : -1;
#endif

	ctx->hw.ctx	= ctx;
	ctx->hw.ev = sslspi_event;
	ctx->hw.master = 1;

	/* Register for SPI Interrupt */
	ret = request_irq(irq, sslspi_irq, IRQF_DISABLED, driver_name, ctx);
	if (ret)
	{
		dev_err(&dev->dev, "probe - request_irq %d err %d\n", irq, ret);
		ret = -EINVAL;
		goto l_irq_err;
	}

	ret = spi_init(&ctx->hw);
	if (ret)
	{
		goto l_hw_err;
	}

	INIT_LIST_HEAD(&ctx->q);
	spin_lock_init(&ctx->lock);
	init_completion(&ctx->done);
	INIT_WORK(&ctx->work, sslspi_worker);
	ctx->wq = create_singlethread_workqueue(master->dev.parent->bus_id);
	if (!ctx->wq)
	{
		dev_err(&dev->dev, "probe - wq err\n");
		ret = -EBUSY;
		goto l_wq_err;
	}

	platform_set_drvdata(dev, ctx);
	ret = spi_register_master(master);
	if (!ret)
	{
        if (irq == MAGUS_IRQ_SPI2)
		{
			/* enable SPI2, SPI2_CLK:PF9, SPI2_CS0:PF8, SPI2_MOSI:PF6, SPI2_MISO:PF5 */
		   *(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x518) &= ~0x360;
		   *(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x514) |= 0x360;
		}
		return ret;
	}

	dev_err(&dev->dev, "probe - spi_register_master failed\n");
	destroy_workqueue(ctx->wq);
l_wq_err:
	spi_exit(&ctx->hw);
l_hw_err:
	free_irq(irq, ctx);
l_irq_err:
#if IO_MAP == 1
	iounmap(ctx->hw.r);
l_free_rgn:
	release_mem_region(rc->start, rc->end - rc->start + 1);
#endif
l_free:
	spi_master_put(master);
	return ret;
}


static int sslspi_remove(struct platform_device *dev)
{
	sslspi_ctx		*ctx;
	struct resource	*rc;

	ctx = platform_get_drvdata(dev);

	flush_workqueue(ctx->wq);
	destroy_workqueue(ctx->wq);
	spi_unregister_master(ctx->master);
	spi_exit(&ctx->hw);
	rc = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (rc)
	{
		free_irq(rc->start, ctx);
	}
#if IO_MAP == 1
	iounmap(ctx->hw.r);
	rc = platform_get_resource(dev, IORESOURCE_MEM, 0);
	release_mem_region(rc->start, rc->end - rc->start + 1);
#endif
	return 0;
}


static struct platform_driver sslspi_drv =
{
	.driver =
	{
		.name = "spi",
		.owner = THIS_MODULE,
	},
	.probe = sslspi_probe,
	.remove = __devexit_p(sslspi_remove),
};


static int __init sslspi_init(void)
{
	platform_driver_register(&sslspi_drv);
	return 0;
}

static void __exit sslspi_exit(void)
{
	platform_driver_unregister(&sslspi_drv);
}


module_init(sslspi_init);
module_exit(sslspi_exit);

MODULE_DESCRIPTION("SSL_SPI");
MODULE_LICENSE("GPL");
