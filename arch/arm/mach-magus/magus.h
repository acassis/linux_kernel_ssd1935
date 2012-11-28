/*
 * arch/arm/mach-magus/magus.h
 *
 * Copyright (c) 2006 sasin@solomon-systech.com
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __MAGUS_H_
#define __MAGUS_H_

static u64	_dma_mask = ~(u64)0;

static void	_dummy_rel(struct device *dev) { }

#define FREE_PIN	0xffffffff


#define _PNP_RC_AI(_name, _adr, _sz, _irq) \
static struct resource _name##rc[] = \
{ \
{ .start = _adr,	.end = (_adr)+(_sz)-1,	.flags = IORESOURCE_MEM, }, \
{ .start = _irq,	.end = _irq,		.flags = IORESOURCE_IRQ, },

#define _PNP_RC_AID(_name, _adr, _sz, _irq, _dma) \
_PNP_RC_AI(_name, _adr, _sz, _irq) \
{ .start = _dma,	.end = (_dma)+1,	.flags = IORESOURCE_DMA, },


/* rahult, Added for pin mux setup & binding with platform layer */
/* Assumption: DEVICE_MAX_PIN=5, max 5 pins per device, if device uses less */
/* than 5, FREE_PIN macro is used to denote it. */
#define _PNP_RC_P(_pin0, _pin1, _pin2, _pin3, _pin4) \
{ \
	.gpio_pins[0] = _pin0,  .gpio_pins[1] = _pin1, \
	.gpio_pins[2] = _pin2,  .gpio_pins[3] = _pin3, \
	.gpio_pins[4] = _pin4,  .flags = IORESOURCE_GPIO, \
}, 

#define _PNP_RC_AIP(_name, _adr, _sz, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
_PNP_RC_AI(_name, _adr, _sz, _irq) \
_PNP_RC_P(_pin0, _pin1, _pin2, _pin3, _pin4)


#define _PNP_RC_AIDP(_name, _adr, _sz, _irq, _dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
_PNP_RC_AID(_name, _adr, _sz, _irq, _dma) \
_PNP_RC_P(_pin0, _pin1, _pin2, _pin3, _pin4)


