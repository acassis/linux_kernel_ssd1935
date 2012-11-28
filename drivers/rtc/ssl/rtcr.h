/**
@file		rtcr.h - hardware dependent definitions for Real Time Clock (RTC)
@author		shaowei@solomon-systech.com
@version	1.0
@date		created: 2Aug06
@todo  
*/

#ifndef	_RTCR_H_
#define	_RTCR_H_


/*
#define RTC_REG_IDR	0x0
#define RTC_REG_CAPR	0x4
#define RTC_REG_CTL	0x8
#define RTC_REG_DATE	0x0c
#define RTC_REG_TIME	0x10
#define RTC_REG_DAY	0x14
#define RTC_REG_IER	0x18
#define RTC_REG_ISR	0x1c
#define RTC_REG_ALARM	0x20
#define RTC_REG_EVDATE	0x24
#define RTC_REG_EVTIME	0x28
#define RTC_REG_SWR	0x2c
*/


#pragma pack(4)

/* register */
typedef struct
{
	uint32_t	id;			/**< device identification register */
	uint32_t	cap;		/**< capabilities register */
	uint32_t	ctl;		/**< control register */
	uint32_t	date;		/**< year, month and day register */
	uint32_t	time;		/**< hour, minute and second register */	
	uint32_t	day;		/**< day of week register */
	uint32_t	ier;		/**< interrupt enable register */
	uint32_t	isr;		/**< interrupt status register */
	uint32_t	alarm;		/**< hour, minute and second alarm register */
	uint32_t	evdate;		/**< month and day alarm register */
	uint32_t	evtime;		/**< event hour, minute and second register */
	uint32_t	stpwatch;	/**< stop watch register */
//	uint32_t	_rsv[64-11*4];		
//	uint32_t	tst;		/**< test register */
//	uint32_t	dbg;		/**< debug register */
}
volatile rtcr_t, *rtcr_p;

#pragma pack()


/* ID */
#define RTC_IDV_CLSDSG		(0x0803 << 6)
#define RTC_ID_CLSDSG(d)	((d) >> 10)
#define RTC_ID_VER(d)		((d) & 0x3FF)

/* control */
#define RTC_CTL_SB		(1 << 9)
#define RTC_CTL_UD		(1 << 8)
#define RTC_CTL_AM		(1 << 7)
#define RTC_CTL_12H		(1 << 6)
#define RTC_CTL_F32768	0
#define RTC_CTL_F32000	1
#define RTC_CTL_F38400	2
#define RTC_CTL_FRQ(f)	((f) << 2)
#define RTC_CTL_RST		(1 << 1)
#define RTC_CTL_EN		(1 << 0)

/* date */
#define RTC_DATE_YEAR(dt)	((dt) >> 16)
#define RTC_DATE_MONTH(dt)	(((dt) >> 8) & 0xFF)
#define RTC_DATE_DAY(dt)	((dt) & 0xFF)
#define RTC_DATE(y, m, d)	(((y) << 16) | ((m) << 8) | (d))

/* time */
#define RTC_TIME_HOUR(tm)	((tm) >> 16)
#define RTC_TIME_MIN(tm)	(((tm) >> 8) & 0xFF)
#define RTC_TIME_SEC(tm)	((tm) & 0xFF)
#define RTC_TIME(h, m, s)	(((h) << 16) | ((m) << 8) | (s))

/* weekday */
#define RTC_WDAY	0x7

/* IER/ISR bits */
#define RTC_INT_PST(d)	(1 << (d + 16))
#define RTC_INT_EVENT	(1 << 9)
#define RTC_INT_ALARM	(1 << 8)
#define RTC_INT_2HZ		(1 << 7)
#define RTC_INT_STPW	(1 << 6)
#define RTC_INT_DAY		(1 << 3)
#define RTC_INT_HR		(1 << 2)
#define RTC_INT_MIN		(1 << 1)
#define RTC_INT_SEC		(1 << 0)


#endif
