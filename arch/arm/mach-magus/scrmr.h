/**
@file		scrmr.h - Hardware dependent definitions for system clock & reset (SCRM)  module
@author		rahult@solomon-systech.com
@version	1.0
@date		created: 12thMay, 2008
@todo  
*/

#ifndef	_SCRMR_H_
#define	_SCRMR_H_


#define _SCRM_CLID	0x080C
#define _SCRM_DSGNR	0x0

#define MAGUS_DDR

#pragma pack(4)

typedef struct
{
	uint32_t	idr;		/**< device identification register */
	uint32_t	capr;		/**< capabilities register */
	uint32_t	pllr;		/**< PLL control register */
	uint32_t	csdr;		/**< clock source divide register */
	uint32_t	psdr;		/**< peripheral source divide register */
	uint32_t	cosr;		/**< clock out select register */
	uint32_t	sstr;		/**< system status register */
	uint32_t	mstr;		/**< module status register */

}
volatile scrmr_t, *scrmr_p;

#pragma pack()

#define SCRM_ID_CLID(d)	( (d) >> 16 )
#define SCRM_ID_DSGNR(d)	( ( (d) >> 10) & 0x3F )
#define SCRM_ID_MAJ(d)	( ( (d) >> 6 ) & 0x0F )
#define SCRM_ID_MIN(d)	( (d) & 0x3F )

/* in normal case, only possible input frequency on Magus */
#define CLKSRC_USBOTG_XTAL_IN	12000000

/* PLL, clock source & peripheral clock dividers */
#define SCRM_PLLR_RESTRT	(1 << 1)
#define SCRM_PLLR_REFDIV(p)	((p>>8) & 0x1f)
#define SCRM_PLLR_OPDIV(p)	((p>>16) & 0x3)
#define SCRM_PLLR_FBDIV(p)	((p>>24) & 0xff)

#define SCRM_CSDR_PSCLR(c)	((c>>12) & 0x7)
#define SCRM_CSDR_HCLKDIV(c)	((c>>8) & 0xf)
#define SCRM_CSDR_APBDIV(c)	((c>>4) & 0x3)

#define SCRM_PSDR_PDIV1(p)	((p>>12) & 0xf)
#define SCRM_PSDR_PDIV2(p)	((p>>8) & 0xf)


/* forward declarations */
int  scrm_init(scrmr_p r);
/**<
init SCRM/Clock module
@param 		register context
@return		0 for success, -ve value for error
*/


uint32_t scrm_get_system_clk(scrmr_p r);
/**<
get the system clock to ARM.
@param r	module register context.
@return		system clock in Hertz.
*/


uint32_t scrm_get_hclk(scrmr_p r);
/**<
get the AHB clock.
@param r	module register context.
@return		AHB clock in Hertz.
*/

uint32_t scrm_get_pclk(scrmr_p r);
/**<
get the APB clock.
@param r	module register context.
@return		APB clock in Hertz.
*/

uint32_t scrm_get_perclk1(scrmr_p r);
/**<
get the peripheral clock 1 (UART, SPI, GPT)
@param r	module register context.
@return		peripheral clock 1 in Hertz.
*/


uint32_t scrm_get_perclk2(scrmr_p r);
/**<
get the peripheral clock 2 (TVOUT......not sure about others)
@param r	module register context.
@return		peripheral clock 2 in Hertz.
*/

void scrm_set_new_freq(scrmr_p r, uint32_t pllr, uint32_t freq);
/**<
set the new pll configuration/dividers.
@param r	module register context.
@param pllr 	new value for pll control register
@param freq 	new value for the system freq
@return		none
*/


uint32_t scrm_decode_pll(uint32_t pll, uint32_t f_in);
/**<
decode the pll control register value & return the PLL output frequency.
@param pll	pll control register value
@param f_in	source input frequency to PLL
@return		PLL output frequency
*/

uint32_t scrm_get_pllr(scrmr_p r);
/**<
get the pll control register value
@param r	module register context.
@return		pll control register value.
*/

uint32_t scrm_set_hclk(scrmr_p r, uint32_t hclk);
/**<
set the AHB clock.
@param r	module register context.
@param hclk	new hclk to set
@return		new AHB clock in Hertz.
*/

uint32_t scrm_set_pclk(scrmr_p r, uint32_t pclk);
/**<
set the APB clock.
@param r	module register context.
@param pclk	new pclk to set
@return		new APB clock in Hertz.
*/

uint32_t scrm_set_perclk1(scrmr_p r, uint32_t perclk1);
/**<
set the perclk1 clock.
@param r	module register context.
@param perclk1	new perclk1 to set
@return		new perclk1 clock in Hertz.
*/

uint32_t scrm_set_perclk2(scrmr_p r, uint32_t perclk2);
/**<
set the perclk2 clock.
@param r	module register context.
@param perclk2	new perclk2 to set
@return		new perclk2 clock in Hertz.
*/

uint32_t scrm_get_pll_out(scrmr_p r);
uint32_t scrm_set_system_clk(scrmr_p r, uint32_t armclk);
#endif
