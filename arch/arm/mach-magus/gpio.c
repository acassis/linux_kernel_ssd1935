/**
@file		gpio.c - control code for General Purpose Input Output (GPIO) module
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 18JUL06, last modified 1Aug2006
@date		modified: 24MAY07	sasin@solomon-systech.com
@todo		
**/

#include "os.h"
#include "gpior.h"
#define	gpio_p	gpior_p
#include "gpio.h"


#define _TIMEOUT	300


gpio_err_e gpio_init(gpior_p r)
/*
initialize GPIO module
@param[in] t	context
@return 	GPIO_ERR_XXX
*/
{
	uint32_t	id;
	gpio_err_e	err;

//dbg("gpio: init info - b4 icr2=%X\n", io_rd32(r->icr2));
	/* check ID register and capacities */
	id = io_rd32(r->idr);

	if (GPIO_ID_CLID(id) != _GPIO_CLID)
	// (GPIO_ID_DSGNR(id) != _GPIO_DSGNR))
	{
		dbg("gpio: init err - class/dev id mismatch exp=%08X got=%08X\n", 
			_GPIO_CLID, GPIO_ID_CLID(id));
		return GPIO_ERR_HW;
	}
	dbg("gpio: init info - ver=%d,%d \n", GPIO_ID_MAJ(id), GPIO_ID_MIN(id));

	/* reset the module */
	err = gpio_reset(r);
	if (err)
	{		
		return err;
	}

//dbg("gpio: init info - after icr2=%X\n", io_rd32(r->icr2));
	return GPIO_ERR_NONE;
}


void gpio_ack_port(gpior_p r, uint32_t pinmask)
/*
ack interrupt pin of port
@param[in] r		register base address
@param[in] pinmask	(1 << [0-based pin#])
*/
{
	io_wr32(r->isr, pinmask);
}


void gpio_mask_port(gpior_p r, uint32_t pinmask, int mask)
/*
mask interrupt pin of port
@param[in] r		register base address
@param[in] pinmask	(1 << [0-based pin#])
*/
{
	uint32_t	v;

	v = io_rd32(r->ier);
	if (!mask == !(v & pinmask))
	{
		io_wr32(r->ier, v ^ pinmask);
	}
}


uint32_t gpio_pend(gpior_p r)
{
	return io_rd32(r->isr);
}


