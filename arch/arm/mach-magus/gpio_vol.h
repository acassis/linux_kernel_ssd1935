#ifndef _GPIO_VOL_H
#define _GPIO_VOL_H

typedef struct gpio_vol_platform_data{
	int code;
	int gpio[2];
	int active_low;
	char *desc;
	int type;
}
gpio_vol_data;

#endif
