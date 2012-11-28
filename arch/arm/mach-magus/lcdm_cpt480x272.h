#ifndef _LCDM_LQ035_H_
#define _LCDM_LQ035_H_

#include "lcdc.h"
#include "sslfb.h"

// set main window parameters
static sslfb_plt_t	  lcdc_main =
{
	.win =
	{
		.a =
		{	
			.addr = 0x53F7E000,		// set main window framebuffer address	
			.width = 480,			// display width
			.height = 272,			// display height
		},
#ifdef CONFIG_FB_SSL_BPP32
		.pixel = 5,					// bit/pixel (5=32bpp)
#else		
		.pixel = 4,					// bit/pixel (4=16bpp)
#endif		
	},
	.panel =
	{								// these parameters refer to LCD data sheet
		.panel = LCDC_PANEL_TFT,	// panel type
		.pins = 24,					// 24-bit data line
		.hdisp_pos = 16,			// Hsync-den time = Hsync period - display period - front proch = 512 - 480 - 16
		.hdisp_len = 512,			// Hsync period
		.vdisp_pos = 4,				// Vsync-den time = Vsync period - display period - front proch = 278 - 272 - 2
		.vdisp_len = 278,			// Vsync period
		.hsync_pos = 0,
		.hsync_sub = 0,
		.hsync_len = 1,				// set 0-2
		.vsync_pos = 0,
		.vsync_len = 1,				// set 0-2
		.vsync_start = 0,
		.vsync_end = 0,
		.lshift_pos = 0,
		.lshift_len = 0,
		.hsync_high = 0,
		.vsync_high = 0,
		.lshift_rise = 1,
		.color = 1,					// set colour or mono
		.delta = 0,					// set delta or strip
	},
	.fps = 60,						// set fps
};

// set float window parameters
static sslfbw_plt_t	 lcdc_flt[] =
{
	{
		.win =
		{
			.w =
			{
				.a =
				{				
					.addr = 0x53EFC000,		// set float windows framebuffer address
					.width = 480,
					.height = 272,
				},
				.stride = 480,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,					// bit/pixel (5=32bpp)
#else						
				.pixel = 4,					// bit/pixel (4=16bpp)
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
					.addr = 0x53E7A000,
					.width = 480,
					.height = 272,
				},
				.stride = 480,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,					// bit/pixel (5=32bpp)
#else													
				.pixel = 4,					// bit/pixel (4=16bpp)
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


/*about windows framebuffer assignment*/
/*
            ------------------------------ 53FFFFFF  (64M memory)

            53FFFFFF-53F7E000>= (480*272*4)

            ----------------53F7E000 (main windows address)

            53F7E000-53EFC000 >= (480*272*4)

            ----------------53EFC000 (float windows_1 address)

            53EFC000-53E7A000 >= (480*272*4)

            ----------------53E7A000 (float windows_2 address)

            application use memory

            ------------------------------ 50000000 (base address)
*/



