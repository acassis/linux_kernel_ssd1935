/**
@file		clock.h
@author		rahult@solomon-systech.com
Clock API's for Magus.
*/

#ifndef _CLOCK_H_
#define _CLOCK_H_


#define CLKSRC_USBOTG_XTAL_IN	12000000

#ifndef MHZ
#define MHZ (1000*1000)
#endif


#define CLK32 32000

#define print_mhz(m) ((m) / MHZ), ((m / 1000) % 1000)

#include <linux/types.h>


#define INVALID ~0
#define TABLE_END ~1
struct cpufreq_pll_table {
	/* in KHz's */
	unsigned int 	index;
	unsigned int	pll_out;
	unsigned int	valid_freqs[3];
};


struct cpufreq_pll_table * magus_get_freq_table(void);

int  magus_clock_init(void);
/**<
init SCRM/Clock module
@param 		none
@return		0 for success, -ve value for error
*/


uint32_t magus_get_system_clk(void);
/**<
get the system clock to ARM.
@param r	none
@return		system clock in Hertz.
*/


uint32_t magus_get_hclk(void);
/**<
get the AHB clock.
@param r	none
@return		AHB clock in Hertz.
*/

uint32_t magus_get_pclk(void);
/**<
get the APB clock.
@param r	none
@return		APB clock in Hertz.
*/

uint32_t magus_get_perclk1(void);
/**<
get the peripheral clock 1 (UART, SPI, GPT)
@param r	none
@return		peripheral clock 1 in Hertz.
*/


uint32_t magus_get_perclk2(void);
/**<
get the peripheral clock 2 (TVOUT......not sure about others)
@param r	none
@return		peripheral clock 2 in Hertz.
*/


void magus_set_new_freq(uint32_t pllr, uint32_t freq);
/**<
set the new pll configuration/dividers.
@param pllr 	new value for pll control register
@param freq 	new value for system freq
@return		none
*/

uint32_t magus_get_pllr(void);

uint32_t magus_set_hclk(uint32_t hclk);
uint32_t magus_set_pclk(uint32_t pclk);
uint32_t magus_set_perclk1(uint32_t perclk1);
uint32_t magus_set_perclk2(uint32_t perclk2);
void magus_print_clocks(void);
uint32_t magus_get_pll_out(void);
uint32_t magus_set_system_clk(uint32_t armclk);
#endif

