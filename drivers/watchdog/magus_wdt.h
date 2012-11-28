/* linux/drivers/char/watchdog/magus_wdt.h
 *
 * Copyright (c) 2008 Solomon Systech 
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Based on, softdog.c by Alan Cox,
 *     (c) Copyright 1996 Alan Cox <alan@redhat.com>
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
 *
 * Changelog:
 *	03-July-2008 Initial	
 *
*/
#include <asm/hardware.h>
#define MAGUS_WATCHDOG    (0x0810C000)
#define MAGUS_SZ_WATCHDOG SZ_1K

/*
 *	MAGUS Watchdog register define .
 * */
// REG
#define MAGUSWDT_CFGR  0x0008	// CRGR
#define MAGUSWDT_TCNTR 0x000C 		// TCNTR
#define MAGUSWDT_RSTR  0x0010		// RSTR
#define MAGUSWDT_INTR  0x0014   	// INTR
#define MAGUSWDT_STSR  0x0018      // STSR

// bit
#define BIT_MAGUSWDT_CFGR_EN 0
#define BIT_MAGUSWDT_CFGR_RSTEN 8
#define BIT_MAGUSWDT_INTR_INTEN 0
#define BIT_MAGUSWDT_STSR_INT 0

#define MAGUSWDT_CFGR_EN (1 << BIT_MAGUSWDT_CFGR_EN)
#define MAGUSWDT_CFGR_RSTEN (1 << BIT_MAGUSWDT_CFGR_RSTEN)
#define MAGUSWDT_INTR_INTEN (1 << BIT_MAGUSWDT_INTR_INTEN)


/* Watchdog */
#if 1
#define magus_device_wdt wdog 
#else
static struct resource magus_wdt_resource[] = {
	[0] = {
		.start = MAGUS_WATCHDOG,
		.end   = MAGUS_WATCHDOG + MAGUS_SZ_WATCHDOG - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MAGUS_IRQ_WDOG,
		.end   = MAGUS_IRQ_WDOG,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device magus_device_wdt = {
	.name		  = "magus-wdt",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(magus_wdt_resource),
	.resource	  = magus_wdt_resource,
};

EXPORT_SYMBOL(magus_device_wdt);
#endif
