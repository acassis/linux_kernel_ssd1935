/**
@file		lcdc.c
@author		sasin@solomon-systech.com
@version	0.0.2
@date		2007-01-27
Hardware Bugs
ID ver 0 (Magus TO1)
- effect register SWAP bits are reversed
- ctl register DUMBDIS bit must be selected when toggling EN bit or it hangs
ID ver 1
- Magus TO2: version is not updated, so CAPs is checked
*/

#include "os.h"
#include "lcdc.h"
#include "lcdcr.h"

#include <linux/delay.h>

static int lcdc_reset(lcdcr_p r)
/*
bug - hangs on disable if DUMBDIS is not selected for both ctl writes
*/
{
	uint32_t	tout = 500000;

	io_wr32(r->ctl, LCDC_CTL_DUMBDIS | LCDC_CTL_RST | LCDC_CTL_EN);
	while (--tout)
	{
		if ((io_rd32(r->ctl) & (LCDC_CTL_RST | LCDC_CTL_EN)) == LCDC_CTL_EN)
		{
			break;
		}
	}
	if (!tout)
	{
		dbg("lcdc: init err - reset tout\n");
		return -1;
	}

	io_wr32(r->ctl, LCDC_CTL_DUMBDIS);

	/* defaults */
	io_wr32(r->cfg, 0);
	return 0;
}


void lcdc_exit(lcdc_p t)
{
	lcdc_reset((lcdcr_p)t->r);
}


int lcdc_init(lcdc_p t)
{
	uint32_t	cap, id;
	int			rt;
	lcdcr_p		r;

	r = (lcdcr_p)t->r;

	id = io_rd32(r->id);
	if (LCDC_ID_CLASS(id) != LCDC_ID_CLASSV)
	{
		dbg("lcdc: init err - id mismatch %X\n", id);
	}
	t->ver = LCDC_ID_VER(id);

	cap = io_rd32(r->cap);
	t->ambilight = !!(cap & LCDC_CAP_AMBILIGHT);
	t->dynalight = !!(cap & LCDC_CAP_DYNALIGHT);
	t->readback = !!(cap & LCDC_CAP_RDBACK);
	t->postproc = !!(cap & LCDC_CAP_PP);
	t->lut = !!(cap & LCDC_CAP_LUT);
	t->stn = !!(cap & LCDC_CAP_STN);
	t->dma = !!(cap & LCDC_CAP_DMA);
dbg("lcdc: init info - dsg=%X ver=%X\n"
"      abc=%d, dbc=%d rdback=%d pp=%d lut=%d stn=%d dma=%d\n",
LCDC_ID_DSG(id), t->ver, 
t->ambilight, t->dynalight, t->readback, t->postproc, t->lut, t->stn, t->dma);

	/* bugfix as IC did not update version number for TO2 */
	if (!t->ver && 
		((cap & (LCDC_CAP_AMBILIGHT | LCDC_CAP_DYNALIGHT | LCDC_CAP_RDBACK)) 
			== (LCDC_CAP_AMBILIGHT | LCDC_CAP_DYNALIGHT | LCDC_CAP_RDBACK)))
	{
dbg("lcdc: init warn - actually ver 0001\n");
		t->ver = 1;
	}

#ifndef CONFIG_ARCH_MAGUS_ACCIO
	rt = 0 ; //lcdc_reset(r); // not reset LCDC, bootloader will do it!
	if (rt)
	{
		return -1;
	}
#endif

	if (!t->ver)
	{
		io_wr32(r->effect, 
			io_rd32(r->effect) | LCDC_EFF_SWAP_4 | LCDC_EFF_SWAP_2);
	}

	return 0;
}


#if 0
static uint16_t rgb16(uint32_t rgb)
{
	return (rgb & 0x1f) | ((rgb & 0x3f00) >> 3) | ((rgb & 0x1f0000) >> 5);
}
#endif


static const uint8_t _bppset[] =
{
	LCDC_CFG_BPP_1, LCDC_CFG_BPP_2, LCDC_CFG_BPP_4, LCDC_CFG_BPP_8,
	LCDC_CFG_BPP_16_,
	LCDC_CFG_BPP_32,
};

static const uint8_t _bppget[] =
{
	0, 1, 2, 3, 4, 4, 5
};


