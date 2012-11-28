/* linux/include/asm-arm/arch-magus/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 2006 sasin@solomon-systech.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <asm/hardware.h>

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
#if 1
		moveq	\rx, #MAGUS_IO		@ physical
		movne	\rx, #0xF0000000	@ virtual
		orreq	\rx, \rx, #0x100000	@ APB1 offset
		orr	\rx, \rx, #0x00003000	@ UART0 offset
#else
#ifdef CONFIG_ARCH_MAGUS_FPGA
		moveq	\rx, #0xD0100000	@ physical
#else
		moveq	\rx, #0x08100000	@ physical
#endif
		movne	\rx, #0xF0000000	@ virtual
		orr	\rx, \rx, #0x00003000	@ UART0 offset
#endif
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x10]	@ TX
		.endm

		.macro	waituart,rd,rx
		.endm

		.macro	busyuart,rd,rx
1002:	ldr	\rd, [\rx, #0x28]	@ LSR
		tst	\rd, #1 << 6		@ LSR_TEMT
		beq	1002b				@ wait until transmit done
		.endm
