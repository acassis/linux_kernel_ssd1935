#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h> 
#include <linux/clk.h>
#include <asm/io.h> 
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>



#include "os.h" 
#include "nfcr.h"
#include "nfc.h"


//extern uint32_t	gfix,gerr,gtotal_errcnt,gmax_errcnt;
//extern int nfc_polling(nfc_p t, uint32_t sta);

static char _tmpbuf[8192 + 512];

typedef struct
{
	//spinlock_t			lock;			//stanley
	struct completion	comp;
	volatile int		status;
	uint32_t			wptr;
	uint32_t			rptr;
	int					pgaddr;
	int					col;
	nfc_p				hw;
}
ssl_t, *ssl_p;

//#define SSL_NAND_DRV_INFO
#ifdef SSL_NAND_DRV_INFO
#define NAND_DRV_INFO(fmt, args...)	printk("SSL NAND DRV - " fmt, ##args)
#else
#define NAND_DRV_INFO(fmt, args...)
#endif

#define SSL_NAND_DRV_ERR
#ifdef SSL_NAND_DRV_ERR
#define NAND_DRV_ERR(fmt, args...)	printk("SSL NAND DRV - " fmt, ##args)
#else
#define NAND_DRV_ERR(fmt, args...)
#endif

/* Total number of MTD partitions */
#define SSL_MTD_PARTITIONS_COUNT	2


/* controller and mtd information */

struct sslnfc_ctx;

typedef struct
{
	struct mtd_info		mtd;
	struct nand_chip	chip;
	struct sslnfc_ctx	*info;
}
sslnfc_chip;

typedef struct sslnfc_ctx
{
	/* mtd info */
	struct nand_hw_control	controller;
	sslnfc_chip	*mtd;

	/* device info */
	nfc_t	hw;
	int		irq;
}
sslnfc_ctx;


#define DRV_DESC	"nfc"
static const char driver_name [] = DRV_DESC;

//#define NFC_SIMPNP
#ifdef NFC_SIMPNP
//static u32	 addr = 0xDC000000;	/* Artemis */
static u32	 addr = 0x40000000;	/* Aphrodite */
static int	 irq = 0x0D;
module_param(addr, uint, 0);
module_param(irq, int, 0);
static struct resource _rc[2];
static struct platform_device dv =
{
	.name = DRV_DESC,
#if IO_MAP == 1
	.num_resources = 2,
	.resource = _rc,
#endif
};
#endif


#if IO_MAP == 1

static struct resource *platform_device_resource(struct platform_device *dev,
							unsigned int mask, int nr)
{
	int i;

	for (i = 0; i < dev->num_resources; i++)
		if (dev->resource[i].flags == mask && nr-- == 0)
			return &dev->resource[i];
	return NULL;
}


static int platform_device_irq(struct platform_device *dev, int nr)
{
	int i;

	for (i = 0; i < dev->num_resources; i++)
		if (dev->resource[i].flags == IORESOURCE_IRQ && nr-- == 0)
			return dev->resource[i].start;
	return NO_IRQ;
}

#endif


static void sslnfc_event(void *ctx, uint32_t e)
{
	struct nand_chip *nchip = ctx;
	ssl_p	sslp = nchip->priv;

	sslp->status = e;
	complete(&sslp->comp);
}


static irqreturn_t sslnfc_irq(int irq, void *dev_id)
{
	struct nand_chip	*chip = dev_id;

	return nfc_isr(((ssl_p)chip->priv)->hw);
}


static int sslnfc_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	int status;
	int length = mtd->writesize + mtd->oobsize;

	memset(_tmpbuf, 0, length);

	/* Read the complete page */
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);
	chip->read_buf(mtd, _tmpbuf, length);

	/* Update the OOB area of the page */
	memcpy(&_tmpbuf[mtd->writesize], chip->oob_poi, mtd->oobsize);

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0, page);
	chip->write_buf(mtd, _tmpbuf, length);
	/* Send command to program the OOB data */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);
	return status & NAND_STATUS_FAIL ? -EIO : 0;
}


static void sslnfc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	nfc_p	hw = ((ssl_p)chip->priv)->hw;
	uint32_t	*idx = &((ssl_p)chip->priv)->rptr;
	uint32_t	flags;			
	ssl_p		sslp = chip->priv;
						
	//spin_lock_irqsave(&sslp->lock, flags);
	nfc_read(hw, buf, *idx, len);
	//spin_unlock_irqrestore(&sslp->lock, flags);	
	(*idx) += len;
}


