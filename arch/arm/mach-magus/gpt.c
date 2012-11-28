/**
@file		gpt.c - control code for General Purpose Timer (GPT) module
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 25JUL06, last modified 23AUG06
@remark		SW should always soft-reset the IP core after it is enabled. 
3 modes: 
(a) system timer has the counter restart and with interrupt
(b) profile timer has the counter free-running and no interrupt. It also acts as the delay timer.
(c) capture timer has the counter free-running but with interrupt
@todo		ext_tin fequency
*/

#include "os.h"
#include "gptr.h"
#include "gpt.h"


/** GPT clock select */
typedef enum
{
	GPT_CLK_NONE = 0,			/**< 0 (default) */
	GPT_CLK_PER = 1,			/**< perclk */
	GPT_CLK_32K = 2,			/**< clk_32k */
	GPT_CLK_EXT = 3				/**< ext_tin */
}
gpt_clk_e;

/** GPT edge detection select */
typedef enum
{
	GPT_EDG_NONE = 0,			/**< no detection (default) */
	GPT_EDG_RIS = 1,			/**< rising edge detection */
	GPT_EDG_FAL = 2,			/**< falling edge detection */
	GPT_EDG_RIFA = 3			/**< rising and falling edge detection */
}
gpt_edge_e;

/** clock divide setting & compare value*/
typedef struct
{
/* opague - retrieved from user frequency requirement*/
	gpt_clk_e	clksel;
	uint16_t	pslr;
	uint32_t	comp_val;
}
gpt_clk_t, *gpt_clk_p;

//#define _DO_RESET	// if defined, always do a reset before any configure (reset counter)
#define _TIMEOUT	300
#define CLK_32K	( 1 << 15 )		// 32,678 Hz

//#define DEFAULT_SYSTEM_TIMER_FREQ	100
//#define DEFAULT_PROFILE_TIMER_FREQ	10000000

#define MIN( x, y ) ( ( (x) > (y) ) ? (y) : (x) )

/* determine pslr, clksel, and comp_val using user_frequency */
static gpt_err_e gpt_setclk( gpt_p t, uint8_t mode, gpt_clk_p clkparm );
static gpt_err_e gpt_reset( gpt_p t );
/**<
reset GPT module
@param[in] t	context
@return 	GPT_ERR_XXX
*/


/** device based API */

gpt_err_e gpt_init( gpt_p t )
{
	uint32_t	id, comp; 
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;


	if (!t->clk32k || (t->clk32k != 32000 && t->clk32k != 32768))
	{
		dbg("gpt: init warn - clk32k %d, using 32000\n", t->clk32k);
		t->clk32k = 32000;
	}

	
	/* check ID register and capacities */
	id = io_rd32( reg->idr );

	if (GPT_ID_CLID(id) != _GPT_CLID)
	{
		dbg("gpt: init err - class/dev id mismatch "
			"exp=0x%x, get=0x%x, id=0x%x\n", 
			_GPT_CLID, GPT_ID_CLID(id), id);
		return GPT_ERR_HW;
	}
	t->major = GPT_ID_MAJ( id );
	t->minor = GPT_ID_MIN( id );
	
	/* reset module */
	err = gpt_reset( t );
	if (err)
	{
		return GPT_ERR_HW;
	}

	comp = io_rd32( reg->comp );
	if ( comp != ~0 )
	{
		dbg("gpt: init err - compare register reset value "
			"exp=%X, get=%32X\n", ~0, comp);
		return GPT_ERR_HW;
	}

	/* disable module */
	io_wr32( reg->ctrl, GPT_CTRL_DIS );
	
	t->usage = 0;		/* timer reference count */
	
	return GPT_ERR_NONE;
}


