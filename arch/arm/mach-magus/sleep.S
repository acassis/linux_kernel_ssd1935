/*
 * linux/arch/arm/mach-magus/sleep.S
 *
 * (C) Copyright 2008
 * Solomon Systech
 * Spiritwen@solomon-systech.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/arch/io.h>
#include <asm/arch/pm.h>
#include <asm/arch/hardware.h>

#define A_SDRC_CMDR_V      IO_ADDRESS(MAGUS_VIO_SDRAMC+0x08)
#define A_SCRM_PLLR_V      IO_ADDRESS(MAGUS_VIO_SCRM+0x08)
#define A_SCRM_PLL2R_V      IO_ADDRESS(MAGUS_VIO_SCRM+0x0c00)

	.text

/*
 * Forces Magus into idle state
 *
 * magus_idle_loop_suspend() - This bit of code just executes the WFI
 * for normal idles.
 *
 * Note: This code get's copied to internal SRAM at boot. When the OMAP
 *	 wakes up it continues execution at the point it went to sleep.
 */
ENTRY(magus_idle_loop_suspend)
	mov	r0, #0
	mrc	p15, 0, r1, c1, c0, 0		@ Read control register
	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
	bic	r2, r1, #1 << 12
	mrs	r3, cpsr			@ Disable FIQs while Icache
	orr	ip, r3, #PSR_F_BIT		@ is disabled
	msr	cpsr_c, ip
	mcr	p15, 0, r2, c1, c0, 0		@ Disable I cache
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
	mcr	p15, 0, r1, c1, c0, 0		@ Restore ICache enable
	msr	cpsr_c, r3			@ Restore FIQ state
	mov	pc, lr
/*
*	stmfd	sp!, {r0, lr}		@ save registers on stack
*	mov	r0, #0			@ clear for mcr setup
*	mcr	p15, 0, r0, c7, c0, 4	@ wait for interrupt
*	ldmfd	sp!, {r0, pc}		@ restore regs and return
*/
ENTRY(magus_idle_loop_suspend_sz)
	.word	. - magus_idle_loop_suspend

/*
 * magus_cpu_suspend() - Forces MAGUS into deep sleep state by completing
 * SDRC shutdown then ARM shutdown.  Upon wake MPU is back on so just restore
 * SDRC.
 * @r3, r4: save SDRAMC info
 * @r5: delay
 * @r6: save PLL2R 
 *
 */
ENTRY(magus_cpu_suspend)
	stmfd	sp!, {r0 - r12, lr}	@ save registers on stack
	mov	r3, #0x0		@ clear for mrc call
	mcr	p15, 0, r3, c7, c10, 4	@ memory barrier, hope SDR/DDR finished
	nop
	nop

	ldr	r3, A_SCRM_PLL2R	@ addr of scrm power
	ldr	r6, [r3]		@ save value of scrm power
	ldr	r4, [r3]		@ value of scrm power
	orr	r4, r4, #0x1		@ Power down PLL2 
	str	r4, [r3]		@ make it so

	ldr	r3, A_SCRM_PLLR	@ addr of scrm power
	ldr	r4, [r3]		@ value of scrm power
	orr	r4, r4, #0x1		@ Power down PLL 
	str	r4, [r3]		@ make it so
	
	ldr	r3, A_SDRC_CMDR	@ addr of sdrc power
	ldr	r4, [r3]		@ value of sdrc power
	and	r4, r4, #0x0FFFFFFF		@ enable self refresh on idle req
	orr	r4, r4, #0x50000000		@ enable self refresh on idle req
	mov	r5, #0x2000		@ set delay (DPLL relock + DLL relock)
	str	r4, [r3]		@ make it so
	
	mov	r2, #0
	nop
	mcr	p15, 0, r2, c7, c0, 4	@ wait for interrupt
	nop
loop:
	subs	r5, r5, #0x1		@ awake, wait just a bit
	bne	loop

	/* The DPLL has on before we take the DDR out of self refresh */
	and	r4, r4, #0x0FFFFFFF		@ now clear self refresh bit.
	str	r4, [r3]		@ put vlaue back.
	
	ldr	r3, A_SCRM_PLL2R	@ addr of scrm pll2 power
	str	r6, [r3]		@ put vlaue back.
	
	mov	r5, #0x1000
loop2:
	subs	r5, r5, #0x1
	bne	loop2
	/* resume*/
	ldmfd	sp!, {r0 - r12, pc}	@ restore regs and return

A_SDRC_CMDR:
	.word A_SDRC_CMDR_V
A_SCRM_PLLR:
	.word A_SCRM_PLLR_V
A_SCRM_PLL2R:
	.word A_SCRM_PLL2R_V
ENTRY(magus_cpu_suspend_sz)
	.word	. - magus_cpu_suspend