static int sslnfc_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	uint32_t	length;

	length = mtd->writesize + mtd->oobsize;
	sslnfc_read_buf(mtd, _tmpbuf, length);
	if (memcmp(buf, _tmpbuf, length))
	{
		NAND_DBG_ERR("Verification failed");
		return -EFAULT;
	}
	return 0;
}


static void sslnfc_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	nfc_p	hw = ((ssl_p)chip->priv)->hw;
	uint32_t	*idx = &((ssl_p)chip->priv)->wptr;
	
		
			
	nfc_write(hw, buf, *idx, len);
	//can reach 2.5M in Msc
	//nfc_write_mlc(hw, buf, *idx, len);
	
	(*idx) += len;
}


static uint8_t sslnfc_read_byte(struct mtd_info *mtd)
{
	uint8_t	d;
	struct nand_chip *chip = mtd->priv;
	nfc_p	hw = ((ssl_p)chip->priv)->hw;
	uint32_t	*idx = &((ssl_p)chip->priv)->rptr;

	nfc_read(hw, &d, *idx, 1);
	(*idx)++;
	return d;
}


static u16 sslnfc_read_word(struct mtd_info *mtd)
{
	uint16_t	d;
	struct nand_chip *chip = mtd->priv;
	nfc_p	hw = ((ssl_p)chip->priv)->hw;
	uint32_t	*idx = &((ssl_p)chip->priv)->rptr;

	nfc_read(hw, (uint8_t *)&d, *idx, 2);
	(*idx) += 2;
	return d;
}


static void sslnfc_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd->priv;
	ssl_p	p = (ssl_p)chip->priv;

	p->hw->ce = chipnr;
}


static void sslnfc_cmd(struct mtd_info *mtd, unsigned int command,int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	ssl_p	sslp = (ssl_p)chip->priv;
	nfc_p	hw = sslp->hw;
		
	/* A new command being issued, reset the read byte index into
		controller buffer */
	sslp->wptr = sslp->rptr = 0;

	/* Reset the command completion flag */
	sslp->status = 0;
	init_completion(&sslp->comp);

	switch (command)
	{
		case NAND_CMD_READID:
			nfc_nand_id(hw, 5);
			goto l_wait;

		case NAND_CMD_READ0:
		case NAND_CMD_READOOB :
			
			nfc_nand_read(hw, page_addr);
			
			wait_for_completion(&sslp->comp);		
				
			//stanley remark it to test speed improvement
			#if 0
			if (sslp->status & NFC_EVT_ECC)
			{
				unsigned char	*tmp;
				//uint32_t  *tmp;
				uint32_t	i;
				uint32_t	size;
				/* TO1: check if this is an erased page ie. false error */
				tmp = _tmpbuf;
				//size = mtd->writesize + mtd->oobsize;
				//Read several bytes is enough, because we maybe has ecc error in blank page,
				//It will cause false error								
				size = 10;
				nfc_read(hw, tmp, 0, size);				

				for (i = 0; i < size; i++)
				{
					if (tmp[i] != 0xff)
					{
						nfcr_p	r;
						uint32_t st;
						r = hw->r;						
						st = io_rd32(r->ISR);							
						//NAND_DBG_ERR("hw->acen=%d \n",hw->acen);
						//NAND_DBG_ERR("hw->eccen=%d \n",hw->eccen);
						//NAND_DBG_ERR("r->ISR =0x%08x \n",r->ISR);						
						//NAND_DBG_ERR("ECC Error at Page-%d, index-%d\n",page_addr, i);
						//NAND_DBG_ERR("sslp->status 0x%08x \n",sslp->status);
						
						for (i = 0; i < size; i++)
							printk(KERN_ERR "%02x ",tmp[i]);
						printk(KERN_ERR "\n");
						
						#if 0
						NAND_DBG_ERR("Dumping complete page -\n");
						for (i = 0; i < mtd->writesize + mtd->oobsize; i++)
						{
							if ((i & 15) == 15)
								printk(KERN_ERR "\n");
							printk(KERN_ERR "%02x ",tmp[i]);
						}
						printk("\n");
						#endif
						
						
						//Stanley disable it for stability test
						//because ECC error doesn't means data error
						//it may cause by normal error at oob area, it will not affect data integrality
						//mtd->ecc_stats.failed++;
						break;
					}
				}								
				//NAND_DBG_ERR("nfc_isr status: false error \n");
			}
			
			#endif
			
			if (sslp->status & NFC_EVT_ECC_CORRECTED)
			{				
				//int b,p;
				//b=page_addr/128;
				//p=page_addr-b*128;	
				//NAND_DBG_ERR("blk %04d page %04d total fix count : %d ; max ECC correction : %d \n",b,p,(sslp->status>>8)&0x07 ,(sslp->status>>16)&0x07);	
				
				mtd->ecc_stats.corrected = (sslp->status>>16)&0x07;					
				
				/*{
					nfcr_p	r;
					uint32_t st;
					r = hw->r;																		
					NAND_DBG_ERR("r->ISR =0x%08x \n",io_rd32(r->ISR));
					NAND_DBG_ERR("r->TECC4R =0x%08x \n",io_rd32(r->TECC4R));
					NAND_DBG_ERR("r->TECC8R =0x%08x \n",io_rd32(r->TECC8R));
				}*/		
							
			}
			else
				mtd->ecc_stats.corrected = 0;	//should clear pervious read state			
			/* Adjust the index to read from correct location in OOB */
			sslp->rptr = (command == NAND_CMD_READOOB) ?
					mtd->writesize + column : column;
			break;

		case NAND_CMD_STATUS:
		{
			uint32_t	sta;						
			nfc_nand_status(hw);
			wait_for_completion(&sslp->comp);
			
			sta = nfc_getstatus(hw);
			/* TO1: put status in data buffer like normal */
			nfc_write(hw, (uint8_t *)&sta, 0, 4);
						
			break;
		}

		case NAND_CMD_SEQIN:
			sslp->col = column;
			sslp->pgaddr = page_addr;
			break;

		case NAND_CMD_PAGEPROG :
			if (sslp->col == -1 || sslp->pgaddr == -1)
			{
				return;
			}
			if (column == -1)
			{
				column = sslp->col;
				sslp->col = -1;
			}
			if (page_addr == -1)
			{
				page_addr = sslp->pgaddr;
				sslp->pgaddr = -1;
			}						
			nfc_nand_prog(hw, page_addr);			
			goto l_wait;

		case NAND_CMD_RESET :			
			nfc_nand_reset(hw);			
			goto l_wait;

		case NAND_CMD_ERASE1 :			
			nfc_nand_erase(hw, page_addr);			
			goto l_wait;

		case NAND_CMD_ERASE2 :
			break;
	}
	return;
l_wait:
	wait_for_completion(&sslp->comp);	
}