int gpt_sys_start( gpt_p t, uint32_t freq )//, gpt_callback_p callback )
{
	uint32_t	ctrl;	
	gpt_clk_t	clkparm;
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;

	t->freq = freq;
	err = gpt_setclk( t, GPT_M_SYSTEM, &clkparm );
	if ( err )
	{
		return err;
	}

	ctrl = GPT_CTRL_EN;
#ifdef _DO_RESET
	err = gpt_reset( t );		/* always reset counter for initialization */
	if ( err )
	{
		return err;
	}
#else
#warning "no reset"
	io_wr32( reg->ctrl, ctrl );		// counter is frozen before clksel is set 
#endif

	io_wr32( reg->pslr, clkparm.pslr );	
	// callback initialized before sys_start
	//t->callback = *callback;

	io_wr32( reg->ier, GPT_INT_COMP );
	io_wr32( reg->comp, clkparm.comp_val );	

	//  frs = 0, restart mode
	ctrl |= ( clkparm.clksel << 8 ) & GPT_CTRL_CLKSEL;	
	ctrl |= GPT_CTRL_COMPEN;
	t->mode = GPT_M_SYSTEM;
	io_wr32( reg->ctrl, ctrl );			// start the counter	

#if 0
dbg("gpt: sys_start info - ctrl=%X, ier=%X, isr=%X, "
	"pslr=%X, cnt=%X, comp=%X, cap=%X\n", 
io_rd32(reg->ctrl), io_rd32(reg->ier), io_rd32(reg->isr), 
io_rd32(reg->pslr), io_rd32(reg->cnt), io_rd32(reg->comp), io_rd32(reg->capt));
#endif

	return (clkparm.clksel == GPT_CLK_PER);
}


gpt_err_e gpt_profile_start( gpt_p t, uint32_t *tick )
/*
configure gpt as a profile timer (free-running counter), 
read the current tick and increase the reference count by 1
@param[in]	t		context
@param[out]	tick	current tick value (counter value)
@return 	GPT_ERR_XXX
@remark		This function is used to meansure the time. If time resolution 
			is set to 100 nanosecond (tick frequency = 10 MHz), 
			the longest elapsed time it can measure is ~6.67s
*/
{
	uint32_t	ctrl;
	gpt_clk_t	clkparm;
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;
	
	if (!t->usage)
	{		
		err = gpt_setclk( t, GPT_M_PROFILE, &clkparm );
		if ( err )
		{
			return err;
		}
				
		ctrl = GPT_CTRL_EN;
#ifdef _DO_RESET
		err = gpt_reset( t );		// always reset counter for initialization
		if ( err )
		{
			return err;
		}
#else
		io_wr32( reg->ctrl, ctrl );	// counter is freezed before clksel is set 
#endif
		io_wr32( reg->pslr, clkparm.pslr );		// prescaler of input clock

//		t->callback = *callback;
//		io_wr32( reg->ier, GPT_INT_COMP );
//		io_wr32( reg->comp, clkparm.comp.val );

		//  frs = 1, free-running mode
		ctrl |= ( (clkparm.clksel << 8) & GPT_CTRL_CLKSEL) | GPT_CTRL_FRS;	
//		ctrl |= GPT_CTRL_COMPEN;
	
		t->mode = GPT_M_PROFILE;
		io_wr32( reg->ctrl, ctrl );			// start the counter
	}
	t->usage++;
	*tick = io_rd32( reg->cnt );
//	dbg("gpt: profile_start info\n");
	return GPT_ERR_NONE;
}


void gpt_profile_end( gpt_p t, uint32_t *tick )
/*
read the current tick, and stop the profile timer if reference count 
decreased to 0
@param[in] t		context
@param[out] tick	current tick (counter value)
*/
{	
	gpt_reg_p	reg = (gpt_reg_p)t->reg;	
	
	*tick = io_rd32( reg->cnt );
	if ( !(--(t->usage)) )		// disable module if nobody is using it
	{
		io_wr32( reg->ctrl, GPT_CTRL_DIS );
	}
}


uint32_t gpt_tick( gpt_p t )
/*
read current tick (counter value)
@param[in] t	context
return counter value (ticks)
*/
{
	uint32_t	ticks;
	gpt_reg_p	r = t->reg;

	ticks = io_rd32(r->cnt);
	if (ticks)
	{
		ticks = io_rd32(r->pslr) + 1;
	}
	if (io_rd32(r->isr) & GPT_INT_COMP)
	{
		ticks += io_rd32(r->pslr) + 1;
	}
	return ticks;
}


