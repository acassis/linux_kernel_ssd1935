/*
 * linux/arch/arm/mach-magus/ssl_gpio.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <asm/hardware.h>
#include <asm/gpio.h>

#include "gpio.h"
#include "os.h"


/* gpio pins allocation bitmap - to keep track of free/allocted pins */
unsigned long magus_gpio_alloc_map[(GPIO_PORT_MAX + 1) * 32 / BITS_PER_LONG];


/*
request GPIO, return value to be checked by the caller before configuring
the pin. 
Note: requesting gpio using this function does not cause it to be
configured in any was, use other pin setup API's for pin configuration.
@param[in]	gpio		gpio pin
@param[in]	label		any string/label used for diagnostics.
@return 	0 on success, or negative error no.
*/
int gpio_request(unsigned gpio, const char *label)
{
	if(GPIO_PORT(gpio) >= GPIO_PORTS) {
		dbg("magus_gpio: gpio_request error - Attempt to request nonexistent GPIO port-%d, pin-%d for \"%s\"\n", 
					GPIO_PORT(gpio)+1, GPIO_PIN(gpio), label ? label : "?");
		return -EINVAL;
	}

	if(test_and_set_bit(gpio, magus_gpio_alloc_map)) {
		dbg("magus_gpio: gpio_request error - GPIO port-%d, pin-%d already used. Allocation for \"%s\" failed\n", 
					GPIO_PORT(gpio)+1, GPIO_PIN(gpio), label ? label : "?");
		return -EBUSY;
	}
	return 0;
}

/*
free GPIO - clear alloc bit in the allocation bitmap.
@param[in]	gpio		gpio pin
*/
void gpio_free(unsigned gpio)
{
	if(GPIO_PORT(gpio) >= GPIO_PORTS )
		return;

	clear_bit(gpio, magus_gpio_alloc_map);

	return;
}

/*
sets the gpio as input pin.
@param[in]	gpio		gpio pin
@return 	0 on success, or negative error no.
*/
int gpio_direction_input(unsigned gpio)
{
	gpio_cfg_t	t;

	memset(&t, 0, sizeof(t));
	t.mode = GPIO_M_IN;
	gpio_cfg(GPIO_PORT_VADDR(GPIO_PORT(gpio)), GPIO_PIN(gpio), &t);
	return 0;
}


/*
sets the gpio as output pin and writes a value.
@param[in]	gpio		gpio pin
@param[in]	value		value to be written
@return 	0 on success, or negative error no.
*/
int gpio_direction_output(unsigned gpio, int value)
{
	gpio_cfg_t	t;
	void	*port;
	int		pin;

	memset(&t, 0, sizeof(t));
	t.mode = GPIO_M_OUT;
	port = GPIO_PORT_VADDR(GPIO_PORT(gpio));
	pin = GPIO_PIN(gpio);
	gpio_put(port, pin, value);
	gpio_cfg(port, pin, &t);
	return 0;
}

/*
gets the value of the pin.
@param[in]	gpio		gpio pin
@return 	value
*/
int gpio_get_value(unsigned gpio)
{
	return gpio_get(GPIO_PORT_VADDR(GPIO_PORT(gpio)), GPIO_PIN(gpio));
}


/*
sets the value of the pin.
@param[in]	gpio		gpio pin
@param[in]	value		value to be written
*/
void gpio_set_value(unsigned gpio, int value)
{
	gpio_put(GPIO_PORT_VADDR(GPIO_PORT(gpio)), GPIO_PIN(gpio), value);
}


/*
configures the pin for any desired functionality.
@param[in]	gpio_mode	32 bit mask for requested configuration. (for details, pls refer asm/arm/arch-magus/hardware.h)
@return 	0 on success, -ve value for error.
*/
int gpio_fn(unsigned long gpio_mode)
{

	unsigned int pin = gpio_mode & GPIO_PIN_MASK;
	unsigned int port = (gpio_mode & GPIO_PORT_MASK) >> GPIO_PORT_SHIFT;
	unsigned int osrc = (gpio_mode & GPIO_OCR_MASK) >> GPIO_OCR_SHIFT;
	unsigned int isrca = (gpio_mode & GPIO_AOUT_MASK) >> GPIO_AOUT_SHIFT;
	unsigned int isrcb = (gpio_mode & GPIO_BOUT_MASK) >> GPIO_BOUT_SHIFT;
	gpio_cfg_t	t;
	void	*port_addr;

	memset(&t, 0, sizeof(t));

	/* pull up enable ? */
	if( gpio_mode & GPIO_PUEN )
		t.pull_en = 1;

	/* GPIO / peripheral ? */
	if( gpio_mode & GPIO_MODE) {
		/* GPIO mode */
		if( gpio_mode &  GPIO_DIR_OUT) {
			t.mode = GPIO_M_OUT;
			switch(osrc) {
				case 0: /* AIN */
					t.oss = GPIO_OSS_A;
					break;
				case 1: /* BIN */
					t.oss = GPIO_OSS_B;
					break;
				case 2: /* CIN */
					t.oss = GPIO_OSS_C;
					break;
				case 3: /* DR */
					t.oss = GPIO_OSS_DREG;
					break;
				default: 
					return -1;
			}
		}
		else {
			t.mode = GPIO_M_IN;
			switch(isrca) {
				case 0: /* GIN */
					t.issa = GPIO_ISS_GIN;
					break;
				case 1: /* ISR */
					t.issa = GPIO_ISS_ISR;
					break;
				case 2: /* AOUT_0 */
					t.issa = GPIO_ISS_0;
					break;
				case 3: /* AOUT_1 */
					t.issa = GPIO_ISS_1;
					break;
				default: 
					return -1;
			}
			switch(isrcb) {
				case 0: /* GIN */
					t.issb = GPIO_ISS_GIN;
					break;
				case 1: /* ISR */
					t.issb = GPIO_ISS_ISR;
					break;
				case 2: /* AOUT_0 */
					t.issb = GPIO_ISS_0;
					break;
				case 3: /* AOUT_1 */
					t.issb = GPIO_ISS_1;
					break;
				default: 
					return -1;
			}

		}
	}
	else {
		/* peripheral mode */
		/* funtion - primary / alternate ? */
		if( gpio_mode & GPIO_AF)
			t.mode = GPIO_M_ALT;
		else
			t.mode = GPIO_M_PRI;


	}

	port_addr = GPIO_PORT_VADDR(port);
	gpio_cfg(port_addr, pin, &t);
	return 0;
}


EXPORT_SYMBOL(gpio_direction_input);
EXPORT_SYMBOL(gpio_direction_output);
EXPORT_SYMBOL(gpio_get_value);
EXPORT_SYMBOL(gpio_set_value);
EXPORT_SYMBOL(gpio_fn);
EXPORT_SYMBOL(gpio_request);
EXPORT_SYMBOL(gpio_free);

