/**
@file		rtc.h - control API for Real Time Clock (RTC)
@author		shaowei@solomon-systech.com
@version	1.0
@date		created: 3Aug06
@todo		alarm and event interrupt enable bits
*/

#ifndef	_RTC_H_
#define	_RTC_H_


/* interrupt */
#define RTC_INT_PSTALL	(0xFF << 16)
#define RTC_INT_PST(d)	(1 << (16 + d))
#define RTC_INT_EVENT	(1 << 9)
#define RTC_INT_ALARM	(1 << 8)
#define RTC_INT_2HZ		(1 << 7)
#define RTC_INT_STPW	(1 << 6)
#define RTC_INT_DAY		(1 << 3)
#define RTC_INT_HR		(1 << 2)
#define RTC_INT_MIN		(1 << 1)
#define RTC_INT_SEC		(1 << 0)

static const uint8_t RTC_MDAYS[] = 
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define RTC_LEAP_YEAR(year)	\
	(!((year) & 3) && (((year) % 100) || !((year) % 400)))

#define RTC_MONTH_DAYS(month, year) \
	(RTC_MDAYS[(month)-1] + (RTC_LEAP_YEAR((year)) && ((month) == 2)))

/** RTC API returns */
typedef enum
{
	RTC_ERR_NONE = 0,		/**< sucessful **/
	RTC_ERR_HW = -1,		/**< hardware error **/
	RTC_ERR_PARM = -2,		/**< parameter error **/
	RTC_ERR_CFG = -3,		/**< configuration error **/
} 
rtc_err_e;


/** rtc module context */
typedef struct
{
/** public - initialized by caller before rtc_init */ 
	void		*r;				/**< base address of registers */
	uint8_t		clk;			/**< reference crystal */

/** private */	
	uint16_t	ver;			/**< version */
}
rtc_t, *rtc_p;


/** Device APIs */


rtc_err_e rtc_init(rtc_p t);
/**<
initialize RTC module
@param[in] t	context
@return 	RTC_ERR_XXX
*/

void rtc_enable(rtc_p t, uint8_t en);
/**<
enable/disable RTC module
@param[in] t	context
@param[in] en	0 - disable, else - enable
*/

rtc_err_e rtc_settime(rtc_p t, struct tm *time);
/**<
set time (including year, month, day, hour, minute and sec)
@param[in] t	context
@param[in] time	ctime setting
@return 	RTC_ERR_XXX
*/

void rtc_gettime(rtc_p t, struct tm *time);
/**<
read time (including year, month, day, hour, minute and sec)
@param[in]  t		context
@param[out] time	rtc time
@return 		RTC_ERR_XXX
*/

void rtc_set_intr(rtc_p t, uint32_t mask, uint8_t mode);
/**<
enable/disable interrupt (PST7~0, EVENT, ALARM, HZ2, SW, DAY, HR, MIN, SEC)
@param[in] t	context
@param[in] mask	bit to be operated. bit=0 are intact.
@param[in] mode 1 - enable interrupt, 0 - disable interrupt
@return 	RTC_ERR_XXX
*/

uint32_t rtc_get_intr(rtc_p t);
/**<
get the interrupt enable status
@param[in] t	context
@return		interrupt enabled status, 1 - enabled, 0 - disabled
*/


uint32_t rtc_get_stat(rtc_p t);
/**<
get the interrupt status
@param[in] t	context
@return		interrupt status, 1 - pending, 0 - no pending
*/


rtc_err_e rtc_alarm_set(rtc_p t, struct tm *time, int enable);
/**<
set alarm on (including hour, min and sec) / off
@param[in] t		context
@param[in] time		configuration of alarm setting; if NULL, no change
@param[in] enable	enable/disable interrupt
@return 	RTC_ERR_XXX
*/

int rtc_alarm_get(rtc_p t, struct tm *time);
/**<
read alarm settings
@param[in]  t		context
@param[out] time	alarm time
@return bits 0:enabled 1:pending
*/

rtc_err_e rtc_set_event(rtc_p t, struct tm *time);
/**<
set calender event on/off (including month, day, hour, min and sec)
@param[in] t	context
@param[in] cfg	configuration of calender event setting; if NULL, turn off event interrupt
@return 	RTC_ERR_XXX
*/

void rtc_get_event(rtc_p t, struct tm *time);
/**<
read calender event time (including month, day, hour, min, and sec)
@param[in]  t		context
@param[out] time	calender event time
@return 		RTC_ERR_XXX
*/

int rtc_set_stopwatch(rtc_p t, uint8_t mins);
/**<
set and enable the stop watch
@param[in] t	context
@param[in] mins	minutes
@return		RTC_ERR_XXX
*/

//void rtc_set_12h(rtc_p t, uint8_t h12)
/*
set time dislay mode: 12H or 24H (switch between am, pm and 24H)
@param[in] t	context
@param[in] h12	1 - 12H mode, 0 - 24H mode 
*/

rtc_err_e rtc_exit(rtc_p t);
/**<
reset and exit RTC module
@param[in] t	context
@return 	RTC_ERR_XXX
*/

int rtc_isr(rtc_p t);
/**< 
interrupt service routine
@param[i] t	context
@return 	0 - not processed, else processed
*/


void rtc_intr_clear(rtc_p t, uint32_t intr_mask);

#define rtc_alarm_on(t)		rtc_set_intr((t), RTC_INT_ALARM, 1)
#define rtc_alarm_off(t)	rtc_set_intr((t), RTC_INT_ALARM, 0)
#define rtc_event_on(t) 	rtc_set_intr((t), RTC_INT_EVENT, 1)
#define rtc_event_off(t)	rtc_set_intr((t), RTC_INT_EVENT, 0)
#define rtc_sec_on(t) 		rtc_set_intr((t), RTC_INT_SEC, 1)
#define rtc_sec_off(t)		rtc_set_intr((t), RTC_INT_SEC, 0)
#define rtc_pst_on(t, f)	rtc_set_intr((t), RTC_INT_PST(f), 1)
#define rtc_pst_off(t)		rtc_set_intr((t), RTC_INT_PSTALL | RTC_INT_2HZ, 0)
#define rtc_alarm_is_on(t) 	((rtc_get_intr((t)) & RTC_INT_ALARM) >> 8)
#define rtc_event_is_on(t) 	((rtc_get_intr((t)) & RTC_INT_EVENT) >> 9)
#define rtc_sec_is_on(t) 	(rtc_get_intr((t)) & RTC_INT_SEC)
//#define rtc_alarm_is_pending(t)	((rtc_get_status((t)) & RTC_INT_ALARM) >> 8)


#define rtc_alarm_clear(t)	rtc_intr_clear((t), RTC_INT_ALARM)
#define rtc_event_clear(t)	rtc_intr_clear((t), RTC_INT_EVENT)
#define rtc_sec_clear(t)	rtc_intr_clear((t), RTC_INT_SEC)
#define rtc_sec_clear(t)	rtc_intr_clear((t), RTC_INT_SEC)
#define rtc_pst_clear(t)	rtc_intr_clear((t), RTC_INT_PSTALL | RTC_INT_2HZ)


#endif
