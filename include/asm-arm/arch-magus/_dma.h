/**
@file		dma.h  - control API for Direct Memory Access (DMA)
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 11JUL06 last modified: 31JUL06
@todo
*/

#ifndef _DMA_H_
#define _DMA_H_


#define DMA_MAX_CHANNELS	32


/** DMA API returns */
typedef enum
{
	DMA_ERR_NONE	= 0,		/**< sucessful */
	DMA_ERR_HW		= -1,		/**< hardware error */
	DMA_ERR_PARM	= -2,		/**< parameter error */
	DMA_ERR_CFG		= -3,		/**< configuration error */
	DMA_ERR_STAT	= -4,		/**< status error */
	DMA_ERR_FULL	= -5
}
dma_err_e;

/** DMA event map */
typedef enum
{
	DMA_EV_DONE,
	DMA_EV_ERR
}
dma_evt_e;


/** Configuration modification flag */
#define DMA_F_ALL		(~0)
#define DMA_F_SRCADDR	(1 << 0)
#define DMA_F_DSTADDR	(1 << 1)
#define DMA_F_COUNT		(1 << 2)


/** settings for DMA channel attributes */
typedef struct
{
	uint8_t	width;		/**< access size: 0-byte(MEM) or 1/2/4/8-byte(FIFO) */
	uint8_t	depth;		/**< burst length = fifowidth* fifoheight*, 0~63 */
	uint8_t	req;		/**< DMA request, 0 no request **/
}
dma_fifo_t, *dma_fifo_p;

/** event callback **/
typedef struct
{
	void 	*ctx;							/**< context for callback */
	void	(*evt)(void *ctx, dma_evt_e e);	/**< event callback function */
}
dma_evt_t, *dma_evt_p;

/** configuration parameters of a DMA channel */
typedef struct
{
	int			flag;		/**< DMA_F_XXX ormask */

	uint32_t	saddr;		/**< source address */
	uint32_t	daddr;		/**< destination address */
	uint32_t	count;		/**< bytes to be transferred */

	dma_fifo_t	src;		/**< source FIFO attributes */
	dma_fifo_t	dst;		/**< destination FIFO attributes */
	dma_evt_t	evt;		/**< event callback */

	/* common attributes if both source and target are fifo */
	uint8_t		flush;		/**< 0 ignore fifo_flush_req; 
								1 sample fifo_flush_req */
}
dma_cfg_t, *dma_cfg_p;

/** DMA context */
typedef struct
{
/** public - initialized by client */
	void	*r;				/**< logical register base address */

	/* dma control; applicable to all channels */
	uint8_t	waitdelay;		/**< (waitdelay+1) system clock units */
	uint8_t	priority;		/**< 0 round-robin, 1 fixed priority */

/** private - zero-d by client on init & opaque after that */
	dma_evt_t	evt[DMA_MAX_CHANNELS];	/**< event callback */
	/* capabilities */
	uint8_t	channels;
	uint8_t	fifosize;		/**< fifo size */
	uint32_t	used;	/**< use bitmap: 1 occupied, 0 non-occupied */
	uint32_t	active;	/**< active bitmap: 1 transfering, 0 idle */
}
dma_t, *dma_p;


/** Device APIs */


dma_err_e dma_init(dma_p t);
/**<
initialize DMA module, reset then disable module
@param[in] t	context
@return    	DMA_ERR_XXX
*/

dma_err_e dma_exit(dma_p t);
/**<
disable the DMA module and exit
@param[in] t	context
@return		DMA_ERR_XXX
*/

int dma_request(dma_p t, dma_cfg_p cfg);
/**<
request a DMA channel and configure it
@param[in]	t	context
@param[in]	cfg	configuration of dma transfer by request
@return		channel assigned or DMA_ERR_XXX if negative
*/

dma_err_e dma_free(dma_p t, int ch);
/**<
free a DMA channel
@param[in]	t	context
@param[in]	ch	return from dma_request
@return		DMA_ERR_XXX
*/

dma_err_e dma_cfg(dma_p t, int ch, dma_cfg_p cfg);
/**<
reconfigure a requested channel
@param[in]	t	context
@param[in]	ch	return from dma_request
@param[in]	cfg	configuration of dma transfer by request
@return		DMA_ERR_XXX
*/

dma_err_e dma_enable(dma_p t, int channel);
/**<
start a DMA transfer under a request
@param[in]     t	context
@param[in]     cfg	configuration of dma transfer by request
@param[in|out] channel	channel to be assigned for this request
@return        		DMA_ERR_XXX
*/

dma_err_e dma_abort(dma_p t, int channel, uint32_t *count);
/**<
abort a DMA transfer & disable the channel
@param[in]  t		context
@param[in]  channel	channel index, 0~DMA_MAX_CHANNELS-1
@param[out] count	remaining number of bytes failed to be transfered
@return     		DMA_ERR_XXX (whether the channel is occupied)
*/

int dma_isr(dma_p t);
/**<
DMA interrupt serice routine
@param[in] t	context
@return 	0 = not processed, else processed
*/


#endif

