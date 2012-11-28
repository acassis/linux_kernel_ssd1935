/**
 * @file	ssl_fb.c 
 * @author	sasin@solomon-systech.com
 * @version	0.0.5
 * @date	070927
 * 
 * Linux framebuffer driver for LCDC.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>

#include <linux/cpufreq.h>
#include <asm/clock.h>
#include <asm/gpio.h>

#include "lcdc.h"
#include "sslfb.h"

#ifdef CONFIG_CPU_FREQ
static struct notifier_block   freq_transition;
static struct notifier_block   freq_policy;
#endif

#if defined CONFIG_ACCIO_CM5210 || defined CONFIG_ACCIO_A2818T
#define GPIO_LCD_POWER		GPIO_NUM(2, 9)	//the gpio support to 5V voltage to LCD
#elif defined CONFIG_ACCIO_P1_SK01
#define GPIO_LCD_POWER		GPIO_NUM(3, 2)	//the gpio support to 5V voltage to LCD 
#endif

#ifdef CONFIG_ACCIO_LCM_CPT480X272
static int lcmspi_suspend(struct spi_device *spi);
static int lcmspi_resume(struct spi_device *spi);
struct lcmspi
{
	struct spi_device *spi;
	unsigned int lcm_suspend_cmd[4];
	unsigned int lcm_resume_cmd[4];
};
struct spi_device *lcm_spi_ctrl;
#endif

typedef struct
{
	struct device	*dev;		/**< device */
	lcdc_flt_t		cfg;		/**< window configuration */
	int				wid;		/**< 0:main, 1,2:floating */
	uint16_t		vheight;	/**< virtual window height */
	uint32_t		offset;		/**< physical byte offset into virtual window */
	uint8_t			enable;
}
sslfb_par;

typedef struct
{
	lcdc_t			hw;
	uint32_t		effect;
	int				fps;
	sslfb_cursor_t	cursor[2];
	sslfb_par		*par[3];
}
sslfb_t;

static sslfb_t	sslfb;


//#define SSLFB_SIMPNP
#define DRV_NAME "lcdc"

#ifdef SSLFB_SIMPNP
#define DRV_ID_TOTAL 3
#endif

#ifdef CONFIG_CPU_FREQ
/*
 *  * CPU clock speed change handler.  We need to adjust the LCD timing
 *   parameters when the CPU clock is adjusted by the power management
 *   subsystem.
 * 
 *   TODO: Determine why f->new != 10*get_lclk_frequency_10khz()
 *  */
static int
sslfb_freq_transition(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *f = data;
	unsigned int value = magus_get_hclk();
	lcdc_start(&sslfb.hw, sslfb.fps, value);
	return 0;
}

static int
sslfb_freq_policy(struct notifier_block *nb, unsigned long val, void *data)
{
	return 0;
}
#endif

static int var_to_par(sslfb_par *par, struct fb_var_screeninfo *var)
{
	lcdc_win_p	win;
	uint32_t	v;
	int			i;

	win = &par->cfg.w;
	win->a.width = var->xres;
	win->stride = var->xres_virtual;
	par->vheight = var->xres_virtual;
	win->a.height = var->yres;
	v = var->bits_per_pixel;
	if (v == 15)
	{
		v = 16;
	}
	for (i = -1; v; i++)
	{
		v >>= 1;
	}
	win->pixel = i;
	par->cfg.x = var->xoffset;
	par->cfg.y = var->yoffset;
	if (!par->wid)
	{
		sslfb.effect = (sslfb.effect & ~3) | var->rotate;
	}
	return 0;
}


static int sslfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	sslfb_par	par;
	int		 	rt;

	par.wid = ((sslfb_par *)info->par)->wid;
	rt = var_to_par(&par, var);
	if (rt)
	{
		return rt;
	}
	return 0;
}


