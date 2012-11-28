/**
 * @file         lcdcr.h
 * @author       Cliff Wong <cliffwong@solomon-systech.com>
 * @version      0.0.2
 * @data         2007-03-16
 *
 * This file provides the Register Map for LCDC.
 * (This part of code is based on the ground work by Wilson Leung & Jacky Cho)
 */

#ifndef __LCDCR_H__
#define __LCDCR_H__


typedef struct
{
	uint32_t	addr;	/**< Display Start Address */
	uint32_t	stride;	/**< Virtual Width in bytes */
	uint32_t	offset;	/**< XY offsets, LCDC_OFS() */
	uint32_t	size;	/**< Display Size, LCDC_SIZE() */
}
volatile lcdcr_win_t, *lcdcr_win_p;

typedef struct
{
	uint32_t	blink;		/**< Blink Configuration */
	uint32_t	addr;		/**< Memory Start Address */
	uint32_t	offset;		/**< XY Offsets */
	uint32_t	size;		/**< size */
	uint32_t	pal[3];		/**< Color Index 1-3 */
}
volatile lcdcr_curs_t, *lcdcr_curs_p;

typedef struct
{
	uint32_t	id;
	uint32_t	cap;
	uint32_t	ctl;
	uint32_t	intr;
	uint32_t	sta;
	uint32_t	pclkdiv;
	uint32_t	panel;
	uint32_t	mod;
	uint32_t	hdisp;
	uint32_t	vdisp;
	uint32_t	hsync;
	uint32_t	vsync_line;
	uint32_t	vsync_pix;
	uint32_t	lshift;
	uint32_t	frcs[2];	/**< Frame Rate Control Seed Registers */
	uint32_t	tftfrc;     /**< TFT Frame Rate Control */
	uint32_t	lut_dat;	/**< 23-16:red, 15-8:green, 7-0:blue */
	uint32_t	lut_idx;	/**< 0-255 color index */
	uint32_t	pwrsave;
	uint32_t	cfg;
	uint32_t	window;
	uint32_t	effect;
	uint32_t	maddr;		/**< Main Window Display Start Address */
	uint32_t	mstride;	/**< Main Window Virtual Width in bytes */
	uint32_t	msize;		/**< Main Window Display Size, LCDC_SIZE() */
	lcdcr_win_t	win[2];		/* floating windows */
	lcdcr_curs_t	cursor[2];	/**< Cursors */
	uint32_t	postproc;
	uint32_t	src[2][8];	/**< Source Signal0-7 Configuration 1-2 */
	uint32_t	gpio[5];    /**< GPIO Signal 0 Configuration */
}
volatile *lcdcr_p;

typedef struct
{
	uint32_t	cfg[2];
	uint32_t	timing[2];
	uint32_t	size[2];
	uint32_t	burst[2];
	uint32_t	cmd[2];
	uint32_t	data[2];
	uint32_t	line[2];
	uint32_t	wport;
	uint32_t	rport;
	uint32_t	sta;
}
volatile *slcdcr_p;

#define LCDC_ID_CLASSV		0x381
#define LCDC_ID_CLASS(d)	((d) >> 16)
#define LCDC_ID_DSG(d)		(((d) >> 10) & 0x3F)
#define LCDC_ID_VER(d)		((d) & 0x3FF)
#define LCDC_ID_MAJ(d)		(((d) >> 6) & 0x0F)
#define LCDC_ID_MIN(d)		((d) & 0x3F)

#define LCDC_CAP_AMBILIGHT	(1 << 6)
#define LCDC_CAP_DYNALIGHT	(1 << 5)
#define LCDC_CAP_RDBACK		(1 << 4)
#define LCDC_CAP_PP			(1 << 3)
#define LCDC_CAP_LUT		(1 << 2)
#define LCDC_CAP_STN		(1 << 1)
#define LCDC_CAP_DMA		(1 << 0)

#define LCDC_CTL_ABORT		(1 << 4)	/* smart panel data xfer abort */
#define LCDC_CTL_BURST		(1 << 3)	/* smart panel bursting enable */
#define LCDC_CTL_DUMBDIS	(1 << 2)	/* dumb panel enable */
#define LCDC_CTL_RST		(1 << 1)
#define LCDC_CTL_EN			(1 << 0)

#define LCDC_INT_UNRE		(1 << 3)
#define LCDC_INT_DONE		(1 << 2)	/* smart panel end of xfer */
#define LCDC_INT_DMAE		(1 << 1)
#define LCDC_INT_EOF		(1 << 0)	/* dumb panel end of frame */