gpt_err_e gpt_time_udelay( gpt_p t, uint32_t usec, gpt_callback_p callback )
/*
Time delay using profile timer
@param[in]	t	Context
@param[in]	usec	microseconds to delay
@param[in]	callback	callback isr for timer
@return		GPT_ERR_XXX
@remark		if callback = NULL, busy-waiting by polling
			else, timer interrupt for callback event
*/
{
	uint32_t	cnt, ticks;
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;

	
	if ( !callback )
	{
		/* busy waiting by polling */
		err = gpt_profile_start( t, &cnt );
		if ( err )
		{
			return err;
		}
		
		ticks = usec * t->freq / 1000000;
//dbg("gpt: time_udelay info - time delay cannot be > %u usec\n", 
//ticks * 1000000 / t->freq);
		if (ticks + cnt < cnt)
		{
			while (io_rd32(reg->cnt) <= ~0)	
			{
				;
			}
		}
		cnt += ticks;
		while (io_rd32(reg->cnt) < cnt)
		{
			;
		}
		gpt_profile_end(t, &cnt);
	}
	else 
	{
		gpt_clk_t	clkparm;
		uint32_t	ctrl;

		/* timer interrupt */
		if ( t->usage )
		{
			ctrl = io_rd32( reg->ctrl );
		}
		else
		{
			err = gpt_setclk( t, GPT_M_PROFILE, &clkparm );
			if ( err )
			{
				return err;
			}
			io_wr32( reg->pslr, clkparm.pslr );
			ctrl = GPT_CTRL_EN;
			ctrl |= ( clkparm.clksel << 8 ) & GPT_CTRL_CLKSEL;
			/* frs = 1, free-run mode */
			ctrl |= GPT_CTRL_FRS;
		}		
		ticks = usec * t->freq / 1000000;
//dbg("gpt: time_mdelay info - time delay can not be > %u msec\n",
//ticks * 1000 / t->freq );
			
		t->callback = *callback;
		io_wr32( reg->ier, GPT_INT_COMP );
		io_wr32( reg->comp, clkparm.comp_val + io_rd32( reg->cnt ) );
		
		ctrl |= GPT_CTRL_COMPEN;
		
		t->mode = GPT_M_PROFILE;
		io_wr32( reg->ctrl, ctrl );	/* start the counter */
		
		t->usage++;			/* need to decrease usage by 1 in isr */
	}
	return GPT_ERR_NONE;
}


gpt_err_e gpt_profile_capture( gpt_p t, uint8_t edgedet, 
				gpt_callback_p callback, uint32_t *tick )
/*
configure the profile timer as capture
@param[in]     t		context
@param[in]     edgedet		edge detection option for capture event
@param[in]     callback		callback event and context
@param[out]    tick		initial counter value
@return GPT_ERR_XXX
*/
{
	uint32_t	ctrl;
	gpt_clk_t	clkparm;
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;
	
	if ( t->usage )
	{
		ctrl = io_rd32(reg->ctrl);
	}
	else
	{
		err = gpt_setclk( t, GPT_M_CAPTURE, &clkparm );
		if ( err )
		{
			return err;
		}
				
		ctrl = GPT_CTRL_EN;
#ifdef _DO_RESET
		err = gpt_reset( t );	// always reset counter for initialization
		if ( err )
		{
			return err;
		}
#else
		io_wr32( reg->ctrl, ctrl );	// counter is freezed before clksel is set 
#endif
		io_wr32( reg->pslr, clkparm.pslr );		// prescaler of input clock
		ctrl |= ( clkparm.clksel << 8 ) & GPT_CTRL_CLKSEL;	
		ctrl |= GPT_CTRL_FRS;				//  frs = 1, free-run mode
		t->mode = GPT_M_PROFILE;			// or GPT_M_CAPTURE?
	}
	t->callback = *callback;
	io_wr32( reg->ier, GPT_INT_CAP );
	
	ctrl = ( ctrl & ~GPT_CTRL_EDGEDET ) | ( edgedet << 5 );
	ctrl |= GPT_CTRL_CAPEN;
	// in capture isr, should disable intr_cap, capen and t->usage--
 	io_wr32( reg->ctrl, ctrl );
	
	t->usage++;
	*tick = io_rd32( reg->cnt );
//dbg("gpt: profile_capture info\n");
	return GPT_ERR_NONE;
}


