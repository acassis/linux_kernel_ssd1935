/**
@file		dma.c - control code for Direct Memoery Access (DMA)
@author 	shaowei@solomon-systech.com
@version 	1.0
@date		created: 11JUL06 last modified: 31JUL06
@todo		value of burstlen: 0~63 or 1~64 ?
**/

#include "os.h"
#include "dma.h"
#include "dmar.h"


#define _TIMEOUT	300

static dma_err_e _dma_reset(dma_p t);
static dma_err_e _dma_fifoparm(dma_fifo_p fifo,
					uint8_t fifosize, uint32_t count, uint8_t *fifowidth);
static int _dma_alloc(dma_p t);


/* device based API*/

dma_err_e dma_init(dma_p t)
/*
initialize DMA module, reset then disable module
@param[in]	t	context
@return		DMA_ERR_XXX
*/
{
	dmar_p	r;
	uint32_t	id, caps;
#if 0
	int			i;
#endif
	dma_err_e 	err;

	r = (dmar_p)t->r;

	/* check ID */
	id = io_rd32(r->id);
	if ((DMA_ID_CLID(id) != _DMAC_CLID) || (DMA_ID_DSGNR(id) != _DMAC_DSGNR))
	{
		dbg("dma: init err - id mismatch, class exp=%08X got=%08X, "
			"design exp=%06X got=%06X \n",
			_DMAC_CLID, DMA_ID_CLID(id), _DMAC_DSGNR, DMA_ID_DSGNR(id));
		return DMA_ERR_HW;
	}

	/* enable and reset the module*/
	err = _dma_reset(t);
	if (err)
	{
		return err;
	}

	/* check capabilities */
	caps = io_rd32(r->cap);

#if 0
	t->channels = i = DMA_CAP_CHN(caps);
	while (i--)
	{
		uint32_t	cfg;

		cfg = io_rd32(r->ch[i].cfg);
		if (cfg != 0x0C0C0050)
		{
			dbg("dma: init err - channel%d cfg rst value %32X \n", i, cfg);
			return DMA_ERR_HW;
		}
	}
#else
	t->channels = DMA_CAP_CHN(caps);
#endif

	t->fifosize = DMA_CAP_FIFOSZ(caps);
	t->used = t->active = 0;

	dbg("dma: init info - ver %u.%u fifosize=%u, %u channels\n",
		DMA_ID_MAJ(id), DMA_ID_MIN(id),
		t->fifosize, t->channels);
	if (t->channels > DMA_MAX_CHANNELS)
	{
		t->channels = DMA_MAX_CHANNELS;
	}

	/* set client control */
	io_wr32(r->ctl, DMA_CTL_WAIT(t->waitdelay) | 
		(t->priority ? DMA_CTL_PRI : 0) | DMA_CTL_EN);

	return DMA_ERR_NONE;
}


int dma_free(dma_p t, int ch)
{
	dmar_p	r;

	r = (dmar_p)t->r;
	io_wr32(r->ch[ch].cfg, DMA_CFG_RST);
	t->used &= ~DMA_CH_IDX(ch);
	return 0;
}


#define DMA_WIDTH_ERR(w)	((w) > 8 || (w) == 3 || ((w) > 4 && (w) < 7))

int dma_cfg(dma_p t, int ch, dma_cfg_p cfg)
{
	dmar_p	r;
	dmar_ch_p	chr;

	r = (dmar_p)t->r;
	chr = &r->ch[ch];

//printk("dma_cfg ch%d flag=%X s=%X d=%X l=%d\n", 
//ch, cfg->flag, cfg->saddr, cfg->daddr, cfg->count);
	if (cfg->flag == DMA_F_ALL)
	{
		uint8_t	swidth, dwidth;
		uint8_t	burst;

		/* check cfg parameter settings */
		swidth = cfg->src.width;
		dwidth = cfg->dst.width;
		if (DMA_WIDTH_ERR(swidth) || DMA_WIDTH_ERR(dwidth))
		{
			dbg("dma: cfg err - FIFO width range s%d d%d\n", swidth, dwidth);
			return DMA_ERR_PARM;
		}
		if (swidth && dwidth)
		{
			if (swidth != dwidth || cfg->src.depth != cfg->dst.depth)
			{
				dbg("dma: cfg err - src/dst FIFO dimensions unmatched\n");
				return DMA_ERR_PARM;
			}
		}

		burst = 64;
		if (swidth)
		{
			dma_err_e	err;

			err = _dma_fifoparm(&cfg->src, 
						t->fifosize, cfg->count, &swidth);
			if (err)
			{
				return err;
			}
			burst = cfg->src.width * cfg->src.depth;
		}
		if (dwidth)
		{
			dma_err_e	err;

			err = _dma_fifoparm(&cfg->dst, 
						t->fifosize, cfg->count, &dwidth);
			if (err)
			{
				return err;
			}
			burst = cfg->dst.width * cfg->dst.depth;
		}

		/* assign event callbacks to the channel*/
		t->evt[ch] = cfg->evt;

		/* Configure channel */
		io_wr32(chr->cfg, DMA_CFG_BURST(burst) |
			(cfg->flush ? DMA_CFG_FLUSH : 0) |
			(cfg->src.width ? DMA_CFG_SRC(swidth, cfg->src.req) : 0) |
			(cfg->dst.width ? DMA_CFG_DST(dwidth, cfg->dst.req) : 0));
	}

	if (cfg->flag & DMA_F_SRCADDR)
	{
		io_wr32(chr->saddr, cfg->saddr);
	}
	if (cfg->flag & DMA_F_DSTADDR)
	{
		io_wr32(chr->daddr, cfg->daddr);
	}
	if (cfg->flag & DMA_F_COUNT)
	{
		io_wr32(chr->count, cfg->count);
	}
	return DMA_ERR_NONE;
}