/* serial even line format */
#define LCDC_PANEL_S2_BGR		(5 << 11)
#define LCDC_PANEL_S2_BRG		(4 << 11)
#define LCDC_PANEL_S2_GBR		(3 << 11)
#define LCDC_PANEL_S2_GRB		(2 << 11)
#define LCDC_PANEL_S2_RBG		(1 << 11)
#define LCDC_PANEL_S2_RGB		(0 << 11)
/* serial odd line format */
#define LCDC_PANEL_S1_BGR		(5 << 8)
#define LCDC_PANEL_S1_BRG		(4 << 8)
#define LCDC_PANEL_S1_GBR		(3 << 8)
#define LCDC_PANEL_S1_GRB		(2 << 8)
#define LCDC_PANEL_S1_RBG		(1 << 8)
#define LCDC_PANEL_S1_RGB		(0 << 8)
#define LCDC_PANEL_FMT_DELTA	(1 << 7)
#define LCDC_PANEL_FMT_STRIPE	(0 << 7)
#define LCDC_PANEL_COLOR		(1 << 6)
/* TFT pins */
#define LCDC_PANEL_PIN24		(3 << 4)
#define LCDC_PANEL_PIN18		(2 << 4)
#define LCDC_PANEL_PIN12		(1 << 4)
#define LCDC_PANEL_PIN9			(0 << 4)
/* STN pins */
#define LCDC_PANEL_PIN16		(2 << 4)
#define LCDC_PANEL_PIN8			(1 << 4)
#define LCDC_PANEL_PIN4			(0 << 4)
#define LCDC_PANEL_CS(n)		((n) << 3)	/* CS 0 or 1 */
#define LCDC_PANEL_T_SMART		4
#define LCDC_PANEL_T_MINIRGB	3
#define LCDC_PANEL_T_SERIAL		2
#define LCDC_PANEL_T_TFT		1
#define LCDC_PANEL_T_STN		0

#define LCDC_HDISP(start, total)				/* in pixels */ \
	((start) | (((total) - 1) << 16))
#define LCDC_VDISP(start, total)	LCDC_HDISP(start, total)

#define LCDC_HSYNC(start, sub, width) 		/* sub=0-3 for serial */ \
	((start) | ((sub) << 16) | (((width) - 1) << 24))

#define LCDC_VSYNC_LINE(start, width) \
	((start) | (((width) - 1) << 24))

#define LCDC_VSYNC_PIX(start, stop) \
	((start) | ((stop) << 16))

#define LCDC_LSHIFT(start, stop) LCDC_VSYNC_PIX(start, stop)

#define LCDC_CFG_BURST_M	(7 << 16)
#define LCDC_CFG_BURST_UND	(1 << 16)
#define LCDC_CFG_BURST_16	(6 << 16)
#define LCDC_CFG_BURST_8	(4 << 16)
#define LCDC_CFG_BURST_4	(2 << 16)
#define LCDC_CFG_BURST_1	(0 << 16)
#define LCDC_CFG_HSYNC_HIGH	(1 << 15)
#define LCDC_CFG_VSYNC_HIGH	(1 << 14)
#define LCDC_CFG_LSHIFT_RISE	(1 << 13)
#define LCDC_CFG_FRC_ROT	(1 << 12)
#define LCDC_CFG_FRC(n)		(((n) - 14) << 8)	/* 14-17 */
#define LCDC_CFG_BLANK		(1 << 7)
#define LCDC_CFG_DITHERDIS	(1 << 6)
#define LCDC_CFG_DITHERDYN	(1 << 5)
#define LCDC_CFG_INVERT		(1 << 4)
#define LCDC_CFG_CSTN256K	(1 << 3)
#define LCDC_CFG_CSTN32K	(0 << 3)
#define LCDC_CFG_BPP_M		7
#define LCDC_CFG_BPP_32		6
#define LCDC_CFG_BPP_16_	5
#define LCDC_CFG_BPP_16		4
#define LCDC_CFG_BPP_8		3
#define LCDC_CFG_BPP_4		2
#define LCDC_CFG_BPP_2		1
#define LCDC_CFG_BPP_1		0

/** floating window global alpha value, id=0-1, v=0-255 */
#define LCDC_WIN_FALP(i, v)	((v) << (16 + ((i) << 3)))
/** floating window pixel format id=0-1, v=LCDC_CFG_BPPx */
#define LCDC_WIN_FBPP(i, v)	((v) << (8 + ((i) * 3)))
/** floating window enable alphablend, id=0-1 */
#define LCDC_WIN_FBLEND(id)	(1 << (5 + ((id) << 1)))/**< alphablend */
/** floating window use global alpha for alphablend, id=0-1 */
#define LCDC_WIN_FGLBA(id)	(1 << (4 + ((id) << 1)))/**< use global alpha */
/** floating window enable, id=0-1 */
#define LCDC_WIN_FEN(id)	(1 << (2 + (id)))
/** cursor window enable, id=0-1 */
#define LCDC_WIN_CEN(id)	(1 << (id))

#define LCDC_SIZE(pixels, lines) \
	(((pixels) - 1) | (((lines) - 1) << 16))
#define LCDC_OFS(x, y) \
	(((x)) | (((y)) << 16))

#define LCDC_EFF_SCALE_8	(3 << 8)
#define LCDC_EFF_SCALE_4	(2 << 8)
#define LCDC_EFF_SCALE_2	(1 << 8)
#define LCDC_EFF_SCALE_1	(0 << 8)
#define LCDC_EFF_SWAP_4		(1 << 6)
#define LCDC_EFF_SWAP_2		(1 << 5)
#define LCDC_EFF_SWAP_1		(1 << 4)
#define LCDC_EFF_MIRROR		(1 << 2)
#define LCDC_EFF_ROT_270	3
#define LCDC_EFF_ROT_180	2
#define LCDC_EFF_ROT_90		1
#define LCDC_EFF_ROT_0		0
#define LCDC_EFF_GETROT(e)	((e) & 3)

#if 0
#define LCDCGETREG(addr) getreg32(LCDC_BASE + addr)
#define LCDCSETREG(addr,value) setreg32(LCDC_BASE + addr, value)
#define LCDCGETBITS(val, mask, pos) ((val >> pos) & mask)
#define LCDCSETBITS(val, mask, pos, data) {\
        val &= ~(mask << pos); \
        val |= (data & mask) << pos; }
#endif

#endif


