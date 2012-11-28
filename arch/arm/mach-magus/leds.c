/*
 * linux/arch/arm/mach-omap1/leds-osk.c
 *
 * LED driver for OSK, and optionally Mistral QVGA, boards
 */
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <asm/mach-types.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>

#include <asm/arch/gpio.h>
//#include <asm/arch/mux.h>



#define LED_STATE_ENABLED	(1 << 0)
#define LED_STATE_CLAIMED	(1 << 1)
#define LED_TIMER_ON        0x04

static u8 led_state;




/* For now, all system indicators require the Mistral board, since that
 * LED can be manipulated without a task context.  This LED is either red,
 * or green, but not both; it can't give the full "disco led" effect.
 */

#define GPIO_LED_BLUE		GPIO_NUM(5, 23)
#define GPIO_LED_RED		GPIO_NUM(5, 11)
#define GPIO_LED_GREEN		GPIO_NUM(5, 22)


void acciop1_leds_event(led_event_t evt)
{
	unsigned long	flags;

	local_irq_save(flags);

	if (!(led_state & LED_STATE_ENABLED) && evt != led_start)
		goto done;

	switch (evt) {
	case led_start:
		led_state |= LED_STATE_ENABLED;
		gpio_direction_output(GPIO_LED_BLUE, 1);
		break;

	case led_halted:
	case led_stop:
		// NOTE:  work may still be pending!!
		led_state &= ~LED_STATE_ENABLED;
		gpio_direction_output(GPIO_LED_BLUE, 0);
		break;

	case led_claim:
		break;

	case led_release:
		break;


	case led_timer:
		/*
		led_state ^= LED_TIMER_ON;
		gpio_direction_output(GPIO_LED_GREEN,  led_state & LED_TIMER_ON);
		*/
		break;

	case led_idle_start:	/* idle == off */
		gpio_direction_output(GPIO_LED_RED, 0); // OFF LED
		break;

	case led_idle_end:
		gpio_direction_output(GPIO_LED_RED, 1); // ON LED
		break;

	/* "green" == tps LED1 (leftmost, normally power-good)
	 * works only with DC adapter, not on battery power!
	 */
	case led_green_on:
		break;
	case led_green_off:
		break;

	/* "amber" == tps LED2 (middle) */
	case led_amber_on:
		break;
	case led_amber_off:
		break;

	/* "red" == LED on tps gpio3 (rightmost) */
	case led_red_on:
		break;
	case led_red_off:
		break;

	default:
		break;
	}

done:
	local_irq_restore(flags);
}

static int __init ssl_leds_init(void)
{
	leds_event = acciop1_leds_event;
	leds_event(led_start);
	printk("****** System LEDs For Magus ******\n");
	return 0;
}

__initcall(ssl_leds_init);
