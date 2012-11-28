/**
@file		nfc.h
@author		luanxu@solomon-systech.com
@version	1.0
@todo
*/

#ifndef	_SSL_NFC_
#define	_SSL_NFC_

//#define SSL_NFC_LOG_INFO
#ifdef SSL_NFC_LOG_INFO
#define NFC_DBG_INFO(fmt, args...)	printk("NFC - " fmt, ##args)
#else
#define NFC_DBG_INFO(fmt, args...)
#endif

#define SSL_NFC_LOG_ERR
#ifdef SSL_NFC_LOG_ERR
#define NFC_DBG_ERR(fmt, args...)      printk("NFC - " fmt, ##args)
#else
#define NFC_DBG_ERR(fmt, args...)
#endif

//#define SSL_NAND_LOG_INFO
#ifdef SSL_NAND_LOG_INFO
#define NAND_DBG_INFO(fmt, args...)   printk("SSL NAND - " fmt, ##args)
#else
#define NAND_DBG_INFO(fmt, args...)
#endif

#define SSL_NAND_LOG_ERR
#ifdef SSL_NAND_LOG_ERR
#define NAND_DBG_ERR(fmt, args...)   printk("SSL NAND - " fmt, ##args)
#else
#define NAND_DBG_ERR(fmt, args...)
#endif



/* ECC Mode */

#define NFC_ECC_MODE_HAM        0
/* Toshiba ECC4 */
#define NFC_ECC_MODE_TECC4      2
/* Toshiba ECC8 */
#define NFC_ECC_MODE_TECC8      3

/* Default ECC Offset */
#define NFC_ECC_OFFSET_DEFAULT  0

/* TECC4 Re-Decode Enable */
#define NFC_TECC4_REDEC_EN      1

/* TECC8 Redudancy bytes */
#define NFC_TECC8_REDN_CNT	0x17

/* Auto Correct Enable */
#define NFC_ACEN_EN             1

/* ECC Enable */
#define NFC_ECC_EN              1
#define NFC_ECC_DISEN           0


/** nfc API returns */
typedef enum
{
	NFC_ERR_NONE	= 0,	/**< successful */
	NFC_ERR_HW		= -1,	/**< hardware error */
	NFC_ERR_PARM	= -2,	/**< parameter error */
	NFC_ERR_TOUT	= -4	/**< timeout */
}
nfc_err;

/** nfc events */
#define NFC_EVT_ECC		0x80000000UL
#define NFC_EVT_ERR		2
#define NFC_EVT_DRDY	1
#define NFC_EVT_CMP		0

#define NFC_EVT_ECC_CORRECTED 4

/** nfc config */
typedef struct
{
	uint8_t		rowsz;		/**< row address size */
	uint8_t		colsz;		/**< column address size */
	uint8_t		blksz;		/**< block size */
	uint8_t		buswidth;	/**< bus width */
	uint8_t		pgsz;		/**< page size */
#if 0
	uint8_t		tweh;		/**< WE cycle high time */
	uint8_t		twel;		/**< WE cycle low time */
	uint8_t		treh;		/**< RE cycle high time */
	uint8_t		trel;		/**< RE cycle low time */
	uint8_t		taclh;		/**< ALE/CLE hold time */
	uint8_t		tacls;		/**< ALE/CLE setup time */
	uint8_t		trwsp;		/**< RE#/WE# separation time */
	uint8_t		twebusy;	/**< WE # to busy tiem */
#endif
}
nfc_cfg_t, *nfc_cfg_p;


/** nfc context */
typedef struct
{
	/** public - initialzed by client */
	void		*r;			/**< register base address */
	void		*ctx;		/**< context for event callback */
	void		(*evt)(void *ctx, uint32_t e);	/**< event callback */

	uint8_t		eccmode;	/**< ecc mode */
	uint8_t		eccofs;		/**< ecc offset */
	uint16_t	eccctrl;	/* TECC4 REDEC or TECC8 REDN value */
	uint8_t		ce;		/**< chip select */
	uint8_t		acen;	/**< auto correct enable */
	uint8_t		eccen;	/**< ecc enable */
	uint8_t		cached;	/**< cached read/write */
	uint8_t		twoplane; /**< two-plane operation */

	uint8_t		buswidth;	/**< bus width 16 */
	uint8_t		pgsz;	/**< page size */
	uint16_t	pgshift;	/**< page shift in address register */

	/** private - zero-d by client on init, opagque after that */
	uint16_t	ver;	/**< major version */
	uint8_t		bufsz;	/**< data buffer size */
}
nfc_t, *nfc_p;


nfc_err nfc_init(nfc_p t);
/**<
nfc initialization
@param[in]	t		context
@return		nfc error
*/

void nfc_exit(nfc_p t);
/**<
disable nfc device
@param[in]	t		context
*/


void nfc_cfg(nfc_p t, nfc_cfg_p c);
/**<
set the line configuration 1 register
@param[in]	t		context
@param[in]	c		line config parameters structure
*/

void nfc_cfg2(nfc_p t, nfc_cfg_p c);
/**<
set the line configuration 2 register
@param[in]      t               context
@param[in]      c               line config parameters structure
*/

nfc_err nfc_setecc(nfc_p t);
/**<
 Set ECC
@param[in]      t               context
@return         nfc error 
*/

int nfc_isr(nfc_p t);
/**<
interrupt service routine
@param[in]	t		context
@return		1=successfule, others=errors
*/

void nfc_write(nfc_p t, const char *buf, int ofs, int len);
/**<
write NFC buffer before issuing program command
@param[in]	t		context
@param[in]	buf		data buffer
@param[in]	ofs		offset into NFC buffer
@param[in]	len		byte length of data to write
*/

void nfc_read(nfc_p t, char *buf, int ofs, int len);
/**<
read NFC buffer after issuing read/readid command
@param[in]	t		context
@param[in]	buf		data buffer
@param[in]	ofs		offset into NFC buffer
@param[in]	len		byte length of data to read
*/

uint8_t	nfc_getstatus(nfc_p t);
/**<
read nand flash status 
@param[in]	t		context
@return 	value of status
*/

void nfc_nand_reset(nfc_p t);
/**<
send reset command
@param[in]	t		context
*/

void nfc_nand_id(nfc_p t, int bytecnt);
/**<
send read id command
@param[in]	t		context
@param[in]	bytecnt	bytes of id to be read
*/

void nfc_nand_status(nfc_p t);
/**<
send read status command
@param[in]	t		context
*/

void nfc_nand_read(nfc_p t, int pag);
/**<
send page read command
@param[in]	t		context
@param[in]	pag		page number
*/

void nfc_nand_prog(nfc_p t, int pag);
/**<
send page program command
@param[in]	t		context
@param[in]	pag		page number
*/

void nfc_nand_erase(nfc_p t, int pag);
/**<
send block erase command
@param[in]	t		context
@param[in]	pag		page number
*/


#endif

