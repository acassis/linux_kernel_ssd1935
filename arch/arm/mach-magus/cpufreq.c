/*
 * cpufreq.c: clock scaling for the Solomon Systech's Magus platform.
 *
 * Based on SA1100 version written by:
 * - Johan Pouwelse (J.A.Pouwelse@its.tudelft.nl): initial version
 * - Erik Mouw (J.A.K.Mouw@its.tudelft.nl):
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ChangeLog: rahult@solomon-systech.com  28th May, 2008 Ported for Magus.
 */


/*#define DEBUG*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <asm/system.h>

#include <asm/hardware.h>
#include <asm/clock.h>

#include "os.h"
#include "scrmr.h"


/* the PLL is required to be divided by an even number to have 50 % duty cycle for SDRAM */
struct cpufreq_pll_table  magus_freq_table[] = 
{
	{
		.index = 0,
		.pll_out = 240000,
		.valid_freqs[0] = 240000 ,
		.valid_freqs[1] = 120000,
		.valid_freqs[2] = 60000,
	},
	{
		.index = 1,
		.pll_out = 220000,
		.valid_freqs[0] = 220000,
		.valid_freqs[1] = 110000,
		.valid_freqs[2] = 55000,
	},
	{
		.index = 2,
		.pll_out = 210000,
		.valid_freqs[0] = 210000,
		.valid_freqs[1] = 105000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 3,
		.pll_out = 200000,
		.valid_freqs[0] = 200000,
		.valid_freqs[1] = 100000,
		.valid_freqs[2] = 50000,
	},
	{
		.index = 4,
		.pll_out = 190000,
		.valid_freqs[0] = 190000,
		.valid_freqs[1] = 95000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 5,
		.pll_out = 180000,
		.valid_freqs[0] = 180000,
		.valid_freqs[1] = 90000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 6,
		.pll_out = 170000,
		.valid_freqs[0] = 170000,
		.valid_freqs[1] = 85000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 7,
		.pll_out = 160000,
		.valid_freqs[0] = 160000,
		.valid_freqs[1] = 80000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 8,
		.pll_out = 150000,
		.valid_freqs[0] = 150000,
		.valid_freqs[1] = 75000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 9,
		.pll_out = 130000,
		.valid_freqs[0] = 130000,
		.valid_freqs[1] = 65000,
		.valid_freqs[2] = INVALID,
	},
	{
		.index = 10,
		.pll_out = TABLE_END,
		.valid_freqs[0] = TABLE_END,
		.valid_freqs[1] = TABLE_END,
		.valid_freqs[2] = TABLE_END,
	},

};



