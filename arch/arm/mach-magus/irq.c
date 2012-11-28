/*
 * linux/arch/arm/mach-magus/irq.c
 *
 * Copyright (C) 2006 sasin@solomon-systech.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
//#include <linux/list.h>
//#include <linux/timer.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>



#include <asm/clock.h>

#include "intc.h"


static void intc_mask_irq(unsigned int irq)
{
	intc_mask((void *)MAGUS_VIO_INTC, irq);
}


static void intc_unmask_irq(unsigned int irq)
{
	intc_unmask((void *)MAGUS_VIO_INTC, irq);
}


static void intc_ack_irq(unsigned int irq)
{
	intc_ack((void *)MAGUS_VIO_INTC, irq);
}


static struct irq_chip magus_intc = 
{
	.ack = intc_ack_irq,
	.mask = intc_mask_irq,
	.unmask = intc_unmask_irq,
};


#include "gpio.h"


static int gpio_irq_type(unsigned int irq, unsigned int type)
{
	gpio_cfg_t	cfg;

	cfg.mode = GPIO_M_INTR;
	cfg.oss = GPIO_OSS_DREG;
	cfg.issa = GPIO_ISS_ISR;
	cfg.issb = GPIO_ISS_ISR;
	cfg.pull_en = GPIO_PULL_NC;
	if (type & __IRQT_RISEDGE)
	{
		cfg.irq = GPIO_IRQ_RS;
	}
	if (type & __IRQT_FALEDGE)
	{
		cfg.irq = GPIO_IRQ_FL;
	}
	if (type & __IRQT_LOWLVL)
	{
		cfg.irq = GPIO_IRQ_LO;
	}
	if (type & __IRQT_HIGHLVL)
	{
		cfg.irq = GPIO_IRQ_HI;
	}

#if 0
	/* gets spinlock again */
	set_irq_handler(irq, cfg.irq > GPIO_IRQ_FL ? 
		handle_level_irq: handle_edge_irq);
#endif
	__set_irq_handler_unlocked(irq, cfg.irq > GPIO_IRQ_FL ? 
		handle_level_irq: handle_edge_irq);

	gpio_cfg(GPIO_PORT_VADDR(GPIO_IRQ_PORT(irq)), GPIO_IRQ_PIN(irq), &cfg);
	return 0;
}


static void gpio_ack_irq(unsigned int irq)
{
	gpio_ack(GPIO_PORT_VADDR(GPIO_IRQ_PORT(irq)), GPIO_IRQ_PIN(irq));
}


static void gpio_mask_irq(unsigned int irq)
{
	gpio_mask(GPIO_PORT_VADDR(GPIO_IRQ_PORT(irq)), GPIO_IRQ_PIN(irq));
}


static void gpio_unmask_irq(unsigned int irq)
{
	gpio_unmask(GPIO_PORT_VADDR(GPIO_IRQ_PORT(irq)), GPIO_IRQ_PIN(irq));
}


static void gpio_isr(unsigned int irq, struct irq_desc *desc)
{
	int	port;

	for (port = 0; port < 6; port++)
	{
		int			pin = 0;
		uint32_t	isr;

		isr = gpio_pend(GPIO_PORT_VADDR(port));
		while (isr)
		{
			if (isr & 1)
			{
				irq = GPIO_IRQ(port, pin);
				desc = irq_desc + irq;
				desc_handle_irq(irq, desc);
			}
			isr >>= 1;
			pin++;
		}
	}
}


static struct irq_chip magus_gpio = 
{
	.ack = gpio_ack_irq,
	.mask = gpio_mask_irq,
	.unmask = gpio_unmask_irq,
	.set_type = gpio_irq_type,
};


void __init magus_init_irq(void)
{
	unsigned int irq;

	intc_init((void *)MAGUS_VIO_INTC);
	for (irq = 0; irq < INTC_IRQS; irq++) 
	{
		set_irq_chip(irq, &magus_intc);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	gpio_init((void *)MAGUS_VIO_GPIO);
	for (irq = INTC_IRQS; irq < INTC_IRQS + GPIO_IRQS; irq++) 
	{
		set_irq_chip(irq, &magus_gpio);
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	set_irq_chained_handler(0x1D, gpio_isr);

	/* rahult, added on 13th may, 2008 */
  	magus_clock_init();

}

