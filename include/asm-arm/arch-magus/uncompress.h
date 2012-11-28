/*
 *  linux/include/asm-arm/arch-magus/uncompress.h
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


#ifdef CONFIG_ARCH_MAGUS_FPGA
#define UART_ADR	0xD0103000
#else
#define UART_ADR	0x08103000
#endif
#define UART_TX		((volatile uint32_t *)UART_ADR)[0x10 >> 2]
#define UART_LSR	((volatile uint32_t *)UART_ADR)[0x28 >> 2]
#define UART_LSR_TEMT	0x40

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader.  We search for the first enabled
 * port in the most probable order.  If you didn't setup a port in
 * your bootloader then nothing will appear (which might be desired).
 *
 * This does not append a newline
 */
static void putc(int c)
{
	while (!(UART_LSR & UART_LSR_TEMT))
	{
		;
	}
	UART_TX = c;
}


static inline void flush(void)
{
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()