int lcdc_win_cfg(lcdc_p t, int id, const lcdc_cfg_p c)
{
	lcdcr_p		r;
	uint32_t	stride, rot;

	r = (lcdcr_p)t->r;

	if (id < 3)
	{
		/* default stride to width, and convert from pixels to bytes */
		stride = c->w.stride;
		if (!stride)
		{
			stride = c->w.a.width;
		}
		stride <<= c->w.pixel - 3;

		/* update address for rotation */
		rot = LCDC_EFF_GETROT(io_rd32(r->effect));
		if (rot)
		{
			rot = (rot > LCDC_EFF_ROT_90) ?
					stride * (c->w.a.height - 1) : 
					(c->w.a.width - 1) << (c->w.pixel - 3);
		}
	}
	else
	{
		stride = rot = 0;
	}

	if (!id)
	{
		io_wr32(r->mstride, stride);
		io_wr32(r->msize, LCDC_SIZE(c->w.a.width, c->w.a.height));
		io_wr32(r->maddr, c->w.a.addr + rot);
		io_wr32(r->cfg, 
			(io_rd32(r->cfg) & ~(LCDC_CFG_BPP_M | LCDC_CFG_BURST_M))
				| _bppset[c->w.pixel] | LCDC_CFG_BURST_16);
	}
	else if (id < 3)
	{
		lcdcr_win_p	w;
		uint32_t	v;

		id--;
		w = &r->win[id];

		io_wr32(w->stride, stride);
		io_wr32(w->size, LCDC_SIZE(c->w.a.width, c->w.a.height));
		io_wr32(w->addr, c->w.a.addr + rot);
		io_wr32(w->offset, LCDC_OFS(c->f.x, c->f.y));
		v = io_rd32(r->window);
		if (!c->f.alpha)
		{
			v &= ~LCDC_WIN_FBLEND(id);
		}
		else
		{
			v |= LCDC_WIN_FBLEND(id);
			if (c->f.alpha != 255)
			{
				v &= ~LCDC_WIN_FALP(id, 255);
				v |= LCDC_WIN_FALP(id, c->f.alpha) | LCDC_WIN_FGLBA(id);
			}
			else
			{
				v &= ~LCDC_WIN_FGLBA(id);
			}
		}
		v &= ~LCDC_WIN_FBPP(id, LCDC_CFG_BPP_M);
		v |= LCDC_WIN_FBPP(id, _bppset[c->w.pixel]);
		io_wr32(r->window, v);
	}
	else if (id < 5)
	{
		lcdcr_curs_p	w;

		w = &r->cursor[id - 3];
		io_wr32(w->size, LCDC_SIZE(c->c.a.width, c->c.a.height));
		io_wr32(w->addr, c->c.a.addr);
		io_wr32(w->offset, LCDC_OFS(c->c.x, c->c.y));
		io_wr32(w->pal[0], c->c.pal[0]);
		io_wr32(w->pal[1], c->c.pal[1]);
		io_wr32(w->pal[2], c->c.pal[2]);
		io_wr32(w->blink, 0x10001);			/* don't blink */
	}
	else
	{
		return -1;
	}
	return 0;
}


int lcdc_win_ena(lcdc_p t, int id, int ena)
{
	lcdcr_p		r;
	uint32_t	v;

	if (!id || id > 4)
	{
		return -1;
	}

	/* map it to register bit-order & make bitmask */
	id = (id + 1) & 3;
	id = 1 << id;

	r = (lcdcr_p)t->r;
	v = io_rd32(r->window);
	if (!(v & id) != !ena)
	{
		io_wr32(r->window, v ^ id);
	}
	return 0;
}