static int sslfb_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct fb_fix_screeninfo *fix = &info->fix;
	sslfb_par	*par = (sslfb_par *)info->par;
	int		 	rt;

	rt = var_to_par(par, var);
	if (rt)
	{
		return -1;
	}

	rt = var->xres << (par->cfg.w.pixel - 3);
	fix->line_length = rt;
	rt *= var->yres;
	fix->smem_len = rt;
	rt = PAGE_ALIGN(rt);
	info->screen_size = rt;

	rt = lcdc_win_cfg(&sslfb.hw, par->wid, (lcdc_cfg_p)&par->cfg);
	if (rt)
	{
		dev_err(info->dev, "set_par: set window err %d\n", rt);
		return -1;
	}

	if (!par->wid)
	{
		rt = lcdc_effect(&sslfb.hw, sslfb.effect); 
		if (rt)
		{
			dev_err(info->dev, "set_par: set rotate err %d\n", rt);
			return -1;
		}
	}

	return 0;
}


static int sslfb_setcolreg(unsigned regno, unsigned red, unsigned green,
	unsigned blue, unsigned transp, struct fb_info *info)
{
	uint32_t	d;

	d = (transp << 24) | (red << 16) | (green << 8) | blue;
	lcdc_set_color(&sslfb.hw, regno, &d, 1);
	return 0;
}


static int sslfb_blank(int blank, struct fb_info *info)
{
/* cody: comment for now. prevent X from suspending lcd */
#if 0
	sslfb_par *par = (sslfb_par *)(info->par);

	par->enable = (blank == FB_BLANK_UNBLANK);
	if (par->wid)
	{
		lcdc_win_ena(&sslfb.hw, par->wid, (blank == FB_BLANK_UNBLANK));
	}
	else
	{
		lcdc_suspend(&sslfb.hw, (blank != FB_BLANK_UNBLANK));
	}
#endif
	return 0;
}


static int sslfb_pan_display(struct fb_var_screeninfo *var, 
	struct fb_info *info)
{
	sslfb_par *par = (sslfb_par *)info->par;

	par->offset = (var->xres_virtual * var->yoffset + var->xoffset)
					<< (par->cfg.w.pixel - 3);
	par->cfg.w.a.addr = info->fix.smem_start + par->offset;
	lcdc_win_cfg(&sslfb.hw, par->wid, (lcdc_cfg_p)&par->cfg);
	return 0;
}


static int var_from_par(struct fb_var_screeninfo *var, sslfb_par *par)
{
	lcdc_win_p	win;
	uint32_t	v;

	var->yres_virtual = par->vheight;
	win = &par->cfg.w;
	var->xres_virtual = win->stride;
	var->xres = win->a.width;
	var->yres = win->a.height;
	var->xoffset = par->cfg.x;
	var->yoffset = par->cfg.y;
	v = 1 << win->pixel;
	var->bits_per_pixel = v;
	if (v == 16)
	{
		var->red.length = var->blue.length = 5;
		var->green.length = 6;
		var->red.offset = 11;
		var->green.offset = 5;
	}
	else if (v == 32)
	{
		var->red.length = var->green.length = var->blue.length = 8;
		var->red.offset = 16;
		var->green.offset = 8;
	}
	var->blue.offset = 0;
	var->grayscale = 0;
	var->nonstd = 0;
	if (!par->wid)
	{
		sslfb_plt_t	*plt;

		plt = (sslfb_plt_t *)par->dev->platform_data;
		var->rotate = sslfb.effect & 3;
		var->left_margin = plt->panel.hdisp_pos - 
			(plt->panel.hsync_len + plt->panel.hsync_pos - 1);
		var->right_margin = plt->panel.hdisp_len - 
			(plt->panel.hdisp_pos - plt->win.a.width);
		var->upper_margin = plt->panel.vdisp_pos - 
			(plt->panel.vsync_len + plt->panel.vsync_pos - 1);
		var->lower_margin = plt->panel.vdisp_len - 
			(plt->panel.vdisp_pos - plt->win.a.height);
		var->hsync_len = plt->panel.hsync_len;
		var->vsync_len = plt->panel.vsync_len;
		var->sync = (plt->panel.hsync_high ? FB_SYNC_HOR_HIGH_ACT : 0) |
			(plt->panel.vsync_high ? FB_SYNC_VERT_HIGH_ACT : 0);
		/* ensure no 32-bit overflow, use 4Giga instead of 1Tera */
#ifdef CONFIG_SERIAL_TFT 
		v = 1000000000UL /
			(plt->panel.hdisp_len * plt->panel.vdisp_len * sslfb.fps);
		var->pixclock = v * 1024 / 3;
#else
		/* ensure no 32-bit overflow, use 4Giga instead of 1Tera */
		v = 4000000000UL / 
			(plt->panel.hdisp_len * plt->panel.vdisp_len * sslfb.fps);
		var->pixclock = v * 250;	/* nano-seconds */
#endif
	}

	return 0;
}

