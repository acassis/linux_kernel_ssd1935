/**
@file		intc
@author		sasin@solomon-systech.com
Control code for Solomon Systech Interrupt Controller
*/

#ifndef _INTC_SASI_
#define _INTC_SASI_


int		intc_init(void *reg);
/**<
initialize & reset h/w
@param	reg		base register address
@return	INTC_ERR_XXX
*/

void	intc_exit(void *reg);
/**<
reset h/w
@param	reg		base register address
*/

void	intc_unmask(void *reg, int intr);
/**<
unmask an interrupt
@param	reg		base register address
@param	intr	interrupt source number
@return	INTC_ERR_XXX
*/

void	intc_mask(void *reg, int intr);
/**<
mask an interrupt
@param	reg		base register address
@param	intr	interrupt source number
@return	INTC_ERR_XXX
*/

int		intc_pend(void *reg);
/**<
get highest priority interrupt pending service
@param	reg		base register address
@return	interrupt source number
*/

void	intc_ack(void *reg, int intr);
/**<
acknowledges interrupt and clears status
@param	reg		base register address
@param	reg		interrupt source number
*/


#endif

