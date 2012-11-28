/**
@file		clock.c - Clock & PLL API's for magus & control code for SCRM module
@author 	rahult@solomon-systech.com
@version 	1.0
@date		created: 12thMay, 2008
*/

#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/clock.h>
#include "os.h"
#include "scrmr.h"


/********************************************************************************
*  Constraints:									*
*	2 MHz	<=	Fref	<=	8 MHz					*
*	200 MHz <=	Fvco	<=	400 MHz					*
*	50 MHz  <=	Fout	<=	400 MHz					*
*										*
*  Dividers:									*
*	FBDIV for feedback divider, FBDIV_VALUE = 2*FBDIV			*
*	OPDIV for output divider, OPDIV_VALUE = OPDIV				*
*	REFDIV for input divider, REFDIV_VALUE = REFDIV				*
*										*
*  Formulas:									*
*	Fref = Fin/REFDIV_VALUE							*
*	Fvco = Fout * OPDIV_VALUE						*
*	Fout = Fref * FBDIV_VALUE/ OPDIV_VALUE)					*
*										*
********************************************************************************/


int  scrm_init(scrmr_p r)
/**<
init SCRM/Clock module
@param 		register context
@return		0 for success, -ve value for error
*/
{
	uint32_t	id;

	/* check ID register and capacities */
	id = io_rd32(r->idr);

	if (SCRM_ID_CLID(id) != _SCRM_CLID)
	{
		dbg("clock: init err - class/dev id mismatch exp=%08X got=%08X\n", _SCRM_CLID, SCRM_ID_CLID(id));
		return -1;
	}
	dbg("clock: init info - ver=%d,%d \n", SCRM_ID_MAJ(id), SCRM_ID_MIN(id));

	dbg("MAGUS Clocks : ARM-%d.%03d MHz, HCLK-%d.%03d MHz, PCLK-%d.%03d MHz, PERCLK1-%d.%03d MHz, PERCLK2-%d.%03d MHz\n", 
				print_mhz(magus_get_system_clk()), 
				print_mhz(magus_get_hclk()),
				print_mhz(magus_get_pclk()), 
				print_mhz(magus_get_perclk1()), 
				print_mhz(magus_get_perclk2()));


	return 0;
}


void scrm_set_new_freq(scrmr_p r, uint32_t pllr, uint32_t freq)
/**<
set the new pll configuration/dividers.
@param r	module register context.
@param pllr 	new value for pll control register (if 0 then no need to change pll_out )
@param freq 	new value for system freq.
@return		none
*/
{
	unsigned long flags;
	uint32_t perclk1;	
	uint32_t uart_div_latch, hclk, pclk;

	local_irq_save(flags);

	if( pllr != 0 ) {

		/* Write new values to PLLR */
		/* Assumptions 
	 	* 1. error checking for all zero dividers is done in compute PLL params routine *
	 	* 2. compute PLL params routine reads PLLR, updates new dividers, other bits unchanged */
		io_wr32(r->pllr, pllr);

		/* update the peripheral clocks */
		perclk1 = magus_get_perclk1();

		/* 1. UART */
		uart_div_latch = perclk1 / (16 * 115200);
		if( uart_div_latch > 0xff ) {

			/* lower 8 bits of divisor latch in LSB register */	
			*(volatile uint32_t *)(MAGUS_VIO_UART | 0x30) = (uart_div_latch & 0xff);

			/* higher 8 bits of divisor latch in MSB register */	
			*(volatile uint32_t *)(MAGUS_VIO_UART | 0x34) = ((uart_div_latch>>8) & 0xff);
		}
		else {	
			*(volatile uint32_t *)(MAGUS_VIO_UART | 0x30) = uart_div_latch;
		}

		/* Write the restart bit to restart the PLL */
		io_wr32(r->pllr, io_rd32(r->pllr) | SCRM_PLLR_RESTRT);

		while( io_rd32(r->pllr) & SCRM_PLLR_RESTRT);
	}

	magus_set_system_clk(freq);

	local_irq_restore(flags);
}

uint32_t scrm_get_pllr(scrmr_p r)
/**<
get the pll control register value
@param r	module register context.
@return		pll control register value.
*/
{
	return (io_rd32(r->pllr));
}

