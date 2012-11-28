/*
 * arch/arm/mach-magus/magus.c
 *
 * Copyright (c) 2006 sasin@solomon-systech.com
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/gpio.h>
#include <asm/clock.h>
#include "map.h"
#include "os.h"

#include "magus.h"

/* platform data */

#ifdef CONFIG_ARCH_MAGUS_ADS


#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/spi/eeprom.h>
#include "lcdm_lb070.h"


static const struct spi_eeprom	at25 = 
{
	.byte_len	= 2048,
	.name		= "at25160n",
	.page_size	= 32,
	.flags		= EE_ADDR2,
};


static int ads7846_pendown(void) 
{
	return !gpio_get_value(MAGUS_GPIO_ADS7846);
}

static const struct ads7846_platform_data	ads7846 =
{
	.x_max			= 0x0fff,
	.y_max			= 0x0fff,
	.x_plate_ohms	= 180,
	.pressure_max	= 255,
	.debounce_max	= 10,
	.debounce_tol	= 3,
	.get_pendown_state = ads7846_pendown,
};

static struct spi_board_info _spi[] __initdata =
{
	{
		.modalias		= "WM8731",
		.bus_num		= 0,
		.chip_select	= 0,
		.max_speed_hz	= 100000,
		.mode			= SPI_MODE_3,
	},
	{
		.modalias		= "at25",
		.bus_num		= 0,
		.chip_select	= 1,
		.max_speed_hz	= 625000,
		.platform_data	= &at25,
	},
	{
		.modalias		= "ads7846",
		.bus_num		= 0,
		.chip_select	= 2,
		.max_speed_hz	= 2500000,
		.irq			= gpio_to_irq(MAGUS_GPIO_ADS7846),
		.platform_data	= &ads7846,
	},
};

#endif

/* frame buffer */
_PNP_RC_AIP(lcdc, MAGUS_IO_LCDC, 0x1000, MAGUS_IRQ_LCDC, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN )
};
_PNP_DEVPR(lcdc, "lcdc", 0, &lcdc_main);
_PNP_DEVP(lcdcw1, "lcdc", 1, &lcdc_flt[0]);
_PNP_DEVP(lcdcw2, "lcdc", 2, &lcdc_flt[1]);


static struct platform_device *devices[] = 
{
//	&ehci,
//	&usbd,
//	&otg,
	&sdhc,
	&sdhc2,
	&spi,
//	&spi2,
	&i2c,
	&rtc,
	&wdog,
	&kpp,
	&lcdc,
	&lcdcw1,
	&lcdcw2,
	&tvout,
	&vpp,
	&vip,
	&d2d,
	&nfc,
};


static void __init magus_init(void)
{
	int i=0, j=0, pin_num=0;
	struct resource *p;

	/* setup & configure the pin mux per device */
	for( i=0; i < ARRAY_SIZE(devices); i++ ) 
	{ 
		/* for each device */
		for(j=0; j < devices[i]->num_resources; j++)  
		{ 
			/* for each resource entry */
			p = &devices[i]->resource[j];
			if(p->flags & IORESOURCE_GPIO) 
			{
				/* for gpio resources */
				for(pin_num=0; pin_num < DEVICE_MAX_PINS; pin_num++) 
				{
					if( p->gpio_pins[pin_num] != FREE_PIN ) 
						/* if the pin needs allocation */
						if( gpio_fn(p->gpio_pins[pin_num]) < 0 )
							dbg("magus_init: pin mux error for device '%s', _pin%d ", devices[i]->name, pin_num);

				}
			}
		}
	}

	platform_add_devices(devices, ARRAY_SIZE(devices));
#ifdef CONFIG_ARCH_MAGUS_ADS
	spi_register_board_info(_spi, ARRAY_SIZE(_spi));
#endif
}


