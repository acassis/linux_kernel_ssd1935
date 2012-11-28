/**
@file		rtc.c - control code for Real Time Clock (RTC)
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 3Aug2006
@remark		LEAP YEAR: There is a leap year every year divisible by four except for 
@		years which are both divisible by 100 and not divisible by 400. 
@		Therefore, the year 2000 will be a leap year, but the years 1700, 
@		1800, and 1900 were not. 
@		DAYLIGHT SAVING: On the first Sunday in April, the time increments 
@		from 1:59:59 AM to 3:00:00 AM. On the last Sunday in October when 
@		the time first reaches 1:59:59 AM, it changes to 1:00:00 AM.
*/

#include "os.h"

#include "rtc.h"
#include "rtcr.h"


//#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)
#define RTC_SET_TIME	1
#define RTC_SET_ALARM	2
#define RTC_SET_EVENT	3

#define _TIMEOUT	10000
static rtc_err_e rtc_reset(rtc_p t);


/* device based APIs */

rtc_err_e rtc_init(rtc_p t)
{
	uint32_t	id;
	rtc_err_e	err;
	rtcr_p	r = (rtcr_p)t->r;

	/* check ID register and capacities */
	id = io_rd32(r->id);

	if (RTC_ID_CLSDSG(id) != RTC_IDV_CLSDSG)
	{
		dbg("rtc: init err - class/dev id mismatch exp=%08X got=%08X\n", 
			RTC_IDV_CLSDSG, RTC_ID_CLSDSG(id));
		return RTC_ERR_HW;
	}
	t->ver = RTC_ID_VER(id);
dbg("rtc: init info - ver=%X\n", t->ver);

	/* reset the module */
	err = rtc_reset(t);
	if (err)
	{
		return err;
	}

	/* set 24H mode and crystal reference */
	io_wr32(r->ctl, (t->clk << 2) | RTC_CTL_EN);

	return RTC_ERR_NONE;
}


rtc_err_e rtc_settime(rtc_p t, struct tm *time)
{
	uint32_t	val, tmp, i = _TIMEOUT * 100;
	rtcr_p	r = (rtcr_p)t->r;

#ifdef ADJUST_LEAP_YEAR
	rtc_intr_clear(&rtc->hw, RTC_INT_DAY);
#endif

	io_wr32(r->day, time->tm_wday);

	i = _TIMEOUT*100;
	do
	{
		tmp = io_rd32(r->day);
	}
	while ((tmp!=time->tm_wday) && (i--));

	val = RTC_DATE(time->tm_year, time->tm_mon, time->tm_mday);
	io_wr32(r->date, val);
	i=_TIMEOUT*100;
	do
	{
		tmp = io_rd32(r->date);
	}
	while ((tmp!=val) && (i--));

	val = RTC_TIME(time->tm_hour, time->tm_min, time->tm_sec);
	i=_TIMEOUT*100;
	do
	{
		io_wr32(r->time, val);
		tmp = io_rd32(r->time);
	}
	while ((tmp!=val) && (i--));

#ifdef ADJUST_LEAP_YEAR
#warning "leap year"
	rtc_set_intr(&rtc->hw, RTC_INT_DAY, 1);
#endif

	return RTC_ERR_NONE;
}


void rtc_gettime(rtc_p t, struct tm *time)
{
	uint32_t	tm;
	rtcr_p	r = (rtcr_p)t->r;

	tm = io_rd32(r->date);
	time->tm_year = RTC_DATE_YEAR(tm);
	time->tm_mon = RTC_DATE_MONTH(tm);
	time->tm_mday = RTC_DATE_DAY(tm);
	time->tm_wday = io_rd32(r->day) & RTC_WDAY;
	tm = io_rd32(r->time);
	time->tm_hour = RTC_TIME_HOUR(tm);
	time->tm_min = RTC_TIME_MIN(tm);
	time->tm_sec = RTC_TIME_SEC(tm);
}


void rtc_set_intr(rtc_p t, uint32_t mask, uint8_t mode)
{
	uint32_t	ier;
	rtcr_p	r = (rtcr_p)t->r;

	ier = io_rd32(r->ier);
	ier &= ~mask;
	if (mode)
	{
		ier |= mask;
	}
	io_wr32(r->ier, ier);
}


uint32_t rtc_get_intr(rtc_p t)
{
	uint32_t tmp = io_rd32(((rtcr_p)t->r)->ier);
//dbg("p_id=%x, p_ier=%x, ier=%x\n", (rtcr_p)t->r, &(((rtcr_p)t->r)->ier), tmp);
	return tmp;
}


uint32_t rtc_get_stat(rtc_p t)
{
	return io_rd32(((rtcr_p)t->r)->isr);
}


