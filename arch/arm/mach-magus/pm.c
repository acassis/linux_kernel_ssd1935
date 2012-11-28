/*
 * linux/arch/arm/mach-magus/pm.c
 *
 * Magus Power Management Routines
 *
 * Copyright (C) 2008 Solomon Systech, Inc.
 * Spiritwen <spiritwen@solomon-systech.com>
 *
 * Based on pm.c for omap2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/atomic.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/mach-types.h>

#include <asm/cacheflush.h> 
#include <asm/arch/irqs.h>
//#include <asm/arch/clock.h>
//#include <asm/arch/sram.h>
#include <asm/arch/pm.h>
#include <asm/gpio.h>

#define PM_VERSION "V0.1.1"
#define SRAM_VA MAGUS_VIO_SRAM 

static void *saved_sram;
static void (*saved_idle)(void);
static void magus_pm_idle(void);

static unsigned int stat_idle_times = 0;
static unsigned short enable_dyn_sleep = 0;

static struct sleep_save gpio_save[] = {
	SAVE_ITEM(PORT_PA_IER),
	SAVE_ITEM(PORT_PB_IER),
	SAVE_ITEM(PORT_PC_IER),
	SAVE_ITEM(PORT_PD_IER),
	SAVE_ITEM(PORT_PE_IER),
	SAVE_ITEM(PORT_PF_IER),
};
static struct sleep_save intc_save[] = {
	SAVE_ITEM(INTC_EN0),
	SAVE_ITEM(INTC_EN1),
	
	/* SCRM */
	SAVE_ITEM(SCRM_PLL2R),
};

static struct ext_device external_device[] = {
	{
		MAGUS_IO_OTG, 0, 0,"OTG",
	},
};


static ssize_t magus_pm_sleep_while_idle_show(struct kset *kset, char *buf)
{
	return sprintf(buf, "PM version %s \n NO: 0; YES: 1  %hu\n idle times: %d\n", PM_VERSION, enable_dyn_sleep, stat_idle_times);
}

static ssize_t magus_pm_sleep_while_idle_store(struct kset *kset,
					      const char * buf,
					      size_t n)
{
	unsigned short value;
	if (sscanf(buf, "%hu", &value) != 1 ||
	    (value != 0 && value != 1)) {
		printk(KERN_ERR "idle_sleep_store: Invalid value\n");
		return -EINVAL;
	}
	if(value == 0){
		pm_idle = NULL;
		stat_idle_times =0;
	}
	else{
		pm_idle = magus_pm_idle;
	}
	enable_dyn_sleep = value;
	return n;
}

static struct subsys_attribute sleep_while_idle_attr = {
	.attr   = {
		.name = __stringify(sleep_while_idle),
		.mode = 0644,
	},
	.show   = magus_pm_sleep_while_idle_show,
	.store  = magus_pm_sleep_while_idle_store,
};

extern struct kset power_subsys;

static void magus_pm_idle(void)
{
	int do_sleep;
	void (*magus_idle_loop_suspend_ptr) (void);

	local_irq_disable();
	local_fiq_disable();
	if (need_resched()) {
		local_fiq_enable();
		local_irq_enable();
		return;
	}

	/*
	 * Since an interrupt may set up a timer, we don't want to
	 * reprogram the hardware timer with interrupts enabled.
	 * Re-enable interrupts only after returning from idle.
	 */
	timer_dyn_reprogram();
	
	do_sleep = 0;
	while (enable_dyn_sleep) {
		do_sleep = 1;
		break;
	}
	if(do_sleep){   
	    
		stat_idle_times ++;
		/*saving portion of SRAM to be used by suspend function. */
	    memcpy(saved_sram, (void *)SRAM_VA, magus_idle_loop_suspend_sz);

	    /*make sure SRAM copy gets physically written into SDRAM.
        SDRAM will be placed into self-refresh during power down */
	    flush_cache_all();

	    /*copy suspend function into SRAM */
	    memcpy((void *)SRAM_VA, magus_idle_loop_suspend, magus_idle_loop_suspend_sz);

	    /*do suspend */
	    magus_idle_loop_suspend_ptr = (void *)SRAM_VA;
	    magus_idle_loop_suspend_ptr();
		//omap2_sram_idle();

	    /*restoring portion of SRAM that was used by suspend function */
 	    memcpy((void *)SRAM_VA, saved_sram, magus_idle_loop_suspend_sz);


		local_fiq_enable();
		local_irq_enable();
		return;
	}	
	local_fiq_enable();
	local_irq_enable();
}