uint32_t gpt_compare(gpt_p t)
/*
@param[in] t	context
@return		tick compare value for timer
*/
{
	return io_rd32( ( ( gpt_reg_p )t->reg )->comp );
}


uint32_t gpt_capture( gpt_p t )
/*
@param[in] t	context
@return 	catpured tick value (capture register)
*/
{
	return io_rd32( ( ( gpt_reg_p )t->reg )->capt );
}


gpt_err_e gpt_exit( gpt_p t )
/*
reset and disable GPT module
@param[in] t	context
@return 	GPT_ERR_XXX
*/
{
	gpt_err_e	err;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;
	
//	io_wr32( reg->ctrl, io_rd32( reg->ctrl ) & ~( GPT_CTRL_CAPEN | GPT_CTRL_COMPEN ) );
	err = gpt_reset( t );
	if ( err )
	{
		return GPT_ERR_HW;
	}
	
	/* disable module */
	io_wr32( reg->ctrl, GPT_CTRL_DIS );
//dbg("gpt: exit info\n");
	return GPT_ERR_NONE;
}



int gpt_sys_ofs(gpt_p t)
{
	gpt_reg_p r = (gpt_reg_p)t->reg;

	if (io_rd32(r->isr) & GPT_INT_COMP)
	{
		io_wr32(r->isr, GPT_INT_COMP);
		return 1;
	}
	return 0;
}



int gpt_isr( gpt_p t )
/*
GPT interrupt service routine
@param[in] t	context
@return		0 = not processed, else processed
*/
{
	uint32_t	stat;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;

	stat = io_rd32( reg->ier ) & io_rd32( reg->isr );
	if ( !stat )
	{
		dbg("gpt: isr err - none pending\n");
		return 0;
	}
	io_wr32( reg->isr, stat );

	if ( stat & GPT_INT_COMP )
	{
		if (t->callback.evt)
		{
			t->callback.evt( t->callback.ctx, GPT_EV_COMP );
		}
		if (t->mode == GPT_M_PROFILE)
		{
			t->usage--;
			if (!t->usage)
			{
				io_wr32( reg->ctrl, GPT_CTRL_DIS );
			}
		}
//dbg("comp isr\n");
	}

	if( stat & GPT_INT_CAP )
	{
		uint32_t ctrl = io_rd32( reg->ctrl ) & 
							~( GPT_CTRL_CAPEN | GPT_CTRL_EDGEDET );
		io_wr32( reg->ctrl, ctrl );
		io_wr32( reg->ier, io_rd32( reg->ier ) & ~GPT_INT_CAP );
		
		if (t->callback.evt)
		{
			t->callback.evt( t->callback.ctx, GPT_EV_CAP );
		}
		t->usage--;
		if (!t->usage)
		{
			io_wr32( reg->ctrl, GPT_CTRL_DIS );
		}
//dbg(" gpt: isr info - capt\n");
	}
	return 1;
}


static gpt_err_e gpt_reset( gpt_p t )
/*
reset GPT module
@param[in]	t	context
@return		GPT_ERR_XXX
*/
{
	int	i;
	gpt_reg_p	reg = (gpt_reg_p)t->reg;

	io_wr32( reg->ctrl, GPT_CTRL_EN );
	io_wr32( reg->ctrl, GPT_CTRL_RST | GPT_CTRL_EN );

	/* reset timeout */
	i = _TIMEOUT;
	while ( io_rd32( reg->ctrl ) & GPT_CTRL_RST )
	{
		if (!i--)
		{
			dbg("gpt: reset err - timeout\n");
			return GPT_ERR_HW;
		}
	}	
	return GPT_ERR_NONE;
}