uint32_t scrm_decode_pll(uint32_t pll, uint32_t f_in)
/**<
decode the pll control register value & return the PLL output frequency.
@param pll	pll control register value
@param f_in	source input frequency to PLL
@return		PLL output frequency i.e. Fout
*/
{
	uint32_t f_out;
	uint32_t refdiv = SCRM_PLLR_REFDIV(pll);
	uint32_t opdiv = SCRM_PLLR_OPDIV(pll);
	uint32_t fbdiv = 2 * SCRM_PLLR_FBDIV(pll);
	uint32_t f_ref = f_in/refdiv;

	f_out = f_ref * fbdiv/opdiv;

	return f_out;
}

uint32_t scrm_get_pll_out(scrmr_p r)
/**<
get the pll out i.e. Fout
@param r	module register context.
@return		PLL out in Hertz.
*/
{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t pll_out;

	pll_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);

	return (pll_out);
}

uint32_t scrm_get_system_clk(scrmr_p r)
/**<
get the system clock to ARM.
@param r	module register context.
@return		system clock in Hertz.
*/
{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t csdr = io_rd32(r->csdr);
	uint32_t f_out, psclr;

	f_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);
	psclr = SCRM_CSDR_PSCLR(csdr);

	return (f_out/(psclr+1));
}