static int sslnfc_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	
	ssl_p	sslp = chip->priv;
	nfc_p	hw = ((ssl_p)chip->priv)->hw;
	unsigned long flags;	
	sslp->status = 0;
	init_completion(&sslp->comp);
	nfc_nand_status(hw);
	wait_for_completion(&sslp->comp);
	//spin_lock_irqsave(&sslp->lock, flags);
	//nfc_polling(hw,NFC_ISR_CMP);
	//spin_unlock_irqrestore(&sslp->lock, flags);
	return nfc_getstatus(hw);
	
}


/* With TECC4, 40 bytes of OOB will be used for ECC. That leaves us with
	only 24 free bytes. */
static struct nand_ecclayout nand_oob_24 =
{
	.eccbytes = 0,
	.eccpos = {0},
	.oobfree = { { .offset = 2, .length = 22 } }
};
/* With TECC8, 160 bytes of OOB will be used for ECC. That leaves us with
	only 58 free bytes. But only 56 bytes can be used */
static struct nand_ecclayout nand_oob_56 =
{
	.eccbytes = 0,
	.eccpos = {0},
	.oobfree = { { .offset = 2, .length = 54 } }
};


static int sslnfc_cfg(struct mtd_info *mtd)
{
	struct nand_chip	*chip = mtd->priv;
	nfc_cfg_t	cfg;
	nfc_p		hw = ((ssl_p)chip->priv)->hw;

	switch(mtd->writesize)
	{
		case 2048 :
			cfg.pgsz = 2;
			mtd->oobsize -= 40;
			chip->ecc.layout = &nand_oob_24;
			/* Set the ECC mode */
			hw->eccmode = NFC_ECC_MODE_TECC4;
			hw->eccctrl = NFC_TECC4_REDEC_EN;
			if (hw->eccen)
			{
				int ret;

				ret = nfc_setecc(hw);
				if (ret != NFC_ERR_NONE)
				{
					printk("nfc_setecc - err %d\n", ret);
					return -ENODEV;
				}
			}
			break;

		case 4096 :
						
			if(mtd->oobsize==128)
			{
				cfg.pgsz = 3;
				mtd->oobsize -= 80;
				chip->ecc.layout = &nand_oob_24;
				hw->eccmode = NFC_ECC_MODE_TECC4;
				hw->eccctrl = NFC_TECC4_REDEC_EN;
			}
			else
			{	
				cfg.pgsz = 3;
				mtd->oobsize -= 162;
				chip->ecc.layout = &nand_oob_56;
				hw->eccmode = NFC_ECC_MODE_TECC8;
				hw->eccctrl = 23;
			}	
			if (hw->eccen)
			{
				int ret;

				ret = nfc_setecc(hw);
				if (ret != NFC_ERR_NONE)
				{
					printk("nfc_setecc - err %d\n", ret);
					return -ENODEV;
				}
			}
			break;

		default :
//		case 256: cfg.pgsz = 0; break;
//		case 512: cfg.pgsz = 1; break;
			printk("No OOB size defined for this chip\n");
			return -ENODEV;
	}

	cfg.blksz = ffs(mtd->erasesize / mtd->writesize) - 4;
	cfg.rowsz = ffs(chip->chipsize / mtd->erasesize) - 1 +
			ffs(mtd->erasesize / mtd->writesize) - 1 - 1;
	cfg.colsz = chip->page_shift;
	cfg.buswidth = chip->options & NAND_BUSWIDTH_16;
NAND_DBG_INFO("PgSz=%d BlkSz=%d RowSz=%d ColSz=%d Buswidth=%d\n",
cfg.pgsz, cfg.blksz, cfg.rowsz, cfg.colsz, cfg.buswidth);
	nfc_cfg(hw, &cfg);

	return 0;
}


