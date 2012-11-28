#ifndef _OS_SASI_
#define _OS_SASI_


#include <linux/kernel.h>
#include <linux/string.h>
//#include <linux/time.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#define tm	rtc_time
#undef	RTC_SET_TIME

#define io_rd32(a)		(*(volatile uint32_t *)&(a))
#define io_wr32(a, d)	(*(volatile uint32_t *)&(a) = d)
#define io_rd16(a)		(*(volatile uint16_t *)&(a))
#define io_wr16(a, d)	(*(volatile uint16_t *)&(a) = d)
#define io_rd8(a)		(*(volatile uint8_t *)&(a))
#define io_wr8(a, d)	(*(volatile uint8_t *)&(a) = d)
#define dbg				printk
#define BF_GET(r, pos, width)	(((r) >> (pos)) & ((1 << (width)) - 1))
#define BF_SET(v, pos, width)	(((v) & ((1 << (width)) - 1)) << (pos))


#endif