#define GPIO_PANEL_REST		GPIO_NUM(3, 23)
#define GPIO_PANEL_CS		GPIO_NUM(3, 19)
#define GPIO_PANEL_CLK		GPIO_NUM(3, 20)
#define GPIO_PANEL_MISO		GPIO_NUM(3, 21)

void Panel_SetReg(int reg, int data)
{
 	int bitNum;

	reg |= 0x700000;
	data |= 0x720000;
	gpio_set_value(GPIO_PANEL_CS, 0);
	for(bitNum=23;bitNum>=0;bitNum--)
	{
		gpio_set_value(GPIO_PANEL_MISO, (reg&(0x1<<bitNum)) != 0);
		gpio_set_value(GPIO_PANEL_CLK, 0);
		gpio_set_value(GPIO_PANEL_CLK, 1);
	}
	gpio_set_value(GPIO_PANEL_CS, 1);
	gpio_set_value(GPIO_PANEL_CS, 0);
	for(bitNum=23;bitNum>=0;bitNum--)
	{
		gpio_set_value(GPIO_PANEL_MISO, (data&(0x1<<bitNum)) != 0);
		gpio_set_value(GPIO_PANEL_CLK, 0);
		gpio_set_value(GPIO_PANEL_CLK, 1);
	}
	gpio_set_value(GPIO_PANEL_CS, 1);	
}
void init_panel()
{
	gpio_direction_output(GPIO_PANEL_REST, 	1); 
	gpio_direction_output(GPIO_PANEL_CS, 	1); 
	gpio_direction_output(GPIO_PANEL_CLK, 	1); 
	gpio_direction_output(GPIO_PANEL_MISO, 	1); 
	
	Panel_SetReg(0x0001, 0x6300);
	Panel_SetReg(0x0004, 0x07c7);
	Panel_SetReg(0x0005, 0xbc84);
	
	gpio_direction_output(GPIO_NUM(5,10), 1);
	//gpio_set_value(GPIO_NUM(5,10), 1);
}


static void sslfb_get_caps(struct fb_info *info, 
				struct fb_blit_caps *caps, struct fb_var_screeninfo *var)
{
	caps->x = ~(u32)0;
	caps->y = ~(u32)0;
	caps->len = ~(u32)0;
}