int lcdc_panel_cfg(lcdc_p t, const lcdc_panel_p c)
{
	lcdcr_p		r;
	uint32_t	v;

	r = (lcdcr_p)t->r;

	/* panel specific & generic */
	v = c->hsync_high ? LCDC_CFG_HSYNC_HIGH : 0;
	v = c->vsync_high ? (LCDC_CFG_VSYNC_HIGH | v) : v;
	v = c->lshift_rise ? (LCDC_CFG_LSHIFT_RISE | v) : v;
#ifdef CONFIG_SERIAL_TFT 
	io_wr32(r->cfg, (io_rd32(r->cfg) & ~(LCDC_CFG_HSYNC_HIGH
		| LCDC_CFG_VSYNC_HIGH ))| LCDC_CFG_LSHIFT_RISE | v);
#else
	io_wr32(r->cfg, (io_rd32(r->cfg) & ~(LCDC_CFG_HSYNC_HIGH 
		| LCDC_CFG_VSYNC_HIGH | LCDC_CFG_LSHIFT_RISE)) | v);
#endif
	/* panel specific */
	io_wr32(r->hdisp, LCDC_HDISP(c->hdisp_pos, c->hdisp_len));
	io_wr32(r->vdisp, LCDC_VDISP(c->vdisp_pos, c->vdisp_len));
	io_wr32(r->hsync, LCDC_HSYNC(c->hsync_pos, c->hsync_sub, c->hsync_len));
	io_wr32(r->vsync_line, LCDC_VSYNC_LINE(c->vsync_pos, c->vsync_len));
	io_wr32(r->vsync_pix, LCDC_VSYNC_PIX(c->vsync_start, c->vsync_end));
	io_wr32(r->lshift, LCDC_LSHIFT(c->lshift_pos, c->lshift_len));
	v = c->pins;
	switch (v)
	{
		case 4: case 9:		v = LCDC_PANEL_PIN4; break;
		case 8: case 12:	v = LCDC_PANEL_PIN8; break;
		case 16: case 18:	v = LCDC_PANEL_PIN16; break;
		case 24: 			v = LCDC_PANEL_PIN24; break;
		default:			return -1;
	}
	v |= c->panel;
	if (c->delta)
	{
		v |= LCDC_PANEL_FMT_DELTA;
	}
	if (c->color)
	{
		v |= LCDC_PANEL_COLOR;
	}
#ifdef CONFIG_SERIAL_TFT 
	//v |= LCDC_PANEL_S1_BRG;
	v &= ~LCDC_PANEL_T_TFT;
	v |= LCDC_PANEL_T_MINIRGB;
#endif
	io_wr32(r->panel, v);
	return 0;
}


int lcdc_start(lcdc_p t, int fps, int clk)
/*
pclkdiv = ((hdisp_total * vdisp_total * fps * 1MiHz) / IN_CLK) - 1
1MHz in the spec is NOT 1000 * 1000 but 1024 * 1024
*/
{
	lcdcr_p		r;
	uint32_t	v;

	r = (lcdcr_p)t->r;

	v = (io_rd32(r->hdisp) >> 16) + 1;
	v *= (io_rd32(r->vdisp) >> 16) + 1;
	v *= fps;
	v /= clk >> 20;
	v--;

	/* regulate to prevent underrun */
	if (_bppget[io_rd32(r->cfg) & LCDC_CFG_BPP_M] == LCDC_CFG_BPP_32)
	{
		if (v >= 0x3ffff)
		{
			v = 0x3ffff;
		}
	}
	else if (v >= 0x7ffff)
	{
		v = 0x7ffff;
	}
	io_wr32(r->pclkdiv, v);

	/* start */
	io_wr32(r->ctl, LCDC_CTL_EN);
/* bug TO1 - clear LCDC_CTL_DUMBDIS */
	io_wr32(r->ctl, LCDC_CTL_EN);
	return 0;
}


int lcdc_suspend(lcdc_p t, int flag)
{
	volatile uint32_t	*r;
	volatile uint32_t	*psfcnt;

	r = &((lcdcr_p)t->r)->ctl;
	psfcnt = &((lcdcr_p)t->r)->pwrsave;

	if (flag)
	{
		io_wr32(*psfcnt, 0);
		io_wr32(*r, LCDC_CTL_DUMBDIS | LCDC_CTL_EN);
		msleep(60);
		io_wr32(*r, LCDC_CTL_DUMBDIS);
	}
	else
	{
		io_wr32(*r, LCDC_CTL_DUMBDIS | LCDC_CTL_EN);
		io_wr32(*r, LCDC_CTL_EN);
	}
	return 0;
}


int lcdc_smart_abort(lcdc_p t)
{
	lcdcr_p		r;

	r = (lcdcr_p)t->r;
	io_wr32(r->ctl, io_rd32(r->ctl) | LCDC_CTL_ABORT);
	return 0;
}


int lcdc_effect(lcdc_p t, uint32_t effect)
{
	lcdcr_p		r;

	r = (lcdcr_p)t->r;
	if (t->ver < 1)
	{
		effect |= LCDC_EFF_SWAP_4 | LCDC_EFF_SWAP_2;
	}
	io_wr32(r->effect, effect);
	return 0;
}


int lcdc_set_color(lcdc_p t, int i, const uint32_t *pal, int len)
{
	lcdcr_p		r;

	r = (lcdcr_p)t->r;
	io_wr32(r->lut_idx, i);
	while (len--)
	{
		io_wr32(r->lut_dat, *pal);
		pal++;
	}
	return 0;
}

