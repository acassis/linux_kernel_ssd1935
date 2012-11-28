/*
 * arch/arm/mach-magus/accio_pf102.c
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
#include "map.h"
#include "os.h"

#include "magus.h"

#ifdef CONFIG_ACCIO_PF102
#include "lcdm_lq043.h"
#endif

/* platform data */

#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>

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
		.modalias	= "WM8978",
		.bus_num	= 0,
		.chip_select	= 3,
		.max_speed_hz	= 100000,
		.mode		= SPI_MODE_3,
	},
	{
		.modalias	= "ads7846",
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 2500000,
		.irq		= gpio_to_irq(MAGUS_GPIO_ADS7846),
		.platform_data	= &ads7846,
	},
};

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


static void accio_pf102_power_off(void)
{
	/* power down the ag board, PA8. cody */
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x18) |= 0x100;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x20) |= 0x100;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x3C) |= 0x100;

	while (1);
}

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
	spi_register_board_info(_spi, ARRAY_SIZE(_spi));
	pm_power_off = accio_pf102_power_off;
}


static struct map_desc magus_io[] __initdata = 
{
	{	/* SCRM */
		.virtual	= MAGUS_VIO_SCRM,
		.pfn		= __phys_to_pfn(MAGUS_IO_SCRM),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
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
	{	/* TMR1 */
		.virtual	= MAGUS_VIO_TMR,
		.pfn		= __phys_to_pfn(MAGUS_IO_TMR),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
#if 0
{	/* TMR2 */
	.virtual	= MAGUS_VIO_TMR + 0x1000,
	.pfn		= __phys_to_pfn(MAGUS_IO_TMR2),
	.length		= SZ_4K,
	.type		= MT_DEVICE
},
#endif
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
};


static void __init magus_mapio(void)
{
	iotable_init(magus_io, ARRAY_SIZE(magus_io));
}

extern void __init magus_init_irq(void);

struct sys_timer;
extern struct sys_timer magus_timer;

MACHINE_START(MAGUS_ACCIO, "Solomon Magus Accio PF102")
	.phys_io		= MAGUS_IO,
	.io_pg_offst	= ((0xF0000000) >> 18) & 0xfffc,
	.map_io			= magus_mapio,
	.init_irq		= magus_init_irq,
	.timer			= &magus_timer,
	.init_machine	= magus_init,
MACHINE_END