static int sslfb_ioctl(struct fb_info *info, 
				unsigned int cmd, unsigned long arg)
{
	sslfb_par *par = (sslfb_par *)info->par;
	sslfb_win_t	win;

	switch (cmd)
	{
		case SSLFBIOGET_WIN:
			if (!par->wid)
			{
				return -EINVAL;
			}
			win.x = par->cfg.x;
			win.y = par->cfg.y;
			win.alpha = par->cfg.alpha;
			return copy_to_user((void __user *)arg, &win, sizeof(win));

		case SSLFBIOSET_WIN:
		{
			int	rt;

			if (!par->wid)
			{
				return -EINVAL;
			}
			rt = copy_from_user(&win, (void __user *)arg, sizeof(win));
			if (rt)
			{
				return rt;
			}
			par->cfg.x = win.x;
			par->cfg.y = win.y;
			par->cfg.alpha = win.alpha;
			lcdc_win_cfg(&sslfb.hw, par->wid, (lcdc_cfg_p)&par->cfg);
			return 0;
		}

		case SSLFBIOGET_EFF:
			if (par->wid)
			{
				return -EINVAL;
			}
			return copy_to_user((void __user *)arg, &sslfb.effect, 4);

		case SSLFBIOSET_EFF:
		{
			int	rot;

			if (par->wid)
			{
				return -EINVAL;
			}
			if (arg == sslfb.effect)
			{
				return 0;
			}
			rot = sslfb.effect & 3;
			sslfb.effect = arg;
			lcdc_effect(&sslfb.hw, sslfb.effect);
			if (rot != (arg & 3))
			{
				/* rotation changed, must reconfigure windows' addresses */
				for (rot = 0; rot < 3; rot++)
				{
					if (sslfb.par[rot])
					{
						lcdc_win_cfg(&sslfb.hw, sslfb.par[rot]->wid, 
							(lcdc_cfg_p)&sslfb.par[rot]->cfg);
					}
				}
			}
			return 0;
		}

		case SSLFBIOGET_CSR:
		case SSLFBIOGET_CSR2:
			cmd -= SSLFBIOGET_CSR;
			if (par->wid || cmd > 1)
			{
				return -EINVAL;
			}
			return copy_to_user((void __user *)arg, 
						&sslfb.cursor[cmd], sizeof(sslfb.cursor[cmd]));

		case SSLFBIOSET_CSR:
		case SSLFBIOSET_CSR2:
		{
			int	rt;
			sslfb_cursor_t	*csr;

			cmd -= SSLFBIOSET_CSR;
			if (par->wid || cmd > 1)
			{
				return -EINVAL;
			}
			csr = &sslfb.cursor[cmd];
			rt = copy_from_user(csr, (void __user *)arg, sizeof(*csr));
			if (rt)
			{
				return rt;
			}
			if (csr->enable)
			{
				lcdc_win_cfg(&sslfb.hw, cmd + 3, (lcdc_cfg_p)&csr->win);
			}
			lcdc_win_ena(&sslfb.hw, cmd + 3, csr->enable);
			return 0;
		}
	}
	return -EINVAL;
}


static struct fb_ops sslfb_ops = 
{
	.owner			= THIS_MODULE,
	.fb_check_var	= sslfb_check_var,
	.fb_get_caps	= sslfb_get_caps,
	.fb_set_par		= sslfb_set_par,
	.fb_setcolreg	= sslfb_setcolreg,
	.fb_blank		= sslfb_blank,
	.fb_pan_display = sslfb_pan_display,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_ioctl		= sslfb_ioctl,
};

static int sslfb_suspend(struct platform_device *dev, pm_message_t state);
static int sslfb_resume(struct platform_device *dev);

static ssize_t sslfb_show(struct device *dev,
		                     struct device_attribute *attr, char *buf)
{
    struct fb_info *fb = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", fb->dev->power.power_state.event);
}

static ssize_t sslfb_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    char *endp;
    int i;
	struct platform_device *pdev;

    struct fb_info *fb = dev_get_drvdata(dev);

	i = simple_strtol(buf, &endp, 10);
	// no need to update power state.
	if(i == fb->dev->power.power_state.event)
	{
		return count;
	}

	pdev = container_of(dev, struct platform_device, dev);

	fb->dev->power.power_state.event = i;
	if(i == PM_EVENT_SUSPEND)
	{
		sslfb_suspend(pdev, PMSG_SUSPEND);
	}
	else if(i == PM_EVENT_ON)
	{		
		sslfb_resume(pdev);
	}
	else
	{
		fb->dev->power.power_state.event = 0;
	}

	return count;
}
static DEVICE_ATTR(pm_ctrl, 0664, sslfb_show, sslfb_store);

static struct attribute *sslfb_attributes[] = {
	&dev_attr_pm_ctrl.attr,
	NULL,
};
static struct attribute_group sslfb_attr_group = {
	.attrs = sslfb_attributes,
};

