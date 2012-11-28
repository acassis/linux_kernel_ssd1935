/*
 * linux/include/asm-arm/arch-magus/pm.h
 *
 * Header file for MAGUS Power Management Routines
 *
 *
 * Copyright 2008 Solomon Systech.
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ASM_ARCH_MAGUS_PM_H
#define __ASM_ARCH_MAGUS_PM_H

/* Port */
#define PORT_PA_IER (GPIO_PORT_VADDR(0) + 0x48)
#define PORT_PB_IER (GPIO_PORT_VADDR(1) + 0x48)
#define PORT_PC_IER (GPIO_PORT_VADDR(2) + 0x48)
#define PORT_PD_IER (GPIO_PORT_VADDR(3) + 0x48)
#define PORT_PE_IER (GPIO_PORT_VADDR(4) + 0x48)
#define PORT_PF_IER (GPIO_PORT_VADDR(5) + 0x48)

/* INTC */
#define INTC_EN0 (MAGUS_VIO_INTC + 0x38)
#define INTC_EN1 (MAGUS_VIO_INTC + 0x3C)
#define INTC_IU	 (MAGUS_VIO_INTC + 0x4C)

/* SCRM */
#define SCRM_PLL2R (MAGUS_VIO_SCRM + 0xC00)
#define SCRM_MSTR (MAGUS_VIO_SCRM + 0x1C)

#ifndef __ASSEMBLER__

#include <linux/clk.h>

/* sleep save info */

struct sleep_save {
	void __iomem	*reg;
	unsigned long	val;
};

struct ext_device{
	unsigned long io_addr;
	void __iomem *io_v_addr;
	unsigned int disable_logic;
	char name[10];
	unsigned int value;
};

#define SAVE_ITEM(x) \
	{ .reg = (x) }

extern void magus_pm_do_save(struct sleep_save *ptr, int count);
extern void magus_pm_do_restore(struct sleep_save *ptr, int count);


extern void magus_cpu_suspend(void);
extern void magus_idle_loop_suspend(void);

extern unsigned int magus_cpu_suspend_sz;
extern unsigned int magus_idle_loop_suspend_sz;

#endif /* ASSEMBLER */
#endif /* __ASM_ARCH_OMAP_PM_H */