static int magus_pm_prepare(void)
{
	/* We cannot sleep in idle until we have resumed */
	saved_idle = pm_idle;
	pm_idle = NULL;
	return 0;
}


/* helper functions to save and restore register state */
/* magus_pm_do_restore
 *
 * restore the system from the given list of saved registers
 *
 * Note, we do not use DBG() in here, as the system may not have
 * restore the UARTs state yet
*/

void magus_pm_do_restore(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		printk(KERN_DEBUG "restore %p (restore %08lx, was %08x)\n",
		       ptr->reg, ptr->val, __raw_readl(ptr->reg));

		__raw_writel(ptr->val, ptr->reg);
	}
}

void magus_pm_do_save(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		ptr->val = __raw_readl(ptr->reg);
		printk("saved %p value %08lx\n", ptr->reg, ptr->val);
	}
}

#define preg(reg)	printk("(0x%p):\t0x%08x\n", reg, __raw_readl(reg));

static void magus_pm_debug(char * desc)
{
	printk("%s:\n", desc);
	preg(PORT_PA_IER);
	preg(PORT_PB_IER);
	preg(PORT_PC_IER);
	preg(PORT_PD_IER);
	preg(PORT_PE_IER);
	preg(PORT_PF_IER);
	
	preg(INTC_EN0);
	preg(INTC_EN1);
	
	preg(SCRM_PLL2R);
	preg(SCRM_MSTR);
}

static inline void magus_pm_save_registers(void)
{
	magus_pm_do_save(gpio_save, ARRAY_SIZE(gpio_save));
	magus_pm_do_save(intc_save, ARRAY_SIZE(intc_save));
}

static inline void magus_pm_restore_registers(void)
{
	magus_pm_do_restore(intc_save, ARRAY_SIZE(intc_save));
	magus_pm_do_restore(gpio_save, ARRAY_SIZE(gpio_save));
}

static inline void magus_pm_disable_int(void)
{
	__raw_writel(0, PORT_PA_IER);
	__raw_writel(0, PORT_PB_IER);
	__raw_writel(0, PORT_PC_IER);
	__raw_writel(0, PORT_PD_IER);
	__raw_writel(0, PORT_PE_IER);
	__raw_writel(0, PORT_PF_IER);
	
	__raw_writel(0, INTC_EN0);
	__raw_writel(0, INTC_EN1);
}

static void magus_pm_configure_extint(void)
{
#if defined CONFIG_ACCIO_CM5208 || defined CONFIG_ACCIO_A2818T || defined CONFIG_ACCIO_CM5210
	__raw_writel(__raw_readl(PORT_PD_IER) | (1<<14), PORT_PD_IER); //PD14 Low battery 
	__raw_writel(__raw_readl(PORT_PE_IER) | (1<<5), PORT_PE_IER); //PE5 Powerkey
#elif defined CONFIG_ARCH_MAGUS_ADS || defined CONFIG_ARCH_MAGUS_ACCIO
	__raw_writel(__raw_readl(PORT_PD_IER) | (1<<3), PORT_PD_IER); //PD3 Powerkey
	__raw_writel(__raw_readl(PORT_PD_IER) | (1<<7), PORT_PD_IER); //PD7 Battery Charger
#endif

	/* Enable GPIO irq */
	__raw_writel(0x1d, INTC_IU); //GPIO
	__raw_writel(0x1c, INTC_IU); //internal RTC
	return;
}

static void diable_external_device(void)
{
#if 0
	int i;
	for(i=0; i < ARRAY_SIZE(external_device); i++){
		/* Save the stats of device.*/
		external_device[i].value = __raw_readl(external_device[i].io_v_addr) && 0xFFFFFFFE;	
		
		if(external_device[i].disable_logic)
			__raw_writel(__raw_readl(external_device[i].io_v_addr) | 0x1, external_device[i].io_v_addr);
		else
			__raw_writel(__raw_readl(external_device[i].io_v_addr) & 0xFFFFFFFE, external_device[i].io_v_addr);
	}
	return ;	
#endif
}

static void restore_external_device(void)
{
#if 0
	int i;
	for(i=0; i < ARRAY_SIZE(external_device); i++){
		if(external_device[i].value)	
			__raw_writel(__raw_readl(external_device[i].io_v_addr) | 0x1, external_device[i].io_v_addr);
		else 
			__raw_writel(__raw_readl(external_device[i].io_v_addr) & 0xFFFFFFFE, external_device[i].io_v_addr);
	}
#endif
}