uint32_t magus_compute_pllr(uint32_t *new_pllr, uint32_t f_out, uint32_t f_in, unsigned long freq, int relation)
{

	/* need to calculate the new FBDIV, OPDIV, REFDIV for the new target frequency */

	int opdiv, refdiv, opdiv_new, refdiv_new;
	uint32_t pllr, new_freq, f_ref, cur_pllr;
	int fbdiv, fbdiv_new;


	cur_pllr = magus_get_pllr();

	/* Constraints */
	/* FBDIV<26-200>, OPDIV<1,2,3>, REFDIV<1-31> */

	fbdiv = 2 * SCRM_PLLR_FBDIV(cur_pllr);
	opdiv = SCRM_PLLR_OPDIV(cur_pllr);
	refdiv = SCRM_PLLR_REFDIV(cur_pllr);

	f_ref = f_in/refdiv;	


	/* Find out the new opdiv which doesnt violate the 200 Mhz <= Fvco <= 400 MHz */
	/* Limitations:							*/
	/*	50 <= Fout < 100 --> opdiv =3 i.e Fvco = Fout * 4 	*/
	/*	100 <= Fout < 200 --> opdiv =2 i.e Fvco = Fout * 2 	*/
	/*	200 <= Fout <= 400 --> opdiv =1 i.e Fvco = Fout * 1 	*/
	/*								*/
	if( (f_out >= 50000000) && (f_out < 100000000)) {
		opdiv_new = 3;
	}
	else if((f_out >= 100000000) && (f_out < 200000000)) {
		opdiv_new = 2;
	}
	else if((f_out >= 200000000) && (f_out <= 400000000)) {
		opdiv_new = 1;
	}

	/* Make sure the new fbdiv/2 is within <26-200> i.e. fbdiv <52-400>*/
	refdiv =2;
	refdiv_new = refdiv;

	fbdiv_new = (f_out * refdiv * opdiv_new)/f_in;
	if( (fbdiv_new < 52) ) {
		/* need to change the refdiv - possible values for refdiv with 12 MHz Fin is <2-6> */
		for(refdiv_new=2; refdiv_new <=6; refdiv_new++) {

			fbdiv_new = (f_out * refdiv_new * opdiv_new)/f_in;

			if(fbdiv_new >= 52)  {
				
				if( (((f_out*refdiv_new*opdiv_new) % f_in) != 0 ) || ((fbdiv_new%2)!=0) ) continue;
				else break;
			}
		}
	}
	else if( fbdiv_new > 400 ) {
		for(refdiv_new=6; refdiv_new <=2; refdiv_new--) {

			fbdiv_new = (f_out * refdiv_new * opdiv_new)/f_in;

			if(fbdiv_new <= 400) {

				if( (((f_out*refdiv_new*opdiv_new) % f_in) != 0 ) || ((fbdiv_new%2)!=0) ) continue;
				else break;
			} 
		}
	}
	else {
		if( (f_out*refdiv*opdiv_new) % f_in != 0 ) {

			for(refdiv_new=2; refdiv_new <=6; refdiv_new++) {
				fbdiv_new = (f_out * refdiv_new * opdiv_new)/f_in;
				if((fbdiv_new >= 52) && (fbdiv_new <= 400 ))  {
					if( (((f_out*refdiv_new*opdiv_new) % f_in) != 0 ) || ((fbdiv_new%2)!=0) ) continue;
					else break;
				}
			}
		}
	}
	
	if( fbdiv_new%2 != 0 ) {
		dbg("cpufreq scaling: warning: fbdiv not even divisible, scaling to nearest freq.\n");
	}
	
	dbg("cpufreq: pll new dividers: opdiv=%d, fbdiv=%d, refdiv=%d\n", opdiv_new, fbdiv_new, refdiv_new);

	fbdiv_new = fbdiv_new/2;

	/* update fbdiv_new */
	pllr = cur_pllr;
	pllr = pllr & ~(0xff << 24);
	pllr = pllr | (fbdiv_new << 24);

	/* update opdiv_new */
	pllr = pllr & ~(0x3 << 16);
	pllr = pllr | (opdiv_new << 16);

	/* update refdiv_new */
	pllr = pllr & ~(0x1f << 8);
	pllr = pllr | (refdiv_new << 8);

	*new_pllr = pllr;
	new_freq = scrm_decode_pll(pllr, f_in);

	dbg("cpufreq: new pll out : %d KHz\n", new_freq/1000);
	return new_freq;
}



static int magus_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0)
		return -EINVAL;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);

	return 0;
}

static unsigned int magus_get_speed(unsigned int cpu)
{
	uint32_t freq;

	if (cpu)
		return 0;

	freq = magus_get_system_clk();

	return (freq/1000); /* in KHz */
}


/* Returns the cpu freq table index for the passed pll_out */
int cpufreq_table_lookup_pllout(struct cpufreq_pll_table *table, unsigned int pll_out)
{
	unsigned int i;

	for (i=0; (table[i].pll_out != TABLE_END); i++) {
		if( table[i].pll_out == pll_out )
			return i;
	}
	return -1;
}
/* Returns the cpu freq table index for the passed target freq */
int cpufreq_table_lookup_freq(struct cpufreq_pll_table *table, unsigned int target_freq)
{
	unsigned int i, j;

	for (i=0; (table[i].pll_out != TABLE_END); i++) {

		for(j=0; j < 3; j++ ) {
			if( table[i].valid_freqs[j] == target_freq )
				return i;
		}
	}
	return -1;
}