static int sslnfc_initchip(sslnfc_ctx *info)
{
	int ret;

	struct nand_chip *chip = &info->mtd->chip;
	nfc_p	hw = &info->hw;

	/* TECC8 is the default mode. Set the actual mode after reading the
		chip geometry */
	hw->eccmode = NFC_ECC_MODE_TECC8;
	hw->eccofs = NFC_ECC_OFFSET_DEFAULT;
	hw->eccctrl = NFC_TECC8_REDN_CNT; // NFC_TECC4_REDEC_EN;
	hw->acen = NFC_ACEN_EN;
	hw->eccen = NFC_ECC_EN;
	hw->ce = 0;
	hw->ctx = chip;
	hw->evt = sslnfc_event;
	/* Initialize the NAND Flash Controller */
	ret = nfc_init(hw);
	if (ret)
	{
		printk("chip init err\n");
		return ret;
	}

	/* Initialize the call back functions */
	chip->options = NAND_USE_FLASH_BBT /* | NAND_SKIP_BBTSCAN */;
	chip->controller = &info->controller;
	chip->ecc.mode = NAND_ECC_NONE;
	chip->ecc.write_oob = sslnfc_write_oob;
	chip->read_byte = sslnfc_read_byte;
	chip->read_word = sslnfc_read_word;
	chip->read_buf = sslnfc_read_buf;
	chip->write_buf = sslnfc_write_buf;
	chip->verify_buf = sslnfc_verify_buf;
	chip->select_chip = sslnfc_select_chip;
	chip->cmdfunc = sslnfc_cmd;
	chip->waitfunc = sslnfc_wait;

	chip->priv = kmalloc(sizeof(ssl_t), GFP_KERNEL);
	((ssl_p)chip->priv)->hw	= &(info->hw);

	info->mtd->info	= info;
	info->mtd->mtd.priv	= chip;
	info->mtd->mtd.owner = THIS_MODULE;
	return 0;
}


/* device management functions */

static int sslnfc_remove(struct platform_device *dev)
{
	sslnfc_ctx *info = platform_get_drvdata(dev);

	if (!info)
	{
		return 0;
	}
	platform_set_drvdata(dev, NULL);

	/* first thing we need to do is release all our mtds
	 * and their partitions, then go through freeing the
	 * resources used
	 */
	if (info->mtd)
	{
		sslnfc_chip *ptr = info->mtd;

		nand_release(&(ptr->mtd));
		disable_irq(info->irq);
		free_irq(info->irq, &info->mtd->chip);
		kfree(info->mtd);
	}

	if (info->hw.r)
	{
#if IO_MAP == 1
		iounmap(info->hw.r);
#endif
		info->hw.r = NULL;
	}

	kfree(info);

	return 0;
}

