/**
@file		dmar.h - hardware dependent definitions for DMA
@author		shaowei@solomon-systech.com
@version 	1.0
@date		created: 11JUL06 modified: 13JUL06
@todo
*/

#ifndef _DMAR_H_
#define _DMAR_H_


#define _DMAC_CLID	0x0801		/* Module ID */
#define _DMAC_DSGNR	0x01		/* Module Design number */

#define DMA_CHANNEL_BASE	0x0100
#define DMA_CHANNEL_OFFSET	0x0020

#pragma pack(4)

/* hardware map of each channel */
typedef struct
{
	uint32_t	cfg;
	uint32_t	saddr;		/**< source address */
	uint32_t	daddr;		/**< destination address */
	uint32_t	count;		/**< transfer count in bytes */
	uint32_t	ier;
	uint32_t	isr;
	uint32_t	_rsv[DMA_CHANNEL_OFFSET/4 - 6];
} 
volatile dmar_ch_t, *dmar_ch_p;


/* hardware map of DMA module */
typedef struct
{
	uint32_t	id;
	uint32_t	cap;
	uint32_t	ctl;
	uint32_t	_rsv1;
	uint32_t	ier;
	uint32_t	isr;
	uint32_t	_rsv2[DMA_CHANNEL_BASE/4 - 6];
	dmar_ch_t	ch[DMA_MAX_CHANNELS];
} 
volatile dmar_t, *dmar_p;

#pragma pack()


/* ID */
#define DMA_ID_CLID(d)	((d) >> 16)
#define DMA_ID_DSGNR(d)	(((d) >> 10) & 0x3F)
#define DMA_ID_MAJ(d)	(((d) >> 6) & 0x0F)
#define DMA_ID_MIN(d)	((d) & 0x3F)
/* capabilities */
#define DMA_CAP_FIFOSZ(d)	(1 << ((((d) >> 2) & 0x03) + 5))
#define DMA_CAP_CHN(d)		((((d) & 0x03) + 1) * 8)
/* control settings */
#define DMA_CTL_WAIT(n)	((n) << 8)
#define DMA_CTL_PRI		4
#define DMA_CTL_RST		2
#define DMA_CTL_EN		1
/* channel index */
#define DMA_CH_IDX(ch)	(1 << (ch))
/* channel config */
#define DMA_CFG_SWREQ		0x10			/* TO2 */
#define DMA_CFG_FLUSH		8
#define DMA_CFG_RST			4
#define DMA_CFG_DIS			2
#define DMA_CFG_EN			1
#define DMA_CFG_BURST(n)	(((n) - 1) << 8)	/* n = 1..128 bytes */
#define DMA_CFG_SFIFO		(1 << 16)
#define DMA_CFG_SWIDTH(n)	((n) << 18)			/* n = 0-3 for 1,2,4,8 bytes */
#define DMA_CFG_SREQ(n)		((n) << 20)			/* n = 0-15 */
#define DMA_CFG_DFIFO		(1 << 24)
#define DMA_CFG_DWIDTH(n)	((n) << 26)			/* n = 0-3 for 1,2,4,8 bytes */
#define DMA_CFG_DREQ(n)		((n) << 28)			/* n = 0-15 */
#define DMA_CFG_SRC(width, req) \
	/* width = 0-3 for 1,2,4,8 bytes */ \
	(DMA_CFG_SFIFO | DMA_CFG_SWIDTH(width) | DMA_CFG_SREQ(req))
#define DMA_CFG_DST(width, req) \
	(DMA_CFG_DFIFO | DMA_CFG_DWIDTH(width) | DMA_CFG_DREQ(req))

/* channel event */
#define DMA_IRQ_ERR		0x80
#define DMA_IRQ_DONE 	0x01
/* channel status */
#define DMA_STA_FLUSH 	0x10
#define DMA_STA_DRQ		0x08
#define DMA_STA_SRQ		0x04
#define DMA_STA_BSY		0x02

/*
#define DMA_CH_DSTREQ	0xF0000000
#define DMA_CH_DSTASZ	0xC000000
#define DMA_CH_DSTTYP	0x3000000
#define DMA_CH_SRCREQ	0xF00000
#define DMA_CH_SRCASZ	0xC0000
#define DMA_CH_SRCTYP	0x30000
#define DMA_CH_BL	0x3F00
#define DMA_CH_FLSHEN	0x08
*/

#endif
