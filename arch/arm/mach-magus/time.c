/*
 *  linux/arch/arm/mach-magus/time.c
 *
 *  Copyright (C) 2006 sasin@solomon-systech.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#include <asm/hardware.h>
#include <asm/mach/time.h>
#include <asm/gpio.h>

#include "gpt.h"


static gpt_t	gpt;


#ifndef CONFIG_GENERIC_TIME
static unsigned long tmr_getofs(void)
{

	return gpt_sys_ofs(&gpt) * 10000;

}
#endif


/*
 * IRQ handler for the timer
 */
static irqreturn_t tmr_isr(int irq, void *dev_id)
{

	write_seqlock(&xtime_lock);

	gpt_isr(&gpt);
	timer_tick();

	/* toggle led ever 1s */
#if 1
{
	static int	_count;

	_count++;
#ifdef CONFIG_ARCH_MAGUS_ADS
	if (_count == 50)
	{
		gpio_set_value(MAGUS_GPIO_TMRLED, 1);
	}
	else
#endif
	if (_count == 100)
	{
#if defined CONFIG_ARCH_MAGUS_FPGA
#define CM_CTRL		(*(volatile uint32_t *)0xF000000C)
#define CM_CTRL_LED	1
		CM_CTRL ^= CM_CTRL_LED;
#elif defined CONFIG_ARCH_MAGUS_ADS
		gpio_set_value(MAGUS_GPIO_TMRLED, 0);
#endif
		_count = 0;
	}
}
#endif

	write_sequnlock(&xtime_lock);


	return IRQ_HANDLED;
}



#define MNT32K	32768

static void __init gpt0_init(void)
{
	gpt.reg = (void *)MAGUS_VIO_TMR;
	gpt.apbclk = MAGUS_CLK_APB;
	gpt.perclk = MAGUS_CLK_PER1;
	/* SCRM.clkmode -> mounted crystal; or 32000Hz from 12MHz */
	gpt.clk32k = ((*(volatile uint32_t *)0xF0000018) & (1 << 16)) ?
		32000 : MNT32K;
	gpt_init(&gpt);
	gpt_sys_start(&gpt, 100);
}



static struct irqaction tmr_irq = 
{
	.name		= "systmr",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= tmr_isr
};


static void __init tmr_init(void)
{
#ifdef CONFIG_ARCH_MAGUS_ADS
	gpio_direction_output(MAGUS_GPIO_TMRLED, 0);
#endif
	gpt0_init();
	setup_irq(MAGUS_IRQ_TMR, &tmr_irq);
}


struct sys_timer magus_timer = 
{
	.init	= tmr_init,
#ifndef CONFIG_GENERIC_TIME
	.offset	= tmr_getofs,
#endif
};

