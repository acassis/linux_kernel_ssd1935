#ifndef _LCDM_TD043_H_ 
#define _LCDM_TD043_H_ 

#include "lcdc.h"
#include "sslfb.h"

static sslfb_plt_t	  lcdc_main =
{
	.win =
	{
		.a =
		{
#ifdef CONFIG_FB_SSL_BPP32
			.addr = 0x53E80000,
#else
			.addr = 0x53F38000,	
#endif 
			.width = 800,
			.height = 480,
		},
#ifdef CONFIG_FB_SSL_BPP32
		.pixel = 5,
#else
		.pixel = 4,
#endif
	},
	.panel =
	{
		.panel = LCDC_PANEL_TFT,
		.pins = 24,
		.hdisp_pos = 216,
		.hdisp_len = 1056,
		.vdisp_pos = 35,
		.vdisp_len = 525,
		.hsync_pos = 0,
		.hsync_sub = 0,
		.hsync_len = 40,
		.vsync_pos = 0,
		.vsync_len = 7,
		.vsync_start = 0,
		.vsync_end = 0,
		.lshift_pos = 0,
		.lshift_len = 0,
		.hsync_high = 0,
		.vsync_high = 0,
		.lshift_rise = 0,
		.color = 1,
		.delta = 0,
	},
	.fps = 50,
};

static sslfbw_plt_t	 lcdc_flt[] =
{
	{
		.win =
		{
			.w =
			{
				.a =
				{
#ifdef CONFIG_FB_SSL_BPP32
					.addr = 0x53D00000,
#else
					.addr = 0x53E70000,
#endif
					.width = 800,
					.height = 480,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 480,
	},
	{
		.win =
		{
			.w =
			{
				.a =
				{
#ifdef CONFIG_FB_SSL_BPP32
					.addr = 0x53B80000,
#else
					.addr = 0x53DA8000,
#endif
					.width = 800,
					.height = 480,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 480,
	},
};

#endif

