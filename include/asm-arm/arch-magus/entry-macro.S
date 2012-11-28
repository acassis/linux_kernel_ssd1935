/*
 * include/asm-arm/arch-magus/entry-macro.S
 *
 * Low-level IRQ helper macros for Magus-based platforms
 *
 *  Copyright (C) 2006 sasin@solomon-systech.com
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
		.macro	disable_fiq
		.endm

		.macro	get_irqnr_preamble, base, tmp
		.endm

		.macro	arch_ret_to_user, tmp1, tmp2
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	\irqnr, #0xF0000000
		add	\irqnr, \irqnr, #0x00001000
		ldr	\irqnr, [\irqnr, #0x50]
		mvn	\irqstat, \irqnr
		tst \irqstat, #0x40
/* EQ will be set if no irqs pending */
		.endm

