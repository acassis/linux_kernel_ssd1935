/**
@file		gpio.h - control APIs for General Purpose Input Output (GPIO) module
@author		shaowei@solomon-systech.com
@version	1.0
@date		created: 18JUL06, last modified 7AUG06
@todo  		submodule of GPIO: base reg + port reg
*/

#ifndef	_GPIO_H_
#define	_GPIO_H_


#define GPIO_GPR_PRI	0
#define GPIO_GPR_ALT	1

#define GPIO_PULL_NC	0
#define GPIO_PULL_LO	1
#define GPIO_PULL_HI	2

#define GPIO_IRQ_RS		0
#define GPIO_IRQ_FL		1
#define GPIO_IRQ_HI		2
#define GPIO_IRQ_LO		3

/** options for GPIO module's output source */
typedef enum
{
	GPIO_OSS_DREG = 0,
	GPIO_OSS_A = 1,
	GPIO_OSS_B = 2,
	GPIO_OSS_C = 3
}
gpio_outsrc_e;

/** options for GPIO module's input source */
typedef enum
{
	GPIO_ISS_GIN = 0,
	GPIO_ISS_ISR = 1,
	GPIO_ISS_0 = 2,
	GPIO_ISS_1 = 3
}
gpio_insrc_e;

/** operation mode of GPIO port */
typedef enum
{
	GPIO_M_PRI = 0,
	GPIO_M_ALT = 1,
	GPIO_M_OUT = 2,
	GPIO_M_IN = 3,
	GPIO_M_INTR = 4
}
gpio_mode_e;

/** GPIO API returns */
typedef enum
{
	GPIO_ERR_NONE = 0,		/** sucessful **/
	GPIO_ERR_HW = -1,		/** hardware error **/
	GPIO_ERR_PARM = -2,		/** parameter error **/
	GPIO_ERR_CFG = -3,		/** configuration error **/
} 
gpio_err_e;


/** configuration for gpio port */
typedef struct
{	
	gpio_mode_e	mode;	/**< mode of pins at this port */
	uint8_t	pull_en;	/**< 0 floating, 1/2 pull low/high */
	uint8_t	oss;		/**< 00 DREG, 01 AIN, 10 BIN, 11 CIN */
	uint8_t	issa;		/**< AOUT: 00 GIN, 01 ISR[Pin], 10 '0', 11 '1' */
	uint8_t	issb;		/**< BOUT: 00 GIN, 01 ISR[Pin], 10 '0', 11 '1' */		
	uint8_t	irq;		/**< 00 rising edge, 01 falling edge, 
							10 high level, 11 low level */
}
gpio_cfg_t, *gpio_cfg_p;


#ifndef gpio_p
#define gpio_p	void *
#endif

gpio_err_e gpio_init(gpio_p r);
/**<
initialize GPIO module
@param[in]	r	register base address
@return 	GPIO_ERR_XXX
*/

gpio_err_e gpio_reset(gpio_p r);
/**<
reset GPIO module
@param[in]	r	register base address
@return 	GPIO_ERR_XXX
*/

gpio_err_e gpio_cfg(gpio_p r, uint8_t pin, gpio_cfg_p cfg);
/**<
configure a pin
@param[in]	r		register base address
@param[in]	cfg		configuration for pin
@param[in]	pin		pin number
@return 		GPIO_ERR_XXX
*/

void gpio_put(gpio_p r, uint32_t pin, int value);
/**<
set gpio pin output to high
@param[in]	r		register base address
@param[in]	pin		pin number
@param[in]	value	0-clear, else set
*/

void gpio_set(gpio_p r, uint32_t pin);
/**<
set gpio pin output to high
@param[in]	r		register base address
@param[in]	pin		pin number
*/

void gpio_clr(gpio_p r, uint32_t pin);
/**<
set gpio pin output to low
@param[in]	r		register base address
@param[in]	pin		pin number
*/

uint32_t gpio_get(gpio_p r, uint8_t pin);
/**<
read the input of the port
@param[in]	r		register base address
@param[in]	pin		pin number
@return 	data input from GPIO pin
*/


/* interrupt */

void gpio_mask(gpio_p r, uint8_t pin);
/**<
mask the interrupt of a pin on a port
@param[in]	t		register base address
@param[in]	pinmask	32-bit pinmask for port
@param[in]	flag	0 - interrupt is not masked, 1 - interrupt is masked
*/

void gpio_unmask(gpio_p r, uint8_t pin);
/**<
unmask the interrupt of a pin on a port
@param[in]	t		register base address
@param[in]	pinmask	32-bit pinmask for port
@param[in]	flag	0 - interrupt is not masked, 1 - interrupt is masked
*/

void gpio_ack(gpio_p r, uint32_t pinmask);
/**<
acknowledge the interrupt of a pin on a port
@param[in]	t		register base address
@param[in]	pinmask	32-bit pinmask for port
@param[in]	flag	0 - interrupt is not masked, 1 - interrupt is masked
*/

uint32_t gpio_pend(gpio_p r);
/**<
check pending interrupts on a port
@param[in]	r		register base address
@return		pinmask of pending interrupts
*/


/* in groups */

gpio_err_e gpio_cfg_pins(gpio_p r, uint32_t pinmask, gpio_cfg_p cfg);
/**<
configure a port (32-pins) with mask
@param[in]	r		register base address
@param[in]	pinmask	mask of pins for the port, 32-bit for 32 pins
@param[in]	cfg		configuration for pin(s)
@return 		GPIO_ERR_XXX
*/

void gpio_set_pins(gpio_p r, uint32_t pinmask, uint32_t data);
/**<
set the data output to the port
@param[in]	r		register base address
@param[in]	pinmask	32-bit pinmask for port
@param[in]	data	data output from port
*/

uint32_t gpio_get_pins(gpio_p r, uint32_t pinmask);
/**<
read the input of the port
@param[in]	r		register base address
@param[in]	pinmask	32-bit pinmask for port
@return 	data input from GPIO pins
*/


/* macro implementation */
#define gpio_cfg(t, p, c)	gpio_cfg_pins((t), 1 << (p), (c))

void gpio_mask_port(gpio_p r, uint32_t pinmask, int flag);
#define gpio_mask(r, p)		gpio_mask_port(r, 1 << (p), 1)
#define gpio_unmask(r, p)	gpio_mask_port(r, 1 << (p), 0)

void gpio_ack_port(gpio_p r, uint32_t pinmask);
#define gpio_ack(r, p)		gpio_ack_port(r, 1 << (p))

#define gpio_put(r, p, v) \
	gpio_set_pins(r, 1 << (p), (v) ? (1 << (p)) : 0)
#define gpio_set(r, p)		gpio_set_pins(r, 1 << (p), 1 << (p))
#define gpio_clr(r, p)		gpio_set_pins(r, 1 << (p), 0)
#define gpio_get(r, p)		(gpio_get_pins(r, 1 << (p)) >> (p))


#endif
