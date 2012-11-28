#ifndef _SSLFB_SASI_
#define _SSLFB_SASI_


/** platform configuration for floating windows */
typedef struct
{
	uint32_t	extramem;	/**< extra bytes beyond frame size to alloc/use */
	uint16_t	vheight;	/**< 0 -> same as win.height */
	lcdc_flt_t	win;		/**< win.addr = 0 -> alloc this space */
	uint8_t		enable;
}
sslfbw_plt_t;

/** platform configuration for main window and panel */
typedef struct
{
	uint32_t		extramem;	/**< extra bytes beyond frame size to alloc */
	uint16_t		vheight;	/**< 0 -> same as win.height */
	lcdc_win_t		win;		/**< win.addr = 0 -> alloc this space */
	lcdc_panel_t	panel;
	uint8_t			effect;
	uint8_t			fps;
}
sslfb_plt_t;


/* structure used by GET/SET_WIN */
typedef struct
{
	uint16_t	x, y;	/* move window pixel offsets from main window */
	uint8_t		alpha;	/* 0:opaque, 255:per pixel, 1-254:global */
}
sslfb_win_t;

/* structure used by GET/SET_CSR(2) */
typedef struct
{
	lcdc_curs_t	win;
	int			enable;
}
sslfb_cursor_t;

/* bits in uint32_t used by GET/SET_EFF */
#define SSLFB_EFF_ROTMASK	3	/* bits not to be changed */
#define SSLFB_EFF_MIRROR	4

#define SSLFBIOGET_WIN	0x46F0
#define SSLFBIOSET_WIN	0x46F1
#define SSLFBIOGET_EFF	0x46F2
#define SSLFBIOSET_EFF	0x46F3
#define SSLFBIOGET_CSR	0x46F4
#define SSLFBIOGET_CSR2	0x46F5
#define SSLFBIOSET_CSR	0x46F6
#define SSLFBIOSET_CSR2	0x46F7

/* FBIOBLANK,0/1 is used to enable/disable floating windows */


#endif