static int sslfb_probe(struct platform_device *dev)
{
	int	rt;
	struct fb_info	*info;
	sslfb_par		*par;
	sslfb_plt_t		*plt;
	struct fb_var_screeninfo	*var;
	struct fb_fix_screeninfo	*fix;

	/* create a new fb structure */
	info = framebuffer_alloc(sizeof(sslfb_par), &dev->dev);
	if (!info)
	{
		dev_err(&dev->dev, "probe err - framebuffer_alloc\n");
		return -ENOMEM;
	}
	var = &info->var;
	fix = &info->fix;
	par = (sslfb_par *)info->par;

	/* build sysfs */
	int err = sysfs_create_group(&dev->dev.kobj, &sslfb_attr_group);
	if (err)
	{
		printk("register sysfs for power is failure.\n");
	}

	/* fb operation function pointers */
	info->fbops = &sslfb_ops;
	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_HWACCEL_ROTATE
					| FBINFO_HWACCEL_XPAN | FBINFO_HWACCEL_YPAN;

	/* init par info */
	memset(par, 0, sizeof(sslfb_par));
	par->dev = &dev->dev;
	par->wid = dev->id;
	sslfb.par[par->wid] = par;

	/* platform config */
	plt = (sslfb_plt_t *)par->dev->platform_data;
	if (!plt)
	{
		dev_err(&dev->dev, "probe err - no platform data\n");
		rt = -ENODEV;
		goto l_plt;
	}
	par->vheight = plt->vheight;
	if (!par->wid)
	{
		par->cfg.w = plt->win;
		sslfb.effect = plt->effect;
		sslfb.fps = plt->fps;
	}
	else
	{
		sslfbw_plt_t	*flt;

		flt = (sslfbw_plt_t *)plt;
		par->cfg = flt->win;
		par->enable = flt->enable;
	}
	/* fill in defaults */
	if (!par->vheight)
	{
		par->vheight = par->cfg.w.a.height;
	}
	if (!par->cfg.w.stride)
	{
		par->cfg.w.stride = par->cfg.w.a.width;
	}

	/* init var screen info */
	var_from_par(var, par);

	/* init fix screen info */
	strcpy(fix->id, DRV_NAME);
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->accel = FB_ACCEL_NONE;
	fix->xpanstep =	1;
	fix->ypanstep =	1;
	fix->ywrapstep = 1;
	fix->line_length = var->xres_virtual * (var->bits_per_pixel >> 3);
	fix->smem_len = var->yres_virtual * fix->line_length + plt->extramem;

	/* alloc memory and update it to info, fix & par */
	if (!par->cfg.w.a.addr)
	{
		dma_addr_t	dma;

		info->screen_base = dma_alloc_writecombine(par->dev, fix->smem_len, 
								&dma, GFP_KERNEL);
		fix->smem_start = par->cfg.w.a.addr = dma;
	}
	else
	{
		fix->smem_start = par->cfg.w.a.addr;
		info->screen_base = ioremap_nocache(fix->smem_start, fix->smem_len);

		//memset(info->screen_base, 0, fix->smem_len); // clear the framebuffer content
	}
	if (!info->screen_base)
	{
		dev_err(&dev->dev, "probe err - map/alloc fb\n");
		rt = -ENOMEM;
		goto l_mem;
	}
	info->screen_size = fix->smem_len;
#if 0
#ifdef CONFIG_ARCH_MAGUS_ACCIO
	/* fill in buffer with 0x0 */
	memset_io(info->screen_base, 0, info->screen_size);
#endif
#endif

	if (!par->wid)
	{
		struct resource	*rc;

		rc = platform_get_resource(dev, IORESOURCE_MEM, 0);
		if (!rc)
		{
			dev_err(&dev->dev, "probe err - get_rc\n");
			rt = -ENOMEM;
			goto l_rc;
		}
		sslfb.hw.r = ioremap_nocache(rc[0].start, rc[0].end - rc[0].start + 1);
		if (!sslfb.hw.r)
		{
			dev_err(&dev->dev, "probe err - ioremap\n");
			rt = -ENOMEM;
			goto l_map;
		}

		rt = lcdc_init(&sslfb.hw);
		if (rt)
		{
			dev_err(&dev->dev, "probe err - init %d\n", rt);
			rt = -ENODEV;
			goto l_init;
		}
	}

	lcdc_win_cfg(&sslfb.hw, par->wid, (lcdc_cfg_p)&par->cfg);
	if (!par->wid)
	{
		lcdc_panel_cfg(&sslfb.hw, &plt->panel);
		lcdc_start(&sslfb.hw, plt->fps, MAGUS_CLK_LCDC);
	}
	else{
		if (par->enable)
		{	
			printk("SSL_FB: enable wid: %d\n", par->wid);
			lcdc_win_ena(&sslfb.hw, par->wid, 1);
		}
		else{
			printk("SSL_FB: disable wid: %d\n", par->wid);
			lcdc_win_ena(&sslfb.hw, par->wid, 0);
		}
	}

	/* register fb and request for dynamic minor number */
	rt = register_framebuffer(info);
	if (rt < 0)
	{
		dev_err(&dev->dev, "probe err - register_framebuffer %d\n", rt);
		goto l_regfb;
	}

	platform_set_drvdata(dev, info);
	return 0;

l_regfb:
	lcdc_exit(&sslfb.hw);
l_init:
	iounmap(sslfb.hw.r);
l_map:
l_rc:
l_mem:
l_plt:
	framebuffer_release(info);
	return rt;
}


