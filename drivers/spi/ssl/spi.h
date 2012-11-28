/**
@file		spi.h
@author		sasin@solomon-systech.com
@version	1.0
@todo
*/

#ifndef _SPI_LUANXU_
#define _SPI_LUANXU_


/** spi API returns */
typedef enum
{
	SPI_ERR_NONE	= 0,	/**< successful */
	SPI_ERR_HW		= -1,	/**< hardware error */
	SPI_ERR_PARM	= -2,	/**< parameter error */
	SPI_ERR_TOUT	= -3	/**< timeout */
}
spi_err;


/**< events in event callback */
typedef enum
{
	SPI_EV_DONE,
	SPI_EV_ERR,
}
spi_ev_e;


/** spi master mode config */
typedef struct
{
	uint8_t		cs;			/**< chip select */
#if 1
	uint8_t		burst;		/**< burst word width */
#endif
	uint8_t		word;		/**< word bit width */
	uint8_t		csdly;		/**< half-bit time from CS to data */
	uint16_t	datdly;		/**< bit time between spi bursts */
	int			baud;		/**< baud rate */
	int			per_clk;	/**< peripheral clock in Hz */
	int			lsb_first : 1;
	int			clk_phase : 1;
	int			clk_fall : 1;
	int			cs_high : 1;
}
spi_cfg_t, *spi_cfg_p;


#define SPI_XFR_CS	1		/* assert CS before xfer */
#define SPI_XFR_NCS	2		/* de-assert CS after xfer */
#define SPI_XFR_DMA	4		/* using external DMA for xfer */

/** transfer descriptor */
typedef struct
{
	const uint8_t	*tx;	/**< aligned tx data buffer */
	uint8_t	*rx;			/**< aligned rx data buffer */
	int		len;			/**< buffer length in bytes */
	int		flag;			/**< SPI_XFR_xxx or-mask */
	int		actual;			/**< rx or tx data length */
}
spi_xfr_t, *spi_xfr_p;


/** spi context */
typedef struct
{
	/** public - initialized by client */
	void		*r;			/**< register base address */
	void		*ctx;		/**< context for evt callback */
	void		(*ev)(void *ctx, spi_ev_e e);	/**< evt callback */
	uint8_t		master;		/**< master mode */
	uint8_t		dma;		/**< dma enable */

	/** private - zero-d by client on init, opaque after that */
	spi_xfr_p	bp;			/**< current xfer descriptor */
	int			inc;		/**< word size = 2 ^ (inc + 3) */
	uint8_t		fifosize;	/**< FIFO word depth */
}
spi_t, *spi_p;



/* api */

spi_err spi_init(spi_p t);
/**<
initialize spi device
@param[in] t		context
@return		spi error
*/


void spi_exit(spi_p t);
/**<
disable spi device
@param[in] t		context
*/


spi_err spi_cfg(spi_p t, spi_cfg_p c);
/**<
spi master configeration
@param[in] t		context
@param[in] c		config parameters
@return				spi error
*/


int spi_isr(spi_p t);
/**<
spi interrupt service routine
@param[in] t		context
@return			1=handled, 0=not handled
*/


int spi_dmawait(spi_p t);
/**<
wait for spi xmit to finish
@param[in] t		context
return		0 - done, 1 - wait for event
*/


int spi_cs(spi_p t, int ena);
/**<
assert/deassert CS
@param[in]	t		context
@param[in]	ena		0-deassert else-assert
@return		0-ok
*/


int spi_io(spi_p t, spi_xfr_p bp);
/**<
xfer data between spi and data buffer
@param[in]	t		context
@param[in]	bp		xfer descriptor
@return		0-ok
*/


/* DMA resource extraction API */

uint32_t	spi_rxdma_addr(uint32_t phy);
/**<
get SPI RX DMA FIFO address for DMA configuration
@param[in]	phy		SPI register physical base address
@return		SPI RX DMA FIFO address
*/


uint32_t	spi_txdma_addr(uint32_t phy);
/**<
get SPI TX DMA FIFO address for DMA configuration
@param[in]	phy		SPI register physical base address
@return		SPI TX DMA FIFO address
*/

uint32_t	spi_rxdma_req(int req);
/**<
get SPI RX DMA FIFO request number for DMA configuration
@param[in]	req		SPI base DMA request number
@return		SPI RX DMA FIFO request number
*/

uint32_t	spi_txdma_req(int req);
/**<
get SPI TX DMA FIFO request number for DMA configuration
@param[in]	req		SPI base DMA request number
@return		SPI TX DMA FIFO request number
*/



/* internal */
#define spi_rxdma_addr(addr)		((uint32_t)(addr) + 0x24)
#define spi_txdma_addr(addr)		((uint32_t)(addr) + 0x28)
#define spi_rxdma_req(req)			((uint32_t)(req) + 1)
#define spi_txdma_req(req)			((uint32_t)(req))


#endif

