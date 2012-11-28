#ifndef _OS_SASI_
#define _OS_SASI_


#include <linux/kernel.h>
#include <linux/string.h>

#define io_rd32(a)		(*(volatile uint32_t *)&(a))
#define io_wr32(a, d)	(*(volatile uint32_t *)&(a) = d)
#define dbg				printk


#endif