static int sslfb_remove(struct platform_device *dev)
{
	struct fb_info	*info = platform_get_drvdata(dev);
	sslfb_par		*par = (sslfb_par *)info->par;

	sslfb.par[par->wid] = 0;
	if (par->wid)
	{
		lcdc_win_ena(&sslfb.hw, par->wid, 0);
	}
	else
	{
		lcdc_exit(&sslfb.hw);
		iounmap(sslfb.hw.r);
	}
	if (info->screen_base)
	{
		if (!((sslfb_plt_t *)info->dev->platform_data)->win.a.addr)
		{
			dma_free_writecombine(info->dev, info->screen_size,
				info->screen_base, (dma_addr_t)info->fix.smem_start);
		}
		else
		{
			iounmap((void *)info->screen_base);
		}
	}

	sysfs_remove_group(&dev->dev.kobj, &sslfb_attr_group);

	unregister_framebuffer(info);
	framebuffer_release(info);
	platform_set_drvdata(dev, 0);
	return 0;
}


//#ifdef CONFIG_PM
#if 1
static int sslfb_suspend(struct platform_device *dev, pm_message_t state)
{
	#if defined CONFIG_ACCIO_CM5210 || defined CONFIG_ACCIO_A2818T || defined CONFIG_ACCIO_P1_SK01
	gpio_direction_output(GPIO_LCD_POWER, 0); //disable 5V voltage to LCD
	#endif
	#ifdef CONFIG_ACCIO_LCM_CPT480X272
	lcmspi_suspend(lcm_spi_ctrl);
	msleep(50);	
	#endif
	lcdc_suspend(&sslfb.hw, 1);
	return 0;
}


static int sslfb_resume(struct platform_device *dev)
{
	#if defined CONFIG_ACCIO_CM5210 || defined CONFIG_ACCIO_A2818T || defined CONFIG_ACCIO_P1_SK01
	gpio_direction_output(GPIO_LCD_POWER, 1); //enable 5V voltage to LCD
	#endif
	lcdc_suspend(&sslfb.hw, 0);
	#ifdef CONFIG_ACCIO_LCM_CPT480X272
	msleep(50);
	lcmspi_resume(lcm_spi_ctrl);
	init_panel();
	msleep(180);	
	#endif
	return 0;
}

#endif


static void sslfb_shutdown(struct platform_device * dev)
{
	lcdc_exit(&sslfb.hw);
}


#ifdef SSLFB_SIMPNP 

static u64 sslfb_dma_mask = ~(u64)0;
static void sslfb_devs_release_main_window(struct device * dev) {}