static int magus_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_freqs freqs;
	uint32_t pllr = 0;
	uint32_t freq, cur_arm_freq, cur_pll_out, target_pll_out, new_pll_freq;
	int index, i, pll_change_flag = 0;
	

	if(target_freq < policy->cpuinfo.min_freq)
		target_freq = policy->cpuinfo.min_freq;

	if(target_freq > policy->cpuinfo.max_freq)
		target_freq = policy->cpuinfo.max_freq;

	if(target_freq < policy->min)
		target_freq = policy->min;

	freq = target_freq * 1000;

	printk("\ncpufreq: magus_set_target: request for target_freq = %d KHz\n", target_freq);

	/* get the current pll out freq & arm freq */
	cur_pll_out = magus_get_pll_out();
	cur_arm_freq = magus_get_system_clk();

	printk("cpufreq: magus_set_target: cur_pll_out = %d KHz, cur_arm_freq = %d KHz\n", cur_pll_out/1000, cur_arm_freq/1000 );
#ifdef MAGUS_DDR
    if( (target_freq > 150000) && (target_freq < 180000) ) {
       printk("\nFrequencies between 150~180 MHz are not stable with DDR\n");
       return 0;
	}
#endif


	if( freq == cur_arm_freq ) return 0;

	/* check if the target freq can be achieved without changing the PLL out */
	/* find the freq table entry for the current pll out */
	pll_change_flag = 0;
	index = cpufreq_table_lookup_pllout(magus_freq_table, cur_pll_out/1000);

	if( index < 0 ) {
		printk("cpufreq: warning: unstable cur_pll_out..\n");
		/* round off ?? */
	}
	else {
		for(i=0; i < 3; i++) {
			if(magus_freq_table[index].valid_freqs[i] == target_freq) {
				pll_change_flag =1; /* pll change not reqd. */
				break;
			}
		}
		
	}
	if(pll_change_flag == 1) {
		printk("cpufreq: pll change not reqd, target requested freq %d KHz, cur pll out %d Kz \n", target_freq, cur_pll_out/1000);
	}
	else {
		/* PLL Change required */	

		/* Check the applicable pll_out freq for the target freq */
		index = cpufreq_table_lookup_freq(magus_freq_table, target_freq);
		if( index < 0 ) {
			dbg("cpufreq: warning: unstable pll_out..\n");
			/* round off ?? */
		}
		else {
			target_pll_out = (magus_freq_table[index].pll_out) * 1000; /* in HZ */

			dbg("cpufreq: pll change is reqd, target requested freq %d KHz, cur pll out %d Kz, reqd pll out %d Khz \n", target_freq, cur_pll_out/1000, target_pll_out/1000);

			
			/* Find out the pllr value for target pll out */
			new_pll_freq = magus_compute_pllr(&pllr, target_pll_out, CLKSRC_USBOTG_XTAL_IN, freq, relation);
			
		}
	}

	
	freqs.old = magus_get_speed(0);
	freqs.new = freq/1000;
	freqs.cpu = 0;
	freqs.flags = 0;

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	if( pll_change_flag == 0 ) {
		magus_set_new_freq(pllr, freq);
	}
	else {
		magus_set_new_freq(0, freq);
	}

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	magus_print_clocks();

	return 0;
}


static int __init magus_cpufreq_driver_init(struct cpufreq_policy *policy)
{

	dbg(KERN_INFO "MAGUS cpu freq change driver v1.0\n");

	if (policy->cpu != 0)
		return -EINVAL;

	policy->cur = policy->min = policy->max = magus_get_speed(0);
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	policy->cpuinfo.min_freq = 50 * 1000; /* in KHz, currently minimum freq constraint is 120 MHz */
	policy->cpuinfo.max_freq = 240 * 1000; /* in KHz, currently minimum freq constraint is 240 MHz */

	 /* PLL stabilizes in two CLK32 periods after restart signal is asserted */
	policy->cpuinfo.transition_latency = 4 * 1000000000LL / 32000; /* in nano seconds */

	return 0;
}


static struct cpufreq_driver magus_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= magus_verify_speed,
	.target		= magus_set_target,
	.get		= magus_get_speed,
	.init		= magus_cpufreq_driver_init,
	.name		= "magus",
};


static int __init magus_cpufreq_init(void)
{

	return cpufreq_register_driver(&magus_driver);
}

arch_initcall(magus_cpufreq_init);


struct cpufreq_pll_table * magus_get_freq_table(void)
{
	return (magus_freq_table);
}
EXPORT_SYMBOL(magus_get_freq_table);