#if 1
static const struct mtd_partition _parts[] =
{
	{
		.name = "BootROM",
		.offset = 0,
		.size = 0x100000,				/* 1 MB */ /* include config*/
		.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "kernel",
		.offset = 0x100000,
		.size = 0x300000,				/* 3 MB */
		//.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "rootfs",
		.offset = 0x400000,
		.size = 0x800000,				/* 8 MB */
		//.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "app",
		.offset = /*MTDPART_OFS_NXTBLK*/(1+3+8+52)*1024*1024,
		.size = 0x10000000-52*1024*1024,				/* 256 MB */
	},
	{
		.name = "appdata",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 0xF400000,				/* 244 MB */ /* from BootROM to here is 512MB */
	},
	{
		.name = "data",
		.offset = (512+128)*1024*1024,		/* leave 128MB after appdata */
		.size = MTDPART_SIZ_FULL,			/* the rest */
	},
	{
		.name = "config",
		.offset = 0x80000,				/* from half of BootROM*/
		.size = 0x80000,				/* 512KB */
	},
	{
		.name = "swap",
		.offset = (512*1024*1024),		/* behind appdata */
		.size = (128*1024*1024),				/* 128 MB */
	},
};
#else
static const struct mtd_partition _parts[] =
{
#if 0
	{
		.name = "linux",
		.offset = 0,
		.size = 0x200000,				/* 2 MB */
		.mask_flags = MTD_WRITEABLE,	/* force read-only	*/
	},
	{
		.name = "cramfs",
		.offset = 0x200000,
		.size = 0x10000000,				/* 256MB */
//		.mask_flags = MTD_WRITEABLE,	/* force read-only	*/
	},
#endif
	{
		.name = "bootldr",
		.offset = 0,
		.size = 0x100000,				/* 1 MB */
		.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "linux",
		.offset = 0x100000,
		.size = 0x200000,				/* 2 MB */
		.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "rootfs",
#if 0
		.offset = 0xE00000,				/*boot rootdisk from block2*/
#else
		.offset = 0x300000,				/*boot rootdisk from block1*/
#endif
		.size = 0x800000,				/* 8 MB */
		.mask_flags = MTD_WRITEABLE,	/* force read-only      */
	},
	{
		.name = "user",
//		.offset = 0xB00000,
		.offset = 0x2100000,
		.size =	 0xC000000,				/* 192 MB */
	},

	{
		.name = "localdata",
//		.offset = 0xB00000,
		.offset = 0xE100000,
		.size =	 0x4000000,				/* 64 MB */
	},
	{
		.name = "data",
		.offset = MTDPART_OFS_NXTBLK,
		.size = MTDPART_SIZ_FULL,			/* the rest */
	},
};
#endif

static void sslnfc_tmr(unsigned long data)
{
	ssl_p	sslp = (ssl_p)data;

printk("nfc nand reset tout\n");
	nfc_init(sslp->hw);
	complete(&sslp->comp);
}