static struct resource sslfb_resources_main_window[] =
{
	[0] = 
	{
		.start	= MAGUS_IO_LCDC,
		.end	= MAGUS_IO_LCDC + 0x1000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = 
	{
		.start	= MAGUS_IRQ_LCDC,
		.end	= MAGUS_IRQ_LCDC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sslfb_devs[] = 
{
	{
		.name	= DRV_NAME,
		.id		= 0,
		.dev	= 
		{
			.dma_mask = &sslfb_dma_mask,
			.coherent_dma_mask = -1UL,
			.release = sslfb_devs_release_main_window,
		},
		.num_resources	= ARRAY_SIZE(sslfb_resources_main_window),
		.resource		= sslfb_resources_main_window,
	},
	{
		.name	= DRV_NAME,
		.id		= 1,
		.dev	=
		{
			.dma_mask = &sslfb_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.release = sslfb_devs_release_main_window, 
		},
		.num_resources = 0,
	},
	{
		.name	= DRV_NAME,
		.id		= 2,
		.dev	=
		{
			.dma_mask = &sslfb_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.release = sslfb_devs_release_main_window,
		},
		.num_resources = 0,
	},
};


static int sslfb_platform_register(void)
{
	int i;

	for (i = 0; i < DRV_ID_TOTAL; i++)
	{
		int	rt;

		rt = platform_device_register(&sslfb_devs[i]);
		if (rt)
		{
			return rt;
		}
	}
	return 0;
}


static void sslfb_platform_unregister(void)
{
	int i;

	i = DRV_ID_TOTAL;
	while (i--)
	{
		platform_device_unregister(&sslfb_devs[i]);
	}
}

#endif /* SSLFB_SIMPNP */


static struct platform_driver sslfb_drv = 
{
	.driver =
	{
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= sslfb_probe,
	.remove		= sslfb_remove,
	.shutdown	= sslfb_shutdown,
#ifdef CONFIG_PM
	.suspend	= sslfb_suspend,
	.resume		= sslfb_resume,
#endif
};


#ifdef CONFIG_ACCIO_LCM_CPT480X272

static int lcmspi_suspend(struct spi_device *spi)
{	
	unsigned char spi_tx_buf[3];
	unsigned int index;
	struct lcmspi *lcmspi_dev = dev_get_drvdata(&spi->dev);
		
	for (index = 0; index < 4; index++)
	{
		spi_tx_buf[0] = (lcmspi_dev->lcm_suspend_cmd[index] >> 16) & 0xFF;
		spi_tx_buf[1] = (lcmspi_dev->lcm_suspend_cmd[index] >> 8) & 0xFF;
		spi_tx_buf[2] = lcmspi_dev->lcm_suspend_cmd[index] & 0xFF;	
		if ( spi_write(spi, spi_tx_buf, 3) != 0 )
		{
			return -1;	
		}
		printk(KERN_DEBUG "lcm_spi_driver: write 0x%02x%02x%02x to SPI bus\n", spi_tx_buf[0], spi_tx_buf[1], spi_tx_buf[2]);
	}
	return 0;
}


static int lcmspi_resume(struct spi_device *spi)
{
	unsigned char spi_tx_buf[3];
	unsigned int index;
	struct lcmspi *lcmspi_dev = dev_get_drvdata(&spi->dev);
	
	for (index = 0; index < 4; index++)
	{
		spi_tx_buf[0] = (lcmspi_dev->lcm_resume_cmd[index] >> 16) & 0xFF;
		spi_tx_buf[1] = (lcmspi_dev->lcm_resume_cmd[index] >> 8) & 0xFF;
		spi_tx_buf[2] = lcmspi_dev->lcm_resume_cmd[index] & 0xFF;	
		if ( spi_write(spi, spi_tx_buf, 3) != 0 )
		{
			return -1;	
		}
		printk(KERN_DEBUG "lcm_spi_driver: write 0x%02x%02x%02x to SPI bus\n", spi_tx_buf[0], spi_tx_buf[1], spi_tx_buf[2]);
	}
	return 0;
}


static int __devexit lcmspi_remove(struct spi_device *spi)
{
	struct lcmspi *lcmspi_dev = dev_get_drvdata(&spi->dev);
	
	dev_set_drvdata(&spi->dev, NULL);
	kfree(lcmspi_dev); 
	return 0;
}


static int __devinit lcmspi_probe(struct spi_device *spi)
{
	int err;
	struct lcmspi *lcmspi_device;	
	/*
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	err = spi_setup(spi);
	if (err < 0)
		return err;
	*/	
	lcmspi_device = kzalloc(sizeof(struct lcmspi), GFP_KERNEL);
	if (!lcmspi_device)
	{
		printk(KERN_DEBUG "sslfb: lcmspi_device kzalloc error\n");
		return -ENOMEM;
	}
	lcmspi_device->spi = spi;
	lcmspi_device->lcm_resume_cmd[0] = 0x00700000 | 0x28;
	lcmspi_device->lcm_resume_cmd[1] = 0x00720000 | 0x0006;
	lcmspi_device->lcm_resume_cmd[2] = 0x00700000 | 0x2D;
	lcmspi_device->lcm_resume_cmd[3] = 0x00720000 | 0x3F40;
	lcmspi_device->lcm_suspend_cmd[0] = 0x00700000 | 0x28;
	lcmspi_device->lcm_suspend_cmd[1] = 0x00720000 | 0x0006;
	lcmspi_device->lcm_suspend_cmd[2] = 0x00700000 | 0x2D;
	lcmspi_device->lcm_suspend_cmd[3] = 0x00720000 | 0x3F46;	
	dev_set_drvdata(&spi->dev, lcmspi_device);
	lcm_spi_ctrl = spi;
	printk(KERN_DEBUG "lcmspi_probe completed\n");
	return 0;
}


static struct spi_driver lcmspi_driver = {
	.driver = {
		.name	= "lcmspi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= lcmspi_probe,
	.remove		= __devexit_p(lcmspi_remove),	
	.suspend	= lcmspi_suspend,
	.resume		= lcmspi_resume,
};

#endif


static int __init sslfb_init(void)
{
	int rt;
	init_panel();
#ifdef SSLFB_SIMPNP
	rt = sslfb_platform_register();
	if (rt)
	{
		printk("sslfb: init err - plat_reg %d\n", rt);
		return rt;
	}
#endif
	rt = platform_driver_register(&sslfb_drv);
	if (rt)
	{
		printk("sslfb: init err - drv_reg %d\n", rt);
		return rt;
	}
#ifdef CONFIG_CPU_FREQ
	freq_transition.notifier_call = sslfb_freq_transition;
	freq_policy.notifier_call = sslfb_freq_policy;
	cpufreq_register_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	cpufreq_register_notifier(&freq_policy, CPUFREQ_POLICY_NOTIFIER);
#endif
#ifdef CONFIG_ACCIO_LCM_CPT480X272
	rt = spi_register_driver(&lcmspi_driver);
	if (rt)
	{
		printk("sslfb: init err - spi_reg %d\n", rt);
		return rt;
	}
#endif
	return 0;
}


static void __exit sslfb_exit(void)
{
	platform_driver_unregister(&sslfb_drv);

#ifdef CONFIG_CPU_FREQ
	cpufreq_unregister_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	cpufreq_unregister_notifier(&freq_policy, CPUFREQ_POLICY_NOTIFIER);
#endif

#ifdef SSLFB_SIMPNP 
	sslfb_platform_unregister();
#endif
#ifdef CONFIG_ACCIO_LCM_CPT480X272
	spi_unregister_driver(&lcmspi_driver);
#endif
}


module_init(sslfb_init);
module_exit(sslfb_exit);

MODULE_AUTHOR("sasin@solomon-systech.com");
MODULE_DESCRIPTION("Framebuffer driver for SSL LCDC");
MODULE_LICENSE("GPL");