uint32_t scrm_set_system_clk(scrmr_p r, uint32_t freq)
/**<
set the ARM clock.
@param r	module register context.
@param hclk	new armclk to set
@return		new ARM clock in Hertz.
*/
{
#if defined CONFIG_ACCIO_CM5208 || defined CONFIG_ACCIO_A2818T || defined CONFIG_ARCH_MAGUS_ADS \
	|| defined CONFIG_ACCIO_CM5210 || defined CONFIG_LUMOS_WE8623_P0 || defined CONFIG_ACCIO_P1_SK01 \
	|| defined CONFIG_ACCIO_LITE
	uint32_t psclr, csdr;
	unsigned long flags;
	uint32_t pll_out = scrm_get_pll_out(r);
	uint32_t hclk, armclk = freq, hclkdiv = SCRM_CSDR_HCLKDIV(io_rd32(r->csdr));
	
	uint32_t sdr_cs0_cfgr, sdr_ECAS, cur_sys_clk = magus_get_system_clk();
    int except=0;  

	if( armclk > pll_out ) armclk = pll_out;

	psclr = (pll_out/armclk -1);

	if( psclr < 0) psclr =0;
	if( psclr > 7) psclr =7;

	dbg("\nNew PSCLR : %d\n", psclr);

	if( (psclr == 0) && (hclkdiv == 0) ) {
		/* Pll out is equal to ARM */
		/* before changing, make sure AHB is maintained at 50 % duty cycle */
		hclkdiv += 1;
		dbg("cpufreq: Updating AHB to maintain 50% duty cycle\n");
		dbg("New HCLK DIV : %d\n", hclkdiv);

		csdr = io_rd32(r->csdr);
		csdr = csdr & ~(0xf<<8);
		csdr = (csdr | ((hclkdiv & 0xf)<<8));
		io_wr32(r->csdr, csdr);
	}
#ifdef MAGUS_DDR
   	sdr_cs0_cfgr = *(volatile uint32_t *)(MAGUS_VIO_SDRAMC | 0x0C);
   	sdr_ECAS = ((sdr_cs0_cfgr >> 12) & 0x1);
   	if( cur_sys_clk > armclk ) {
    	/* switching from high to low freq */
       	if( armclk/1000 <= 150000) {
        	if( sdr_ECAS == 1 ) {
            	sdr_ECAS =0;
            	sdr_cs0_cfgr &= ~(1 << 12);
                *(volatile uint32_t *)(MAGUS_VIO_SDRAMC | 0x0C) = sdr_cs0_cfgr; 
           	}
       	}
       	else if( armclk/1000 >= 180000) {
          	if( sdr_ECAS == 0 ) { /* ideally this shud never happen */
               	sdr_ECAS =1;
               	sdr_cs0_cfgr |= (sdr_ECAS << 12);
           	    *(volatile uint32_t *)(MAGUS_VIO_SDRAMC | 0x0C) = sdr_cs0_cfgr; 
           	}
       	}
   	}  
   	else {
       /* switching from low to high freq */
     	if( armclk/1000 <= 150000) {
        	if( sdr_ECAS == 1 ) { /* ideally this shud never happen */
            	sdr_ECAS =0;
               	sdr_cs0_cfgr &= ~(1 << 12);
               	*(volatile uint32_t *)(MAGUS_VIO_SDRAMC | 0x0C) = sdr_cs0_cfgr; 
         	}
      	}
      	else if( armclk/1000 >= 180000) {
        	except =1;
           	csdr = io_rd32(r->csdr);
           	csdr = csdr & ~(0x7<<12);
           	csdr = (csdr | ((psclr & 0x7)<<12));
           	io_wr32(r->csdr, csdr);

           	if( sdr_ECAS == 0 ) { 
               	sdr_ECAS =1;
               	sdr_cs0_cfgr |= (sdr_ECAS << 12);
               	*(volatile uint32_t *)(MAGUS_VIO_SDRAMC | 0x0C) = sdr_cs0_cfgr; 
        	}
     	}
   	}
#endif
	if( except ==0 ) {
    	csdr = io_rd32(r->csdr);
        csdr = csdr & ~(0x7<<12);
        csdr = (csdr | ((psclr & 0x7)<<12));
    	io_wr32(r->csdr, csdr);
   	}
  	return magus_get_system_clk();
#else
// The AHB_CLK must be 120MHz on Accio Platform, or the LCD display will flitter.
// However, when system clk is 60MHz, it is impossible to set AHB_CLK 120MHz in any way.
// So we set AHB_CLK 60MHz. But when we set system clock 120MHz 240MHz, confirm AHB_CLK 120MHz.

	/* Changed by Spirit. 08.09.02*/
#define DEFAULT_FIX_AHB_CLK 120000000   // 120MHz
	uint32_t psclr, csdr;
	unsigned long flags;
	uint32_t pll_out = scrm_get_pll_out(r);
	uint32_t hclk, armclk = freq, hclkdiv = SCRM_CSDR_HCLKDIV(io_rd32(r->csdr));

	uint32_t current_armclk = scrm_get_system_clk(r);
	
	if (current_armclk == armclk) 
        return current_armclk;

   	hclk = DEFAULT_FIX_AHB_CLK;
	csdr = io_rd32(r->csdr);
	if( armclk > pll_out ) armclk = pll_out;

	psclr = (pll_out/armclk -1);

	if( psclr < 0) psclr =0;
	if( psclr > 7) psclr =7;

	dbg("\nNew PSCLR : %d\n", psclr);

	if( (psclr == 0) && (hclkdiv == 0) ) {
		/* Pll out is equal to ARM */
		/* before changing, make sure AHB is maintained at 50 % duty cycle */
		hclkdiv += 1;
		//dbg("cpufreq: Updating AHB to maintain 50% duty cycle\n");
		/*
		csdr = io_rd32(r->csdr);
		csdr = csdr & ~(0xf<<8);
		csdr = (csdr | ((hclkdiv & 0xf)<<8));
		io_wr32(r->csdr, csdr);
		*/
	}
	else{
		hclkdiv = ((pll_out / (psclr + 1)) / hclk - 1); 
        if (hclkdiv == -1) {
           hclkdiv = 1;
        }	
	}

	dbg("New HCLK DIV : %d\n", hclkdiv);
	local_irq_save(flags);
	csdr = io_rd32(r->csdr);
    
	// update HCLKDIV   
    csdr = csdr & ~(0xf << 8); 
    csdr = (csdr | ((hclkdiv & 0xf) << 8));
    // update PSCLR 
 	csdr = csdr & ~(0x7 << 12);
    csdr = (csdr | ((psclr & 0x7) << 12));
    
	io_wr32(r->csdr, csdr);
	local_irq_restore(flags);
    
	return magus_get_system_clk();	   

#endif
}



uint32_t scrm_get_hclk(scrmr_p r)
/**<
get the AHB clock.
@param r	module register context.
@return		AHB clock in Hertz.
*/
{
	uint32_t hclkdiv = SCRM_CSDR_HCLKDIV(io_rd32(r->csdr));
	uint32_t sysclk = scrm_get_system_clk(r);

	return (sysclk/(hclkdiv+1));

}

