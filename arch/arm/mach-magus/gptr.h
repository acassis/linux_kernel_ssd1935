/**
@file		gptr.h - hardware dependent definitions for General Purpose Timer (GPT) module
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 25JUL06
@todo
*/

#ifndef _GPTR_SHAOWEI_H_
#define _GPTR_SHAOWEI_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include "stdint.h"
#endif

#define _GPT_CLID	0x0802		/**< Module ID */
#define _GPT_DSGNR	0x00		/**< Module Desgin number */


#pragma pack(4)

/* register of GPT module */
typedef struct
{
	uint32_t	idr;		/**< GPT ID register */
	uint32_t	capr;		/**< GPT capability register */
	uint32_t	ctrl;		/**< GPT control register */
	uint32_t	ier;		/**< GPT interrupt enable register */
	uint32_t	isr;		/**< GPT interrupt status register */
	uint32_t	pslr;		/**< GPT prescaler register */
	uint32_t	cnt;		/**< GPT counter register */
	uint32_t	comp;		/**< GPT compare register */
	uint32_t	capt;		/**< GPT capture register */
//	uint32_t	tst;		/**< GPT test register */
}
volatile gpt_reg_t, *gpt_reg_p;

#pragma pack()


/* ID */
#define GPT_ID_CLID(d)	((d) >> 16)
#define GPT_ID_DSGNR(d)	(((d) >> 10) & 0x3F)
#define GPT_ID_MAJ(d)	(((d) >> 6) & 0x0F)
#define GPT_ID_MIN(d)	((d) & 0x3F)
/* control */
#define GPT_CTRL_RST	(1 << 1)
#define GPT_CTRL_EN	1
#define GPT_CTRL_DIS	0
#define GPT_CTRL_CAPEN	(1 << 11)
#define GPT_CTRL_COMPEN	(1 << 10)
#define GPT_CTRL_CLKSEL	(3 << 8)
#define GPT_CTRL_EDGEDET	(3 << 5)
#define GPT_CTRL_TOUTSEL	(1 << 4)
#define GPT_CTRL_FRS	(1 << 3)
#define GPT_CTRL_CLRCNT	(1 << 2)
/* interrupt */
#define GPT_INT_COMP	1
#define GPT_INT_CAP	(1 << 1)


#endif
