/**
@file		gpiodev.h - hardware dependent definitions for General Purpose Input Output (GPIO) module
@author		shaowei@solomon-systech.com
@version	1.0
@date		created: 18JUL06, last modified 7AUG06
@todo  
*/

#ifndef	_GPIOR_H_
#define	_GPIOR_H_


#define _GPIO_CLID	0x0810
#define _GPIO_DSGNR	0x0


#pragma pack(4)

typedef struct
{
	uint32_t	idr;		/**< device identification register */
	uint32_t	capr;		/**< capabilities register */
	uint32_t	_rsv[2];	/**< reserved */
	uint32_t	ctrl;		/**< control register */
	uint32_t	func;		/**< function register */
	uint32_t	mode;		/**< mode register */
	uint32_t	puen;		/**< pull enable register */
	uint32_t	ddir;		/**< data direction register */
	uint32_t	oss1;		/**< output source select register 1 */
	uint32_t	oss2;		/**< output source select register 2 */
	uint32_t	issa1;		/**< input source slect A register 1 */
	uint32_t	issa2;		/**< input source slect A register 2 */
	uint32_t	issb1;		/**< input source slect B register 1 */
	uint32_t	issb2;		/**< input source slect B register 2 */
	uint32_t	dreg;		/**< data register */
	uint32_t	icr1;		/**< interrupt configuration register 1 */
	uint32_t	icr2;		/**< interrupt configuration register 2 */
	uint32_t	ier;		/**< interrupt enable register */
	uint32_t	isr;		/**< interrupt status register */	
}
volatile gpior_t, *gpior_p;

#pragma pack()

#define GPIO_ID_CLID(d)	( (d) >> 16 )
#define GPIO_ID_DSGNR(d)	( ( (d) >> 10) & 0x3F )
#define GPIO_ID_MAJ(d)	( ( (d) >> 6 ) & 0x0F )
#define GPIO_ID_MIN(d)	( (d) & 0x3F )

#define GPIO_CTRL_PMSK	0x02
#define GPIO_CTRL_RST	0x01
#define GPIO_PIN_IDX(d)	( (0x01)<<(d) )

//#define GPIO_PIN_DIDX(d)	( 0x03<<( (d) >16 ? (2*i-32) : (2*i) ) )
//#define GPIO_BIT_VAL(d,v)	( (v) << (d) )

#endif