uint32_t scrm_set_hclk(scrmr_p r, uint32_t hclk_freq)
/**<
set the AHB clock.
@param r	module register context.
@param hclk	new hclk to set
@return		new AHB clock in Hertz.
*/
{
	uint32_t hclkdiv, csdr, hclk;
	uint32_t sysclk = scrm_get_system_clk(r);
	uint32_t pll_out = scrm_get_pll_out(r);

	hclk = hclk_freq;
	if( hclk > sysclk) {
		dbg("\nAHB clock can not be higher than ARM clock.\n");
		hclk = sysclk;
	}

	if( (sysclk != hclk) && (((sysclk/hclk) % 2) != 0)) {
		dbg("\nError: AHB clock has to be equal or even divisible of ARM clock.\n");
		return magus_get_hclk();
	}
	if( (sysclk == hclk) && (sysclk == pll_out) ) {
		/* AHB clock has to be even divider of PLL out */
		dbg("\nError: AHB clock has to be even divisible of F_out (PLL Out)\n");
		return magus_get_hclk();
		
	}

	hclkdiv = (sysclk/hclk -1);

	if( hclkdiv < 0) hclkdiv =0;
	if( hclkdiv > 15) hclkdiv =15;

	dbg("\nNew HCLK DIV : %d\n", hclkdiv);

	csdr = io_rd32(r->csdr);
	csdr = csdr & ~(0xf<<8);
	csdr = (csdr | ((hclkdiv & 0xf)<<8));
	io_wr32(r->csdr, csdr);


	return magus_get_hclk();

}

uint32_t scrm_get_pclk(scrmr_p r)
/**<
get the APB clock.
@param r	module register context.
@return		APB clock in Hertz.
*/
{
	uint32_t apbdiv = SCRM_CSDR_APBDIV(io_rd32(r->csdr));
	uint32_t hclk = scrm_get_hclk(r);

	return (hclk/(apbdiv+1));

}

uint32_t scrm_set_pclk(scrmr_p r, uint32_t pclk)
/**<
set the APB clock.
@param r	module register context.
@param pclk	new pclk to set
@return		new APB clock in Hertz.
*/
{
	uint32_t apbdiv, csdr;
	uint32_t hclk = scrm_get_hclk(r);

	apbdiv = (hclk/pclk -1);

	if( apbdiv < 0) apbdiv =0;
	if( apbdiv > 3) apbdiv =3;

	dbg("New APB DIV : %d\n", apbdiv);

	csdr = io_rd32(r->csdr);
	csdr = csdr & ~(0x3<<4);
	csdr = (csdr | ((apbdiv & 0x3)<<4));
	io_wr32(r->csdr, csdr);

	return magus_get_pclk();

}

uint32_t scrm_get_perclk1(scrmr_p r)
/**<
get the peripheral clock 1 (UART, SPI, GPT)
@param r	module register context.
@return		peripheral clock 1 in Hertz.
*/
{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t psdr = io_rd32(r->psdr);
	uint32_t f_out, pdiv1;

	f_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);
	pdiv1 = SCRM_PSDR_PDIV1(psdr);

	return (f_out/(pdiv1+1));

}

uint32_t scrm_set_perclk1(scrmr_p r, uint32_t perclk1)
/**<
set the perclk1 clock.
@param r	module register context.
@param perclk1	new perclk1 to set
@return		new perclk1 clock in Hertz.
*/

{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t psdr = io_rd32(r->psdr);
	uint32_t f_out, pdiv1;

	f_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);
	pdiv1 = f_out/perclk1 -1;

	if(pdiv1 < 0) pdiv1=0;
	if(pdiv1 > 15) pdiv1=15;

	dbg("New PDIV1 : %d\n", pdiv1);

	psdr = psdr & ~(0xf<<12);
	psdr = (psdr | ((pdiv1 & 0xf)<<12));
	io_wr32(r->psdr, psdr);


	return magus_get_perclk1();

}