#if 0
static gpt_err_e gpt_setclk( gpt_p t, uint8_t mode, gpt_clk_p clkparm )
/*
@param[in]  t		context
@param[in]  mode	timer working mode: GPT_M_XXX, (SYSTEM, PROFILE, CAPTURE)
@param[out] clkparm	struct gpt_clk_t to store the clock setting based on 
					user-required tick frequency
@return		GPT_ERR_XXX
@remark		GPT can not generate time resolution higher than 
			(a) perclk for MEASURE, or 
			(b) MIN(perclk, apbclk)/16 for TIMER, or 
			(c) MIN(perclk)/4 for PERIODIC (interrupt disabled), or 
			(d) MIN(perclk, apbclk)/32 for PERIODIC (interrupt enabled), or 
			(e) Can not generate time resolution lower than CLK_32K>>11 for 
				MEASURE mode. (added on 18 Aug 06)
*/
{
	uint32_t	N, lim1, lim2;

	lim1 = MIN( t->perclk, t->apbclk );
	lim2 = MIN( CLK_32K, t->apbclk );

	if ( mode == GPT_M_SYSTEM )
	{
		lim1 >>= 5;
		lim2 >>= 5;
	}	
	else if ( mode == GPT_M_CAPTURE )
	{
		lim1 >>= 4;
		lim2 >>= 4;
	}

	if ( t->freq > lim1 )
	{
		dbg("gpt: setclk err - frequency limited to %u\n", lim1);
		return GPT_ERR_PARM;
	}
	else if ( t->freq > lim2 )
	{
		clkparm->clksel = GPT_CLK_PER;
		N = t->perclk / t->freq;
		if ( mode == GPT_M_PROFILE )
		{
			dbg("gpt: setclk warn - MAX_COUNTER_FREQ = perclk, "
				"but MAX_ACCESS_REGITER_FREQ = MIN( perclk, apbclk )/16\n");
		}
	}
	else
	{
		clkparm->clksel = GPT_CLK_32K;
		N = CLK_32K / t->freq;
	}

 	if ( mode == GPT_M_SYSTEM )
	{
		clkparm->pslr = ( ( N >> 12 ) ? 1 << 11 : N >> 1 ) - 1;
		/* least value for comp_val is 1, not 0, so the least steps 
			counter can go is 2, not 1. */
		clkparm->comp_val = ( N >> 12 ) ? ( N >> 11 ) - 1 : 1;
	}
	else
	{
		clkparm->pslr = ( ( N >> 11 ) ? 1 << 11 : N ) - 1;
	}

	return GPT_ERR_NONE;
}
#endif

static gpt_err_e gpt_setclk(gpt_p t, uint8_t mode, gpt_clk_p clkparm)
/*
@param[in]	t		context
@param[in]	mode	timer working mode: GPT_M_XXX, (SYSTEM, PROFILE, CAPTURE)
@param[out]	clkparm	struct gpt_clk_t to store the clock setting based on 
					user-required tick frequency
@return		GPT_ERR_XXX
@remark		GPT can not generate time resolution higher than 
			(a) perclk for MEASURE, or 
			(b) MIN(perclk, apbclk)/16 for TIMER, or 
			(c) MIN(perclk)/4 for PERIODIC (interrupt disabled), or 
			(d) MIN(perclk, apbclk)/32 for PERIODIC (interrupt enabled), or 
			(e) Can not generate time resolution lower than CLK_32K>>11 for 
				MEASURE mode. (added on 18 Aug 06)
*/
{
	uint32_t	N, f;
	uint32_t	lim;
	uint32_t	lim2;

	f = t->apbclk;
	N = t->perclk;
	lim = MIN(N, f);
	lim2 = MIN(t->clk32k, f);
	f = t->freq;

	if (mode == GPT_M_SYSTEM)
	{
		lim >>= 5;
		lim2 >>= 5;
	}
	else if (mode == GPT_M_CAPTURE)
	{
		lim >>= 4;
		lim2 >>= 4;
	}

	if (f > lim)
	{
		dbg("gpt: setclk err - frequency limited to %u\n", lim);
		return GPT_ERR_PARM;
	}

#if 0
	if (f > lim2)
	{
		clkparm->clksel = GPT_CLK_PER;
	}
	else
	{
		clkparm->clksel = GPT_CLK_32K;
		N = t->clk32k;
	}
	N /= f;
#else
	clkparm->clksel = GPT_CLK_PER;
	N /= f;
#endif

 	if (mode == GPT_M_SYSTEM)
	{
		clkparm->pslr = 0;
		clkparm->comp_val = N - 1;
	}
	else
	{
		clkparm->pslr = ((N >> 11) ? 1 << 11 : N) - 1;
	}

	return GPT_ERR_NONE;
}


