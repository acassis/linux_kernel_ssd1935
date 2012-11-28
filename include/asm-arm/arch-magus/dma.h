/**
@file		ssldma.h  - control API for Direct Memory Access (DMA)
@author 	sasin@solomon-systech.com
@version 	1.0
*/

#ifndef _SSLDMA_H_
#define _SSLDMA_H_


#include <asm/arch/_dma.h>


int ssldma_request(dma_cfg_p cfg);
/**<
request a DMA channel and configure it
@param[in]	cfg	configuration of dma transfer by request
@return		channel assigned or DMA_ERR_XXX if negative
*/

dma_err_e ssldma_free(int ch);
/**<
free a DMA channel
@param[in]	ch	return from dma_request
@return		DMA_ERR_XXX
*/

dma_err_e ssldma_cfg(int ch, dma_cfg_p cfg);
/**<
reconfigure a requested channel
@param[in]	ch	return from dma_request
@param[in]	cfg	configuration of dma transfer by request
@return		DMA_ERR_XXX
*/

dma_err_e ssldma_enable(int ch);
/**<
start a DMA transfer under a request
@param[in|out]	ch	return from dma_request
@return        	DMA_ERR_XXX
*/

dma_err_e ssldma_abort(int ch, uint32_t *count);
/**<
abort a DMA transfer & disable the channel
@param[in]  ch		channel index, 0~DMA_MAX_CHANNELS-1
@param[out]	count	remaining number of bytes failed to be transfered
@return     		DMA_ERR_XXX (whether the channel is occupied)
*/


/* internal */

extern dma_t	ssldma;

#define ssldma_request(cfg) dma_request(&ssldma, cfg)
#define ssldma_free(ch)		dma_free(&ssldma, ch)
#define ssldma_cfg(ch, cfg)	dma_cfg(&ssldma, ch, cfg)
#define ssldma_enable(ch)	dma_enable(&ssldma, ch)
#define ssldma_abort(ch, c)	dma_abort(&ssldma, ch, c)


#endif