uint32_t scrm_get_perclk2(scrmr_p r)
/**<
get the peripheral clock 2 (TVOUT......not sure about others)
@param r	module register context.
@return		peripheral clock 2 in Hertz.
*/
{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t psdr = io_rd32(r->psdr);
	uint32_t f_out, pdiv2;

	f_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);
	pdiv2 = SCRM_PSDR_PDIV2(psdr);

	return (f_out/(pdiv2+1));

}

uint32_t scrm_set_perclk2(scrmr_p r, uint32_t perclk2)
/**<
set the perclk2 clock.
@param r	module register context.
@param perclk2	new perclk2 to set
@return		new perclk2 clock in Hertz.
*/
{
	uint32_t pll = io_rd32(r->pllr);
	uint32_t psdr = io_rd32(r->psdr);
	uint32_t f_out, pdiv2;

	f_out = scrm_decode_pll(pll, CLKSRC_USBOTG_XTAL_IN);
	pdiv2 = f_out/perclk2 -1;

	if(pdiv2 < 0) pdiv2=0;
	if(pdiv2 > 15) pdiv2=15;

	dbg("New PDIV2 : %d\n", pdiv2);

	psdr = psdr & ~(0xf<<8);
	psdr = (psdr | ((pdiv2 & 0xf)<<8));
	io_wr32(r->psdr, psdr);


	return magus_get_perclk2();

}


/* Magus Clock API's wrappers */
void magus_print_clocks(void) 
{
	dbg("New MAGUS Clocks : \n ARM-%d.%03d MHz,\n HCLK-%d.%03d MHz,\n PCLK-%d.%03d MHz,\n PERCLK1-%d.%03d MHz,\n PERCLK2-%d.%03d MHz\n", 
				print_mhz(magus_get_system_clk()), 
				print_mhz(magus_get_hclk()),
				print_mhz(magus_get_pclk()), 
				print_mhz(magus_get_perclk1()), 
				print_mhz(magus_get_perclk2()));
}
EXPORT_SYMBOL(magus_print_clocks);
int  magus_clock_init(void) 
{
	return (scrm_init((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_clock_init);

uint32_t magus_get_system_clk(void)
{
	return (scrm_get_system_clk((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_system_clk);

uint32_t magus_get_hclk(void)
{
	return (scrm_get_hclk((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_hclk);

uint32_t magus_get_pclk(void)
{
	return (scrm_get_pclk((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_pclk);

uint32_t magus_get_perclk1(void)
{
	return (scrm_get_perclk1((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_perclk1);

uint32_t magus_get_perclk2(void)
{
	return (scrm_get_perclk2((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_perclk2);

void magus_set_new_freq(uint32_t pllr, uint32_t freq)
{
	return (scrm_set_new_freq((void *)MAGUS_VIO_SCRM, pllr, freq));
}
EXPORT_SYMBOL(magus_set_new_freq);

uint32_t magus_get_pllr(void)
{
	return (scrm_get_pllr((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_pllr);

uint32_t magus_set_hclk(uint32_t hclk)
{
	return (scrm_set_hclk((void *)MAGUS_VIO_SCRM, hclk));
}
EXPORT_SYMBOL(magus_set_hclk);
uint32_t magus_set_pclk(uint32_t pclk)
{
	return (scrm_set_pclk((void *)MAGUS_VIO_SCRM, pclk));
}
EXPORT_SYMBOL(magus_set_pclk);
uint32_t magus_set_perclk1(uint32_t perclk1)
{
	return (scrm_set_perclk1((void *)MAGUS_VIO_SCRM, perclk1));
}
EXPORT_SYMBOL(magus_set_perclk1);
uint32_t magus_set_perclk2(uint32_t perclk2)
{
	return (scrm_set_perclk2((void *)MAGUS_VIO_SCRM, perclk2));
}
EXPORT_SYMBOL(magus_set_perclk2);
uint32_t magus_get_pll_out(void)
{
	return (scrm_get_pll_out((void *)MAGUS_VIO_SCRM));
}
EXPORT_SYMBOL(magus_get_pll_out);
uint32_t magus_set_system_clk(uint32_t armclk)
{
	return (scrm_set_system_clk((void *)MAGUS_VIO_SCRM, armclk));
}
EXPORT_SYMBOL(magus_set_system_clk);