static int magus_pm_suspend(void)
{
	void (*magus_cpu_suspend_ptr) (void);
	local_irq_disable();
	local_fiq_disable();

	/* 1. Copy suspend code to internal SRAM */
	/* saving portion of SRAM to be used by suspend function. */
	memcpy(saved_sram, (void *)SRAM_VA, magus_cpu_suspend_sz);

	/*make sure SRAM copy gets physically written into SDRAM.
	SDRAM will be placed into self-refresh during power down */
	flush_cache_all();

	/*copy suspend function into SRAM */
	memcpy((void *)SRAM_VA, magus_cpu_suspend, magus_cpu_suspend_sz);
	
	//magus_pm_debug("Status before save");
	/*2. Save and Disable interrupts except for the wake events */
	magus_pm_save_registers();
	magus_pm_disable_int();	

	/* Shutdown PLL2 */
	__raw_writel((__raw_readl(SCRM_PLL2R) | 0x1), SCRM_PLL2R); //PD3 Powerkey
	
	/*2.1 . Enable wake-up events */
	/* set the irq configuration for wake */
    magus_pm_configure_extint();
	
	/*3. Disable and save other modules */
	diable_external_device();

	magus_pm_debug("Status before suspend");

	/* Must wait for serial buffers to clear */
	mdelay(200);
	/*make sure SRAM copy gets physically written into SDRAM.
	SDRAM will be placed into self-refresh during power down */
	/*4. do suspend */
	/* Jump to SRAM suspend code */
	if(1){
	//if(enable_dyn_sleep){ //For debug.
		flush_cache_all();
		magus_cpu_suspend_ptr = (void *)SRAM_VA;
		magus_cpu_suspend_ptr();
	}

	magus_pm_debug("Status after wake up");
	/*4.1.  Disable and save other modules */
	restore_external_device();	
	
	magus_pm_restore_registers();
	magus_pm_debug("Status after restore");

	/*5. restoring portion of SRAM that was used by suspend function */
    memcpy((void *)SRAM_VA, saved_sram, magus_cpu_suspend_sz);

	/*6. Restore interrupts */
	local_fiq_enable();
	local_irq_enable();
	return 0;
}

static int magus_pm_enter(suspend_state_t state)
{
	int ret = 0;

	switch (state)
	{
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = magus_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void magus_pm_finish(void)
{
	pm_idle = saved_idle;
}

static struct platform_suspend_ops magus_pm_ops = {
	.prepare	= magus_pm_prepare,
	.enter		= magus_pm_enter,
	.finish		= magus_pm_finish,
	.valid		= suspend_valid_only_mem,
};

void inline init_addr_external_device(void)
{
#if 0
	int i;
	for(i=0; i < ARRAY_SIZE(external_device); i++){
		/* acquire resources */
		if (!request_mem_region(external_device[i].io_addr, 0x1000, external_device[i].name)){
			printk("pm: probe err - req_mem_rgn\n");
			goto l_mem_err;
		}
		external_device[i].io_v_addr = ioremap_nocache(external_device[i].io_addr, 0x1000);
		if (!external_device[i].io_v_addr){
			printk("pm: probe err - remap\n");
	    	goto l_map_err;
		}
		continue;
	
l_map_err:
		release_mem_region(external_device[i].io_addr, 0x1000);
l_mem_err:
		continue;
	}
	return ;
#endif
}

int __init magus_pm_init(void)
{
	int error;
	u32 sram_size_to_allocate;
	printk("Power Management for MAGUS. %s\n", PM_VERSION);
	
	init_addr_external_device();
	if (magus_idle_loop_suspend_sz > magus_cpu_suspend_sz)
		sram_size_to_allocate = magus_idle_loop_suspend_sz;
	else
		sram_size_to_allocate = magus_cpu_suspend_sz;


	saved_sram = kmalloc(sram_size_to_allocate, GFP_ATOMIC);
	if (!saved_sram) {
		printk(KERN_ERR
	             "PM Suspend: cannot allocate memory to save portion of SRAM\n");
	    return -ENOMEM;
	}   

	suspend_set_ops(&magus_pm_ops);
	//pm_idle = magus_pm_idle;

	error = subsys_create_file(&power_subsys, &sleep_while_idle_attr);
	if (error)
		printk(KERN_ERR "PM: subsys_create_file failed: %d\n", error);

	return 0;
}

__initcall(magus_pm_init);