int dma_request(dma_p t, dma_cfg_p cfg)
{
	int	ch, rt;
	dmar_p	r;
	dmar_ch_p	chr;

	if (cfg->flag != DMA_F_ALL)
	{
		return DMA_ERR_CFG;
	}

	ch = _dma_alloc(t);
	if (ch < 0)
	{
		return DMA_ERR_FULL;
	}

	rt = dma_cfg(t, ch, cfg);
	if (rt)
	{
		dma_free(t, ch);
		return rt;
	}

	/* enable interrupt of this channel */
	r = (dmar_p)t->r;
	chr = &r->ch[ch];
	io_wr32(r->ier, io_rd32(r->ier) | DMA_CH_IDX(ch));
	io_wr32(chr->ier, DMA_IRQ_ERR | DMA_IRQ_DONE);

	return ch;
}


dma_err_e dma_enable(dma_p t, int ch)
/*
start a DMA transfer under a request
@param[in]	t		context
@param[in]	ch		channel to be assigned for this request
@return 		DMA_ERR_XXX
*/
{
	dmar_p	r;
	dmar_ch_p	chr;

#if 0
	/* ensure channel is valid, configured and not active */
	if (ch >= t->channels)
	{
		dbg("dma: start err - invalid channel %d\n", ch);
		return DMA_ERR_PARM;
	}
	if (!(t->used & DMA_CH_IDX(ch)))
	{
		dbg("dma: enable err - unconfigured channel%d\n", ch);
		return DMA_ERR_CFG;
	}
	if (t->active & DMA_CH_IDX(ch))
	{
		dbg("dma: enable err - active channel%d\n", ch);
		return DMA_ERR_CFG;
	}
#endif

	r = (dmar_p)t->r;
	chr = &r->ch[ch];

	/* power saving: enable dma clk for first active channel */
#if 0
	if (!t->active)
	{
		io_wr32(r->ctl, io_rd32(r->ctl) | DMA_CTL_EN);
	}
#endif

	t->active |= DMA_CH_IDX(ch);

#if 0
dbg("dma: enable info - ctl=%X ier=%X ch%d cfg=%X ier=%X s=%X d=%X l=%d\n", 
io_rd32(r->ctl), io_rd32(r->ier), ch, io_rd32(chr->cfg), io_rd32(chr->ier),
io_rd32(chr->saddr), io_rd32(chr->daddr), io_rd32(chr->count));
#endif

	io_wr32(chr->cfg, io_rd32(chr->cfg) | DMA_CFG_EN);

	return DMA_ERR_NONE;
}


dma_err_e dma_abort(dma_p t, int ch, uint32_t *count)
/*
abort a DMA transfer (disable channel) and get the number of bytes failed to be transfered
@param[in]	t		context
@param[in]	ch		channel index, 0 ~ DMA_MAX_CHANNELS-1
@param[out] count	remaining number of bytes failed to be tranfered
@return		DMA_ERR_XXX (whether the channel is occupied)
*/
{
	dmar_p r;
	dmar_ch_p chr;

	if ((ch > t->channels - 1) || (ch < 0))
	{
		dbg("dma: abort err - invalid channel %d\n", ch);
		return DMA_ERR_PARM;
	}

	r= (dmar_p)t->r;
	chr = &(r->ch[ch]);

	if (!(t->active & DMA_CH_IDX(ch)))
	{
		dbg("dma: abort info - inactive channel%d\n", ch);
		*count = 0;
		return DMA_ERR_NONE;
	}

	/* disable this channel */
	io_wr32(chr->cfg, io_rd32(chr->cfg) | DMA_CFG_DIS);
	t->active &= ~DMA_CH_IDX(ch);
	*count = io_rd32(chr->count);
	return DMA_ERR_NONE;
}


