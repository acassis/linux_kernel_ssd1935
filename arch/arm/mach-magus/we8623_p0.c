/*
 * arch/arm/mach-magus/we8623_p0.c
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

#ifdef CONFIG_ACCIO_LCM_CPT480X272
#include "lcdm_cpt480x272.h"
#elif CONFIG_ACCIO_LCM_TPO800X480
#include "lcdm_tpo800x480.h"
#endif

/* platform data */

#include <linux/spi/spi.h>
#include "ak4182.h"

static int ak4182_pendown(void)
{
	return !gpio_get_value(MAGUS_GPIO_AK4182);
}

static const struct ak4182_platform_data	ak4182 =
{
	.x_max		= 0x0fff,
	.y_max		= 0x0fff,
	.x_plate_ohms	= 180,
	.pressure_max	= 255,
	.debounce_max	= 10,
	.debounce_tol	= 3,
	.get_pendown_state = ak4182_pendown,
};

static struct spi_board_info _spi[] __initdata =
{
#ifdef CONFIG_ACCIO_LCM_CPT480X272	
	{
		.modalias	= "lcmspi",
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 18000000,
		.mode		= SPI_MODE_3,		
	},	
#endif
	{
		.modalias	= "WM8987",
		.bus_num	= 0,
		.chip_select	= 1,
		.max_speed_hz	= 100000,
		.mode		= SPI_MODE_3,
	},
	{
		.modalias	= "ak4182",
		.bus_num	= 0,
		.chip_select	= 2,
		.max_speed_hz	= 2500000,
		.irq		= gpio_to_irq(MAGUS_GPIO_AK4182),
		.platform_data	= &ak4182,
	},
	{
		.modalias	= "ifx01_cmmb",
		.bus_num	= 1,
		.chip_select	= 0,
		.max_speed_hz	= 4000000,
		.mode		= SPI_MODE_0,
	},

};

#include <linux/i2c.h>
#include <linux/rtc/s35390a.h>

static const struct s35390a_platform_data s35390a =
{
	.irq1	= 0,
	.irq2	= gpio_to_irq(GPIO_NUM(5, 4)),
};

static struct i2c_board_info __initdata _i2c[] =
{
	{
		I2C_BOARD_INFO("rtc-s35390a", 0x30),
		.platform_data = &s35390a,
	},
};

#include <linux/input.h>
#include "gpio_vol.h"

static gpio_vol_data	vol_data =
{
	.code		= KEY_AUDIO,
	.gpio[0]	= MAGUS_GPIO_VOL_UP,
	.gpio[1]	= MAGUS_GPIO_VOL_DOWN,
	.active_low	= 0,
	.desc		= "gpio_vol",
	.type		= EV_KEY,
};

/* frame buffer */
_PNP_RC_AIP(lcdc, MAGUS_IO_LCDC, 0x1000, MAGUS_IRQ_LCDC, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN )
};
_PNP_DEVPR(lcdc, "lcdc", 0, &lcdc_main);
_PNP_DEVP(lcdcw1, "lcdc", 1, &lcdc_flt[0]);
_PNP_DEVP(lcdcw2, "lcdc", 2, &lcdc_flt[1]);

/* gpio_volume */
_PNP_RC_AIP(gpio_vol, MAGUS_IO_GPIO, 0x1000, MAGUS_IRQ_GPIO, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN )
};
_PNP_DEVPR(gpio_vol, "gpio_vol", 0, &vol_data);

RC_AIP(sslled, "ssl_led", MAGUS_IO_GPIO, MAGUS_IRQ_GPIO, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );

static struct platform_device *devices[] = 
{
//	&ehci,
//	&usbd,
//	&otg,
	&sdhc,
	&sdhc2,
	&spi,
	&spi2,
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
	&gpio_vol,
	&sslled,
};

static void we8623_p0_power_off(void)
{
	/* power down the kh board, PD0. cody */
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x318) |= 0x1;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x320) |= 0x1;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x33C) &= ~0x1;

	while (1)
		*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x33C) &= ~0x1;
}

static void we8623_p0_restart(char mode)
{
	uint32_t watchdog_vadd;

	/* todo */
	//arm_machine_restart('h');
	watchdog_vadd = (uint32_t) ioremap_nocache(0x0810c000, 0x1000);
	
	*(volatile uint32_t *)(watchdog_vadd | 0x008) = 0x105;	// enable watchdog module.
	*(volatile uint32_t *)(watchdog_vadd | 0x014) = 0x001;	// interrupt enable.
	*(volatile uint32_t *)(watchdog_vadd | 0x00c) = 0x001;	// reset system after 1second.

	mdelay(2000);
	printk("Reboot failed in we8623_p0_restart().\n");
	while (1);
}

static void gpio_setting(void)
{
	/*disable CMMB module, codec 12M clk and power, disable amplifier*/
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x318) = 0x00002025;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x320) = 0x00002025;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x33C) = 0x00000025;

	/*enable SDHC & TV-IN power off &LED enable*/
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x518) = 0x00803000;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x520) = 0x00803000;
	*(volatile uint32_t *)(MAGUS_VIO_GPIO | 0x53C) = 0x00002000;
	
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
#ifdef CONFIG_I2C
	i2c_register_board_info(0, _i2c, ARRAY_SIZE(_i2c));
#endif
	pm_power_off = we8623_p0_power_off;
	arm_pm_restart = we8623_p0_restart;

	gpio_setting();
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
	{	/* PWM */
		.virtual	= MAGUS_VIO_PWM,
		.pfn		= __phys_to_pfn(MAGUS_IO_PWM),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
	{   /* SDRAMC */
	    .virtual    = MAGUS_VIO_SDRAMC,
	    .pfn        = __phys_to_pfn(MAGUS_IO_SDRAMC),
	    .length     = SZ_4K,
	    .type       = MT_DEVICE
	},
	{   /* Internal RAM */
	    .virtual    = MAGUS_VIO_SRAM,
	    .pfn        = __phys_to_pfn(MAGUS_IO_SRAM),
	    .length     = SZ_64K,
	    .type       = MT_DEVICE
	},
};


static void __init magus_mapio(void)
{
	iotable_init(magus_io, ARRAY_SIZE(magus_io));
}

extern void __init magus_init_irq(void);

struct sys_timer;
extern struct sys_timer magus_timer;

MACHINE_START(MAGUS_ACCIO, "Solomon Magus Accio P1")
	.phys_io		= MAGUS_IO,
	.io_pg_offst	= ((0xF0000000) >> 18) & 0xfffc,
	.map_io			= magus_mapio,
	.init_irq		= magus_init_irq,
	.timer			= &magus_timer,
	.init_machine	= magus_init,
MACHINE_END