rtc_err_e rtc_alarm_set(rtc_p t, struct tm *time, int enable)
{
	rtcr_p		r = (rtcr_p) t->r;
	uint32_t	d;

	if (time)
	{
		io_wr32(r->alarm, RTC_TIME(time->tm_hour, time->tm_min, time->tm_sec));
	}
	d = io_rd32(r->ier);
	if (!enable != !(d & RTC_INT_ALARM))
	{
		io_wr32(r->isr, RTC_INT_ALARM);
		/* wait for bit to clear */
		while (io_rd32(r->isr) & RTC_INT_ALARM)
		{
			;
		}
		io_wr32(r->ier, d ^ RTC_INT_ALARM);
	}
	return RTC_ERR_NONE;
}


int rtc_alarm_get(rtc_p t, struct tm *time)
{
	rtcr_p	r = (rtcr_p)t->r;

	if (time)
	{
		uint32_t	tm;

		tm = io_rd32(r->alarm);
		time->tm_hour = RTC_TIME_HOUR(tm);
		time->tm_min = RTC_TIME_MIN(tm);
		time->tm_sec = RTC_TIME_SEC(tm);
	}
	return ((io_rd32(r->ier) & RTC_INT_ALARM) ? 1 : 0) | 
				((io_rd32(r->isr) & RTC_INT_ALARM) ? 2 : 0);
}


rtc_err_e rtc_set_event(rtc_p t, struct tm *time)
{
	rtcr_p	r = (rtcr_p) t->r;

	if (!time)
	{
		io_wr32(r->ier, io_rd32(r->ier) & ~RTC_INT_EVENT);
		return 0;
	}

	io_wr32(r->evdate, RTC_DATE(0, time->tm_mon, time->tm_mday));
 	io_wr32(r->evtime, RTC_TIME(time->tm_hour, time->tm_min, time->tm_sec));
	io_wr32(r->ier, io_rd32(r->ier) | RTC_INT_EVENT);

	return RTC_ERR_NONE;
}


void rtc_get_event(rtc_p t, struct tm *time)
{
	uint32_t	tm;
	rtcr_p	r = (rtcr_p) t->r;

	tm = io_rd32(r->evdate);
	time->tm_mon = RTC_DATE_MONTH(tm);
	time->tm_mday = RTC_DATE_DAY(tm);

	tm = io_rd32(r->evtime);
	time->tm_hour = RTC_TIME_HOUR(tm);
	time->tm_min = RTC_TIME_MIN(tm);
	time->tm_sec = RTC_TIME_SEC(tm);
}


int rtc_set_stopwatch(rtc_p t, uint8_t mins)
{
	rtcr_p	r = (rtcr_p)t->r;

	if (mins >= 32)
	{
		dbg("rtc: setstopwatch err - invalid time\n");
		return RTC_ERR_PARM;
	}
	io_wr32(r->stpwatch, mins);
	io_wr32(r->ier, io_rd32(r->ier) | RTC_INT_STPW);
	return RTC_ERR_NONE;
}


static rtc_err_e rtc_reset(rtc_p t)
{
	int	i;
	rtcr_p	r = (rtcr_p)t->r;

	io_wr32(r->ctl, RTC_CTL_RST | RTC_CTL_EN);
	/* reset timeout */
	i = _TIMEOUT;
	while (io_rd32(r->ctl) != RTC_CTL_EN)
	{
		if (!i--)
		{
			dbg("rtc: reset err - reset hang \n");
			return RTC_ERR_HW;
		}
	}
	return RTC_ERR_NONE;
}


rtc_err_e rtc_exit(rtc_p t)
{
	rtc_err_e	err;
	rtcr_p	r = (rtcr_p)t->r;

	err = rtc_reset(t);
	if (err)
	{
		return RTC_ERR_HW;
	}

	/* disable module */
	io_wr32(r->ctl, 0);
//	dbg("rtc: exit info \n");
	return RTC_ERR_NONE;
}


int rtc_isr(rtc_p t)
{
	uint32_t	ier, sta;
	rtcr_p	r = (rtcr_p)t->r;

	ier = io_rd32(r->ier);
	sta = ier & io_rd32(r->isr);
	if (!sta)
	{
		dbg("rtc: isr err - none\n");
		return 0;
	}
	io_wr32(r->isr, sta);

	if (sta & RTC_INT_DAY)
	{
#ifdef ADJUST_LEAP_YEAR
#warning "adjust leap year manually"
		uint32_t date = io_rd32(r->date);

		if ((date & 0x1f) == 29 && (date & 0x1f00) == 0x200 && 
			(!RTC_LEAP_YEAR(date >> 16)))
		{
			/* if not leap year, jump from Feb 29 to March 1 */
			io_wr32(r->date, ((date & 0xffff0000) | 0x301));
		}
#endif
	}

	return sta;
}


void rtc_intr_clear(rtc_p t, uint32_t intr_mask)
{
	rtcr_p	r = (rtcr_p)t->r;

	io_wr32(r->isr, intr_mask);
	while (io_rd32(r->isr) & intr_mask)
	{
		;
	}
}