dma_err_e dma_exit(dma_p t)
/*
disable the dma module and exit
@param[in] t	context
@return		DMA_ERR_XXX
*/
{
	dma_err_e	err;
	dmar_p	r;

	r = (dmar_p)t->r;
	/* reset and disable the module */
	err = _dma_reset(t);
	if (err)
	{
		return err;
	}
	return DMA_ERR_NONE;
}


int dma_isr(dma_p t)
/*
interrupt service routine
@param[in] t	context
@return 	0 - not processed, else processed
*/
{
	uint32_t	sta;
	dmar_p		r;
	dmar_ch_p	chr;
	int			ch;
	int			handled;

	r = (dmar_p)t->r;

	/* check interrupt channel */
	sta = io_rd32(r->isr);
	if (!sta)
	{
		dbg("dma: isr err - none pending\n");
		return 0;
	}
	io_wr32(r->isr, sta);

	handled = 0;
	chr = r->ch;
	for (ch = 0; ch < t->channels; ch++, chr++)
	{
		uint32_t	csta;

		if (!(sta & DMA_CH_IDX(ch)))	/**< channel of interruption **/
		{
			continue;
		}

		/* check the interrupted channel interrupt status */
		csta = io_rd32(chr->isr);
		if (!csta)
		{
			dbg("dma: isr warn - none pending for ch%d\n", ch);
			continue;
		}
		handled++;

		t->active &= ~DMA_CH_IDX(ch);
		if (csta & DMA_IRQ_ERR)
		{
dbg("dma: isr err - ch%d count=%d\n", ch, chr->count);
			/* reset channel */
			io_wr32(chr->cfg, DMA_CFG_RST);
			while (io_rd32(chr->cfg) & DMA_CFG_RST)
			{
				;
			}
		}
		t->evt[ch].evt(t->evt[ch].ctx, 
			(csta & DMA_IRQ_ERR) ? DMA_EV_ERR : DMA_EV_DONE);
	}

	/* power saving: disable dma clk if no channels active */
#if 0
	if (!t->active)
	{
		dma_exit(t);
	}
#endif

	return (!!handled);
}


/* internal */


static dma_err_e _dma_reset(dma_p t)
/*
reset the dma module
@param[in] t	context
@return 	DMA_ERR_XXX
*/
{
	int	i;
	dmar_p	r;

	r  = (dmar_p)t->r;
	io_wr32(r->ctl, DMA_CTL_RST | DMA_CTL_EN);

	/* reset timeout */
	i = _TIMEOUT;
	while (io_rd32(r->ctl) & DMA_CTL_RST)
	{
		if (!i--)
		{
			dbg("dma: reset err - reset hang\n");
			return DMA_ERR_HW;
		}
	}

	io_wr32(r->ctl, 0);
	return DMA_ERR_NONE;
}


static dma_err_e _dma_fifoparm(dma_fifo_p fifo, 
					uint8_t size, uint32_t count, uint8_t *width)
/*
check validity of fifo parameters and return binary fifowidth coding
@param[in]	fifo		DMA channel fifo setting
@param[in]	fifosize	capability of FIFO size
@param[in]	count		number of bytes to be transfered
@param[out]	fifowidth	bindary coding for FIFO width
@return			DMA_ERR_XXX
*/
{
	/* w: fifo_width; b: burst_length */
	uint8_t	w, b;

	/* check fifowidth (access size) */
	b = fifo->width;
	w = b >> 1;
	if (w == 4)
	{
		w--;
	}
	else if (w > 4)
	{
		dbg("dma: cfg err - invalid FIFO access size %d\n", b);
		return DMA_ERR_PARM;
	}

	/* ensure divisible */
	if (count & (b - 1))
	{
		dbg("dma: cfg err - count not multiple of FIFO access size\n");
		return DMA_ERR_PARM;
	}

	/* check burst length vs fifo size */
	b *= fifo->depth;
	if (b > size)
	{
		dbg("dma: cfg err - burst length %d > FIFO size %d\n", b, size);
		return DMA_ERR_PARM;
	}

	*width = w;
	return DMA_ERR_NONE;
}


static int _dma_alloc(dma_p t)
/*
request for a un-occupied DMA channel
@param[in]		t	context
return			channel number or -1
*/
{
	int	ch, max;

	max = t->channels;
	for (ch = 0; ch < max; ch++)
	{
		if (!(t->used & DMA_CH_IDX(ch)))
		{
			t->used |= DMA_CH_IDX(ch);
			return ch;
		}
	}

	dbg("dma: alloc err - no free channel\n");
	return -1;
}

