/*
 * linux/include/asm-arm/arch-magus/gpio.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __ASM_ARCH_MAGUS_GPIO_H
#define __ASM_ARCH_MAGUS_GPIO_H


#include <asm/hardware.h>

#define GPIO_PORT(gpio)		((gpio & GPIO_PORT_MASK) >> GPIO_PORT_SHIFT)
#define GPIO_PIN(gpio)		((gpio) & ((1 << GPIO_PINS) - 1))
#define GPIO_NUM(port, pin)	(((port) << GPIO_PINS) | (pin))


int gpio_request(unsigned gpio, const char *label);

void gpio_free(unsigned gpio);

int gpio_direction_input(unsigned gpio);

int gpio_direction_output(unsigned gpio, int value);

int gpio_get_value(unsigned gpio);

void gpio_set_value(unsigned gpio, int value);

/* API routine which takes below pin definitions as argument & configures the pin accordingly */
int gpio_fn(unsigned long gpio_mode);

#include <asm-generic/gpio.h>			/* cansleep wrappers */

#define gpio_to_irq(gpio)	(INTC_IRQS + (gpio))
#define irq_to_gpio(irq)	GPIO_NUM(GPIO_IRQ_PORT(irq), GPIO_IRQ_PIN(irq))


#endif