static struct map_desc magus_io[] __initdata = 
{
#ifdef CONFIG_ARCH_MAGUS_FPGA
	{
		.virtual	= 0xF0000000,
		.pfn		= __phys_to_pfn(0x10000000),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
#else
	{	/* SCRM */
		.virtual	= MAGUS_VIO_SCRM,
		.pfn		= __phys_to_pfn(MAGUS_IO_SCRM),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
#endif
	{	/* INTC */
		.virtual	= MAGUS_VIO_INTC,
		.pfn		= __phys_to_pfn(MAGUS_IO_INTC),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* UART1 */
		.virtual	= MAGUS_VIO_UART,
		.pfn		= __phys_to_pfn(MAGUS_IO_UART),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* UART2 */
		.virtual	= MAGUS_VIO_UART2,
		.pfn		= __phys_to_pfn(MAGUS_IO_UART2),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* UART3 */
		.virtual	= MAGUS_VIO_UART3,
		.pfn		= __phys_to_pfn(MAGUS_IO_UART3),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* UART4 */
		.virtual	= MAGUS_VIO_UART4,
		.pfn		= __phys_to_pfn(MAGUS_IO_UART4),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* TMR1 */
		.virtual	= MAGUS_VIO_TMR,
		.pfn		= __phys_to_pfn(MAGUS_IO_TMR),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
{	/* TMR2 */
	.virtual	= MAGUS_VIO_TMR + 0x1000,
	.pfn		= __phys_to_pfn(MAGUS_IO_TMR2),
	.length		= SZ_4K,
	.type		= MT_DEVICE
},
	{	/* GPIO */
		.virtual	= MAGUS_VIO_GPIO,
		.pfn		= __phys_to_pfn(MAGUS_IO_GPIO),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{	/* DMAC */
		.virtual	= MAGUS_VIO_DMAC,
		.pfn		= __phys_to_pfn(MAGUS_IO_DMAC),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{   /* SDRAMC */
	    .virtual    = MAGUS_VIO_SDRAMC,
	    .pfn        = __phys_to_pfn(MAGUS_IO_SDRAMC),
	    .length     = SZ_4K,
	    .type       = MT_DEVICE
	},
#ifdef CONFIG_ARCH_MAGUS_ADS
	{	/* Ethernet on CS4 */
		.virtual	= MAGUS_VIO_CS8900,
		.pfn		= __phys_to_pfn(MAGUS_IO_CS4),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
#endif
};


static void __init magus_mapio(void)
{
	iotable_init(magus_io, ARRAY_SIZE(magus_io));
}


#if 0
/*
framebuffer switcher modes:
mode	fb0		fb1
0		none	none
1		LCDm	LCDw1
2		TVOm	TVOw
3		LCDm	TVOm
*/
int magus_fb(int flag)
{
	static int _fbmode;

	/* no change */
	if (flag == _fbmode)
	{
		return 0;
	}

	/* unload previous devices */
	if (_fbmode)
	{
		if (_fbmode & 1)
		{
			if (_fbmode == 1)
			{
				platform_device_unregister(&lcd1);
			}
			platform_device_unregister(&lcd0);
		}
		if (_fbmode & 2)
		{
			if (_fbmode == 2)
			{
				platform_device_unregister(&tvo1);
			}
			platform_device_unregister(&tvo0);
		}
	}

	/* load new devices */
	if (flag)
	{
		if (flag & 1)
		{
			platform_device_register(&lcd0);
			if (flag == 1)
			{
				platform_device_register(&lcd1);
			}
		}
		if (flag & 2)
		{
			platform_device_register(&tvo0);
			if (flag == 2)
			{
				platform_device_register(&tvo1);
			}
		}
	}

	_fbmode = flag;
	return 0;
}
#endif


extern void __init magus_init_irq(void);

struct sys_timer;
extern struct sys_timer magus_timer;

#ifdef CONFIG_ARCH_MAGUS_FPGA
MACHINE_START(MAGUS_FPGA, "Solomon Magus FPGA")
//	.boot_params	= 0xE2000100,
#else
MACHINE_START(MAGUS_ADS, "Solomon Magus ADS")
//	.boot_params	= 0x8C000100,
#endif
	.phys_io		= MAGUS_IO,
	.io_pg_offst	= ((0xF0000000) >> 18) & 0xfffc,
	.map_io			= magus_mapio,
	.init_irq		= magus_init_irq,
	.timer			= &magus_timer,
	.init_machine	= magus_init,
MACHINE_END