gpio_err_e gpio_cfg_pins(gpior_p r, uint32_t mask, gpio_cfg_p cfg)
/* 
configure a port (32-pins) with mask
@param[in] t		context
@param[in] cfg	configuration for a port
@param[in] mask		mask of pins for the port, 32-bit for 32 pins
@return 		GPIO_ERR_XXX
*/
{
	uint8_t		i;
	uint32_t	opmask;

	if (cfg->mode > GPIO_M_INTR)
	{
		dbg("gpio: cfg err - unknown mode\n");
		return GPIO_ERR_PARM;
	}

	if (!mask)
	{
		dbg("gpio: cfg err - no pin selected for config\n");
		return GPIO_ERR_NONE;
	}

	opmask = ~mask;
	io_wr32(r->puen, (io_rd32(r->puen) & opmask) | 
							(cfg->pull_en ? mask : 0));

	/* set port mode */
	if (cfg->mode == GPIO_M_PRI)
	{
		io_wr32(r->func, io_rd32(r->func) & opmask);
		io_wr32(r->mode, io_rd32(r->mode) & opmask);
	}
	else if (cfg->mode == GPIO_M_ALT)
	{
		io_wr32(r->func, io_rd32(r->func) | mask);
		io_wr32(r->mode, io_rd32(r->mode) & opmask);
	}	
	else
	{
		uint32_t	mask1, mask2;

		mask1 = mask2 = 0;
		for (i = 0; i < 16; i++)
		{
			mask1 |= (mask & (1 << i)) ? (3 << (2 * i)) : 0;
			mask2 |= (mask & (1 << (i + 16))) ? (3 << (2 * i)) : 0;
		}

		io_wr32(r->mode, io_rd32(r->mode) | mask);
		if (cfg->mode == GPIO_M_OUT)
		{
			uint32_t	oss1, oss2;	
			uint32_t	t_oss = 0;				/* template of oss mode */

			oss1 = io_rd32(r->oss1) & ~mask1;
			oss2 = io_rd32(r->oss2) & ~mask2;

			for (i=0; i<16; i++)
			{
				t_oss |= (cfg->oss & 3) << (2*i);
			}
			oss1 |= t_oss & mask1;
			oss2 |= t_oss & mask2;

			io_wr32(r->oss1, oss1);
			io_wr32(r->oss2, oss2);
			io_wr32(r->ddir, io_rd32(r->ddir) | mask);
		}
		else
		{
			uint32_t	issa1, issa2, issb1, issb2;
			uint32_t	t_issa, t_issb;	/* template of issa, issb modes */

			t_issa = t_issb = 0;
			issa1 = io_rd32(r->issa1) & ~mask1;
			issa2 = io_rd32(r->issa2) & ~mask2;
			issb1 = io_rd32(r->issb1) & ~mask1;
			issb2 = io_rd32(r->issb2) & ~mask2;

			if (cfg->mode == GPIO_M_INTR)		/* ISR */
			{
				uint32_t	icr1, icr2;
				uint32_t	t_icr = 0;		/* template of icr mode */

				icr1 = icr2 = 0;
				cfg->issa = GPIO_ISS_ISR;
				cfg->issb = GPIO_ISS_ISR;

				icr1 = io_rd32(r->icr1) & ~mask1;
				icr2 = io_rd32(r->icr2) & ~mask2;

				for (i = 0; i < 16; i++)
				{
					t_icr |= (cfg->irq & 3) << (2 * i);
				}
				icr1 |= t_icr & mask1;
				icr2 |= t_icr & mask2;

				io_wr32(r->icr1, icr1);
				io_wr32(r->icr2, icr2);
//				io_wr32(r->ier, io_rd32(r->ier) | mask);
			}
			for (i = 0; i < 16; i++)
			{
				t_issa |= (cfg->issa & 3) << (2 * i);
				t_issb |= (cfg->issb & 3) << (2 * i);
			}

			issa1 |= t_issa & mask1;
			issa2 |= t_issa & mask2;
			issb1 |= t_issb & mask1;
			issb2 |= t_issb & mask2;

			io_wr32(r->issa1, issa1);
			io_wr32(r->issa2, issa2);
			io_wr32(r->issb1, issb1);
			io_wr32(r->issb2, issb2);

			io_wr32(r->ddir, io_rd32(r->ddir) & opmask);
		}
	}
#if 0
{
dbg("gpio: cfg info - addr=%08X mask=%08X mode=%d\n"
	"func=%08X, mode=%08X dir=%08X pull=%08X\n"
	"issa1=%08X, issa2=%08X\nissb1=%08X, issb2=%08X\n"
	"icr1=%08X, icr2=%08X\n", 
r, mask, cfg->mode, 
io_rd32(r->func), io_rd32(r->mode), io_rd32(r->ddir), io_rd32(r->puen),
io_rd32(r->issa1), io_rd32(r->issa2), io_rd32(r->issb1), io_rd32(r->issb2), 
io_rd32(r->icr1), io_rd32(r->icr2));
}
#endif
	return GPIO_ERR_NONE;
}


void gpio_set_pins(gpior_p r, uint32_t pinmask, uint32_t data)
/*
set the data output to the port
@param[in]	r		register base address
@param[in]	pinmask	32-bit pinmask for port
@param[in]	data		data output from port
*/
{
	io_wr32(r->dreg, (io_rd32(r->dreg) & ~pinmask) | (data & pinmask));
}


uint32_t gpio_get_pins(gpior_p r, uint32_t pinmask)
/*
read the input of the port
@param[in]	r	register base address
@param[in]	pinmask	32-bit pinmask for port
@return 	data input from GPIO pins
*/
{
	return io_rd32(r->dreg) & pinmask;
}


gpio_err_e gpio_reset(gpior_p r)
/*
reset a GPIO port
@param[in] r	register base address
@return 	GPIO_ERR_XXX
*/
{
	int	i;

	io_wr32(r->ctrl, GPIO_CTRL_RST);

	/* reset timeout */
	i = _TIMEOUT;	
	while (io_rd32(r->ctrl) & GPIO_CTRL_RST) 
	{
		if (!i--)
		{
			dbg("gpio: reset err - timeout\n");
			return GPIO_ERR_HW;
		}
	}
	return GPIO_ERR_NONE;
}

