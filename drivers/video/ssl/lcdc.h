/**
 * @file         lcdc.h
 * @author       Cliff Wong <cliffwong@solomon-systech.com>
 * @version      0.0.2
 * @data         2007-01-27
 * @brief        LCDC Control Code Header File
 * 
 * This file provides the OS-independent control code for LCDC.
 * (This part of code is based on the ground work by Wilson Leung & Jacky Cho)
 */

#ifndef __SSD_LCDC_H__
#define __SSD_LCDC_H__

typedef struct
{
	void		*r;
	uint16_t	ver;
	unsigned	ambilight : 1;
	unsigned	dynalight : 1;
	unsigned	readback : 1;
	unsigned	postproc : 1;
	unsigned	lut : 1;
	unsigned	dma : 1;
	unsigned	stn : 1;
}
lcdc_t, *lcdc_p;

typedef struct
{
	uint32_t	addr;		/**< physical frame buffer address */
	uint16_t	width;		/**< window pixel width */
	uint16_t	height;		/**< window pixel height */
}
lcdc_area_t, *lcdc_area_p;

typedef struct
{
	lcdc_area_t	a;
	uint16_t	stride;		/**< window pixel stride, 0 = width */
	uint8_t		pixel;		/**< 0-5 for bits per pixel is  2^(pixel) */
}
lcdc_win_t, *lcdc_win_p;

typedef struct
{
	lcdc_win_t	w;
	uint16_t	x;
	uint16_t	y;
	uint8_t		alpha;		/**< 0-no alpha, 255-local alpha, else global */
}
lcdc_flt_t, *lcdc_flt_p;

typedef struct
{
	lcdc_area_t	a;			/**< a.width is in 16 pixel units */
	uint16_t	x;			/**< in 32-bit words */
	uint16_t	y;
	uint32_t	pal[3];		/**< palette for ids 1-3, (0 transparent) 
								follows main window pixel mode */
}
lcdc_curs_t, *lcdc_curs_p;

typedef union
{
	lcdc_win_t	w;
	lcdc_flt_t	f;
	lcdc_curs_t	c;
}
lcdc_cfg_t, *lcdc_cfg_p;

typedef enum
{
	LCDC_PANEL_STN,
	LCDC_PANEL_TFT,
	LCDC_PANEL_SERIAL,
	LCDC_PANEL_MINI_RGB,
	LCDC_PANEL_SMART
}
lcdc_panel_e;

typedef struct
{
	/* panel interface */
	uint8_t		panel;			/**< lcdc_panel_e */
	uint8_t		pins;			/**< STN:4,8,16 or TFT:9,12,18,24 */

	/* panel timing */
	uint16_t	hdisp_pos;
	uint16_t	hdisp_len;
	uint16_t	vdisp_pos;
	uint16_t	vdisp_len;
	uint16_t	hsync_pos;		/**< HSYNC pixel position */
	uint16_t	hsync_sub;		/**< HSYNC sub pixel position */
	uint16_t	hsync_len;		/**< HSYNC total pixels */
	uint16_t	vsync_pos;		/**< VSYNC line position */
	uint16_t	vsync_len;		/**< VSYNC total lines */
	uint16_t	vsync_start;	/**< VSYNC pixel position */
	uint16_t	vsync_end;		/**< VSYNC pixel end position */
	uint16_t	lshift_pos;		/**< LSHIFT pixel position */
	uint16_t	lshift_len;		/**< LSHIFT total pixels */

	/* panel signalling & storage flags */
	uint8_t		hsync_high : 1;
	uint8_t		vsync_high : 1;
	uint8_t		lshift_rise : 1;
	uint8_t		color : 1;
	uint8_t		delta : 1;
}
lcdc_panel_t, *lcdc_panel_p;

#define LCDC_ID_MAIN	0
#define LCDC_ID_FLT1	1
#define LCDC_ID_FLT2	2
#define LCDC_ID_CURS1	3
#define LCDC_ID_CURS2	4

int		lcdc_init(lcdc_p t);
void	lcdc_exit(lcdc_p t);

int		lcdc_panel_cfg(lcdc_p t, const lcdc_panel_p c);
int		lcdc_win_cfg(lcdc_p t, int id, const lcdc_cfg_p c);
int		lcdc_win_ena(lcdc_p t, int id, int ena);

int		lcdc_start(lcdc_p t, int fps, int clk);
int		lcdc_suspend(lcdc_p t, int flag);

int		lcdc_set_color(lcdc_p t, int id, const uint32_t *pal, int len);

int		lcdc_effect(lcdc_p t, uint32_t effect);


#endif

