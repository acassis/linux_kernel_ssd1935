#ifndef _LCDM_LQ043_H_
#define _LCDM_LQ043_H_

#include "lcdc.h"
#include "sslfb.h"

static sslfb_plt_t	  lcdc_main =
{
	.win =
	{
		.a =
		{
			.addr = 0x53FB5000,
			.width = 320,
			.height = 240,
		},
#ifdef CONFIG_FB_SSL_BPP32
		.pixel = 5,
#else
		.pixel = 4,
#endif
	},
	.panel =
	{
		.panel = LCDC_PANEL_SERIAL,
		.pins = 24,
		.hdisp_pos = 60,
		.hdisp_len = 390,
		.vdisp_pos = 21,
		.vdisp_len = 261,
		.hsync_pos = 0,
		.hsync_sub = 0,
		.hsync_len = 2,
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
		.delta = 1,
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
					.addr = 0x53F6A000,
					.width = 320,
					.height = 240,
				},
				.stride = 320,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 240,
	},
	{
		.win =
		{
			.w =
			{
				.a =
				{
					.addr = 0x53F1F000,
					.width = 320,
					.height = 240,
				},
				.stride = 320,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 240,
		.enable = 1,
	},
};

#endif

