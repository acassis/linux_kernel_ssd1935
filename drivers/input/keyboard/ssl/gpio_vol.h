#ifndef _GPIO_VOL_H
#define _GPIO_VOL_H
#include <asm/hardware.h>

#define UPX	0
#define DOWNX	1
#define UP 	2
#define DOWN	3
#define STEP 6

struct key_detect{
	int status;
	int irq;
};

typedef struct gpio_vol_platform_data{
	int code;
	int gpio[2];
	int active_low;
	char *desc;
	int type;
}
gpio_vol_data;

#define GET_MAX_VOL 0
#define GET_CUR_VOL 1
#define SET_VOL 2
#define GET_MIN_VOL 3
#define FM_CONTROL 4

#define GPIOVOL_SET	_IOW('f', 1, int)
#define GPIOVOL_GET	_IOR('f', 2, int)

#endif