#define _PNP_DEVPR(_name, _id, _num, _plt) \
static struct platform_device _name = \
{ \
	.name = _id, \
	.id = _num, \
	.num_resources = ARRAY_SIZE(_name##rc), \
	.resource = _name##rc, \
	.dev = \
	{ \
		.release = _dummy_rel,  \
		.dma_mask = &_dma_mask, \
		.coherent_dma_mask = 0xffffffff, \
		.platform_data = _plt, \
	}, \
}

#define _PNP_DEV(_name, _id, _num)	_PNP_DEVPR(_name, _id, _num, 0)

#define _PNP_DEVP(_name, _id, _num, _plt) \
static struct platform_device _name = \
{ \
	.name = _id, \
	.id = _num, \
	.dev = \
	{ \
		.release = _dummy_rel,  \
		.dma_mask = &_dma_mask, \
		.coherent_dma_mask = 0xffffffff, \
		.platform_data = _plt, \
	}, \
}


/*
Resources specified by ASIDP - Address, Size, IRQ, DMA, gpio_pins[0~4]
if size is not provided, it is assumed to be 1 ARM MMU page ie 4KB
DMA if provided is assumed to be RX request number, with TX=RX+1
*/

#define RCN_ASIDP(_name, _id, _num, _adr, _sz, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
_PNP_RC_AIDP(_name, _adr, _sz, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
}; \
_PNP_DEV(_name, _id, _num)

#define RCN_ASIP(_name, _id, _num, _adr, _sz, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
_PNP_RC_AIP(_name, _adr, _sz, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
}; \
_PNP_DEV(_name, _id, _num)

#define RCN_AIDP(_name, _id, _num, _adr, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIDP(_name, _id, _num, _adr, 0x1000, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4)

#define RCN_AIP(_name, _id, _num, _adr, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIP(_name, _id, _num, _adr, 0x1000, _irq, _pin0, _pin1, _pin2, _pin3, _pin4)

#define RC_AIDP(_name, _id, _adr, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIDP(_name, _id, -1, _adr, 0x1000, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4)

#define RC_AIP(_name, _id, _adr, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIP(_name, _id, -1, _adr, 0x1000, _irq, _pin0, _pin1, _pin2, _pin3, _pin4)

#define RC_ASIDP(_name, _id, _adr, _sz, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIDP(_name, _id, -1, _adr, _sz, _irq, dma, _pin0, _pin1, _pin2, _pin3, _pin4)

#define RC_ASIP(_name, _id, _adr, _sz, _irq, _pin0, _pin1, _pin2, _pin3, _pin4) \
RCN_ASIP(_name, _id, -1, _adr, _sz, _irq, _pin0, _pin1, _pin2, _pin3, _pin4)


/* platform data */

RC_AIP(ehci, "ehci", MAGUS_IO_EHCI, MAGUS_IRQ_EHCI, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
//RC_AIP(otg, "otg", MAGUS_IO_OTG, MAGUS_IRQ_OTG, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(i2c, "i2c", MAGUS_IO_I2C, MAGUS_IRQ_I2C, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(tvout, "tvout", MAGUS_IO_TVOUT, MAGUS_IRQ_TVOUT, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(vpp, "vpp", MAGUS_IO_VPP, MAGUS_IRQ_VPP, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(vip, "vip", MAGUS_IO_VIP, MAGUS_IRQ_VIP, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(d2d, "d2d", MAGUS_IO_D2D, MAGUS_IRQ_D2D, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(wdog, "wdog", MAGUS_IO_WDOG, MAGUS_IRQ_WDOG, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(rtc, "rtc", MAGUS_IO_RTC, MAGUS_IRQ_RTC, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_AIP(kpp, "kpp", MAGUS_IO_KPP, MAGUS_IRQ_KPP, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RCN_AIP(sdhc, "sdhc", 0, MAGUS_IO_SDHC, MAGUS_IRQ_SDHC, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RCN_AIP(sdhc2, "sdhc", 1, MAGUS_IO_SDHC2, MAGUS_IRQ_SDHC2, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RC_ASIP(usbd, "usbd", MAGUS_IO_USBD, 0x3000, MAGUS_IRQ_USBD, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
RCN_AIDP(spi, "spi", 0, MAGUS_IO_SPI, MAGUS_IRQ_SPI, MAGUS_DMA_SPI_TX, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
#if defined CONFIG_ACCIO_PF101 || defined CONFIG_ACCIO_P1 || defined CONFIG_LUMOS_WE8623_P0 || defined CONFIG_ACCIO_LITE
RCN_AIDP(spi2, "spi", 1, MAGUS_IO_SPI2, MAGUS_IRQ_SPI2, MAGUS_DMA_SPI2_TX, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );
#endif
RC_ASIP(nfc, "nfc", MAGUS_IO_NFC, 0x2100, MAGUS_IRQ_NFC, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN, FREE_PIN );


/*
temporary switcher until OTG is up
switch flags: 0 - no USB; 1 - host; -1 - device
*/
#include <linux/delay.h>

int magus_otg(int flag)
{
#if defined CONFIG_ACCIO_CM5208 || defined CONFIG_ACCIO_A2818T || defined CONFIG_ACCIO_CM5210
	static int	_usbmode = 0x5555AAAA;
	volatile uint32_t	*v;

	if (_usbmode == 0x5555AAAA)
	{
		/* 1 time initialization */
#if defined CONFIG_ARCH_MAGUS_ADS || defined CONFIG_ARCH_MAGUS_ACCIO
		/* turn off overcurrent detection */
		gpio_direction_output(MAGUS_GPIO_EHCIOCD, 0);
#endif
#if 0
		v = ioremap_nocache(MAGUS_IO_OTG, 0x1000);
		v[2] = 1;		/* otg enable */
		v[3] = 1;		/* otg reset */
		udelay(10);
		v[3] = 0;		/* otg unreset */
		v[4] = 0x1C0;	/* bypass, force host */
		iounmap(v);
		platform_device_register(&ehci);
		platform_device_unregister(&ehci);
#endif
		_usbmode = 0;
	}

	if (flag == _usbmode)
	{
		return 0;
	}

	v = ioremap_nocache(MAGUS_IO_OTG, 0x1000);
	if (_usbmode)
	{
		platform_device_unregister(_usbmode > 0 ? &ehci : &usbd);
		v[2] = 0;			/* otg disable */
	}
	if (flag)
	{
		struct platform_device	*p;

		v[2] = 1;			/* otg enable */
		v[3] = 1;			/* otg reset */
		mdelay(10);
		v[3] = 0;			/* otg unreset */
		if (flag > 0)
		{
			v[4] = 0x1C0;	/* bypass, force host */
			mdelay(10);
			p = &ehci;
		}
		else
		{
			v[4] = 0x140;	/* bypass, force device */
			p = &usbd;
		}
		platform_device_register(p);
	}
	iounmap(v);
	_usbmode = flag;
	return 0;
#else
	static int	_usbmode = 0x5555AAAA;

	if (_usbmode == 0x5555AAAA)
	{
		/* 1 time initialization */
#if defined CONFIG_ARCH_MAGUS_ADS || defined CONFIG_ARCH_MAGUS_ACCIO
		/* turn off overcurrent detection */
		gpio_direction_output(MAGUS_GPIO_EHCIOCD, 0);
#endif
		_usbmode = 0;
	}
	if (flag == _usbmode)
	{
		return 0;
	}
	if (_usbmode)
	{
		struct platform_device	*p;

		p = _usbmode > 0 ? &ehci : &usbd;
		platform_device_unregister(p);
		p->dev.kobj.k_name = 0;
	}
	if (flag)
	{
		platform_device_register((flag > 0) ? &ehci : &usbd);
	}
	_usbmode = flag;
	return 0;
#endif
}

EXPORT_SYMBOL(magus_otg);

#endif
