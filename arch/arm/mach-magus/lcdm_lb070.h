#ifndef _LCDM_LB070_H_
#define _LCDM_LB070_H_

#include "lcdc.h"
#include "sslfb.h"

static sslfb_plt_t	lcdc_main =
{
	.win =
	{
		.a =
		{
			.addr = 0x93E00000,
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
		.pins = 18,
		.hdisp_pos = 160,
		.hdisp_len = 1056,
		.vdisp_pos = 10,
		.vdisp_len = 525,
		.hsync_pos = 0,
		.hsync_sub = 0,
		.hsync_len = 128,
		.vsync_pos = 0,
		.vsync_len = 2,
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

static sslfbw_plt_t	lcdc_flt[] =
{
	{
		.win =
		{
			.w =
			{
				.a =
				{
					.addr = 0x93D00000,
					.width = 100,
					.height = 100,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
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
					.addr = 0x93C00000,
					.width = 100,
					.height = 100,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 700,
			.y = 380,
		},
		.vheight = 480,
	},
};

#endif

