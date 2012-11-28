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
			.addr = 0x53F80000,
			.width = 480,
			.height = 272,
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
#if defined CONFIG_ACCIO_P1_SK01
		.pins = 24,
#else
		.pins = 18,
#endif
		.hdisp_pos = 47,
		.hdisp_len = 525,
		.vdisp_pos = 10,
		.vdisp_len = 286,
		.hsync_pos = 2,
		.hsync_sub = 0,
		.hsync_len = 41,
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
					.addr = 0x53F00000,
					.width = 480,
					.height = 272,
				},
				.stride = 480,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 272,
	},
	{
		.win =
		{
			.w =
			{
				.a =
				{
					.addr = 0x53E80000,
					.width = 480,
					.height = 272,
				},
				.stride = 480,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,
#else
				.pixel = 4,
#endif
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 272,
		.enable = 1,
	},
};

#endif