static int sslnfc_probe(struct platform_device *dev)
{
	sslnfc_ctx	*info;
	int err;
	int size;
	unsigned long	flags;
	int 			irq;
	struct mtd_info	*mtd;
	ssl_p			sslp;
	struct timer_list	tmr;
#if IO_MAP == 1
	struct resource	*rc = platform_device_resource(dev, IORESOURCE_MEM, 0);
	irq = platform_device_irq(dev, 0);
#else
	struct resource		 *rc = _rc;
	irq = _rc[1].start;
#endif

	if (!rc || irq == NO_IRQ)
	{
		dev_err(&dev->dev, "sslnfc_probe - irq\n");
		return -ENXIO;
	}

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
	{
		dev_err(&dev->dev, "sslnfc_probe - no memory for flash info\n");
		err = -ENOMEM;
		goto exit_error;
	}

	memzero(info, sizeof(*info));
	platform_set_drvdata(dev, info);

	/* acquire resources */

	spin_lock_init(&info->controller.lock);
	init_waitqueue_head(&info->controller.wq);

	/* allocate and map the resource */

#if IO_MAP == 1
	size = rc->end - rc->start + 1;
	if (!request_mem_region(rc->start, size, driver_name))
	{
		dev_err(&dev->dev, "sslnfc_probe - request_mem_region\n");
		err = -EBUSY;
		goto mem_region_error;
	}

	info->hw.r = ioremap_nocache(rc->start, size);
	if (!info->hw.r)
	{
		dev_err(&dev->dev, "sslnfc_probe - ioremap\n");
		err = -EBUSY;
		goto ioremap_error;
	}
#else
	info->hw.r = (void *)rc->start;
#endif

	/* allocate our information */
	size = sizeof(*(info->mtd));
	info->mtd = kmalloc(size, GFP_KERNEL);
	if (!info->mtd)
	{
		dev_err(&dev->dev, "sslnfc_probe - failed to allocate mtd storage\n");
		err = -ENOMEM;
		goto mtd_error;
	}
	memzero(info->mtd, size);

	spin_lock_irqsave(&info->controller.lock, flags);
	err = request_irq(irq, sslnfc_irq, IRQF_DISABLED,
				driver_name, &info->mtd->chip);
	if (err)
	{
		goto irq_error;
	}
	info->irq = irq;

	spin_unlock_irqrestore(&info->controller.lock, flags);

	sslnfc_initchip(info);
	mtd = &info->mtd->mtd;

	/* reset chip */
	sslp = (ssl_p)info->mtd->chip.priv;
	init_timer(&tmr);
	tmr.function = sslnfc_tmr;
	tmr.data = (long)sslp;
	mod_timer(&tmr, jiffies + HZ);
	
	
	init_completion(&sslp->comp);
	nfc_nand_reset(sslp->hw);
	wait_for_completion(&sslp->comp);	
	//nfc_polling(sslp->hw,NFC_ISR_CMP);	
	
	
	del_timer_sync(&tmr);

	err = nand_scan_ident(mtd, 1);
	if (!err)
	{
		sslnfc_cfg(mtd);
		err = nand_scan_tail(mtd);
	}
	if (!err)
	{
		err = add_mtd_partitions(mtd, _parts, ARRAY_SIZE(_parts));
		if (!err)
		{
			return 0;
		}
	}

	disable_irq(info->irq);
	free_irq(info->irq,&info->mtd->chip);
irq_error:
	kfree(info->mtd);
mtd_error:
#if IO_MAP == 1
	iounmap(info->hw.r);
#endif
ioremap_error:
#if IO_MAP == 1
	release_mem_region(rc->start, rc->end - rc->start + 1);
#endif
mem_region_error:
	kfree(info);
exit_error:
	if (!err)
	{
		err = -EINVAL;
	}
	return err;
}


#ifdef CONFIG_PM

static int sslnfc_suspend(struct platform_device *dev, pm_message_t pm)
{
	return 0;
}

static int sslnfc_resume(struct platform_device *dev)
{
	return 0;
}

#endif


static struct platform_driver sslnfc_drv = 
{
	.driver =
	{
		.name		= DRV_DESC,
		.owner		= THIS_MODULE,
	},
	.probe		= sslnfc_probe,
	.remove		= sslnfc_remove,
#ifdef CONFIG_PM
	.suspend	= sslnfc_suspend,
	.resume		= sslnfc_resume,
#endif
};


static int __init sslnfc_init(void)
{
#ifdef NFC_SIMPNP
	int rt = 0;
#endif
	printk("NAND Driver, (c) 2007 Solomon Systech\n");

#ifdef NFC_SIMPNP
	_rc[0].start = addr;
	_rc[0].end = addr + 0x100000 - 1;
	_rc[0].flags = IORESOURCE_MEM;
	_rc[1].start = irq;
	_rc[1].end = irq;
	_rc[1].flags = IORESOURCE_IRQ;

	rt = platform_device_register(&dv);
	if (rt)
	{
		NAND_DRV_ERR("ssl_nfc: init err - plat_dev_reg\n");
		return rt;
	}
	NAND_DRV_INFO("addr=%08X irq=%d flag=%08X\n", addr, irq, flag);
#endif

	return platform_driver_register(&sslnfc_drv);
}

static void __exit sslnfc_exit(void)
{
	NAND_DRV_INFO("sslnfc_exit\n");
	platform_driver_unregister(&sslnfc_drv);
#ifdef NFC_SIMPNP
	platform_device_unregister(&dv);
#endif
}

module_init(sslnfc_init);
module_exit(sslnfc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tarun Bajaj");
MODULE_DESCRIPTION("SSL MTD NAND driver");
