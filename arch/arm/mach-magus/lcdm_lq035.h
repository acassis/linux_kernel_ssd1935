#ifndef _LCDM_LQ035_H_
#define _LCDM_LQ035_H_

#include "lcdc.h"
#include "sslfb.h"

static sslfb_plt_t	  lcdc_main =
{
	.win =
	{
		.a =
		{
#if defined CONFIG_ACCIO_LITE
			.addr = 0x51FDA000, 
#elif defined CONFIG_ACCIO_PF101_01_64MB || defined CONFIG_ACCIO_P1
			.addr = 0x53FB5000,
#elif defined CONFIG_ACCIO_PF101_01_32MB
			.addr = 0x51F00000,
#endif
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
		.panel = LCDC_PANEL_TFT,
		.pins = 24,
		.hdisp_pos = 68,
		.hdisp_len = 408,
		.vdisp_pos = 18,
		.vdisp_len = 262,
		.hsync_pos = 0,
		.hsync_sub = 0,
		.hsync_len = 1,
		.vsync_pos = 0,
		.vsync_len = 1,
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
#if defined CONFIG_ACCIO_LITE
					.addr = 0x51FB4000,
#elif defined CONFIG_ACCIO_PF101_01_64MB || defined CONFIG_ACCIO_P1
					.addr = 0x53F6A000,
#elif defined CONFIG_ACCIO_PF101_01_32MB
					.addr = 0x51F26000,
#endif
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
#if defined CONFIG_ACCIO_LITE
					.addr = 0x51F8E000,
#elif defined CONFIG_ACCIO_PF101_01_64MB || defined CONFIG_ACCIO_P1
					.addr = 0x53F1F000,
#elif defined CONFIG_ACCIO_PF101_01_32MB
					.addr = 0x51F4C000,
#endif
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
		.enable = 0,
	},
};

#endif

