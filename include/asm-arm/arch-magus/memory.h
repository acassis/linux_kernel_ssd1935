/*
 *  linux/include/asm-arm/arch-magus/memory.h
 *
 *  Copyright (C) 2006 sasin@solomon-systech.com
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
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

#ifdef CONFIG_ARCH_MAGUS_FPGA
#define PHYS_OFFSET	UL(0xE2000000)
#elif defined CONFIG_ARCH_MAGUS_ADS
#define PHYS_OFFSET	UL(0x51000000)
#elif defined CONFIG_ACCIO_CM5208
#define PHYS_OFFSET	UL(0x51000000)
#elif defined CONFIG_ACCIO_CM5210
#define PHYS_OFFSET UL(0x51000000)
#elif defined CONFIG_ACCIO_A2818T
#define PHYS_OFFSET	UL(0x51000000)
#elif defined CONFIG_ACCIO_LITE
#define PHYS_OFFSET UL(0x50400000/*0x50C00000*/)
#else
#define PHYS_OFFSET	UL(0x51000000)
#endif

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus(x)	(x - PAGE_OFFSET + PHYS_OFFSET)
#define __bus_to_virt(x)	(x - PHYS_OFFSET + PAGE_OFFSET)

#endif
