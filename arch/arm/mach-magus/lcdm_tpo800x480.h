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
#ifdef CONFIG_FB_SSL_BPP32		
			.addr = 0x53E80000,		// set main window framebuffer address
#else			
			.addr = 0x53F44000,
#endif			
			.width = 800,			// display width
			.height = 480,			// display height
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
		.hdisp_pos = 216,			// Hsync-den time = Hsync period - display period - front proch = 1056 - 800 - 40
		.hdisp_len = 1056,			// Hsync period
		.vdisp_pos = 35,			// Vsync-den time = Vsync period - display period - front proch = 525 - 480 - 10
		.vdisp_len = 525,			// Vsync period
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
		.lshift_rise = 0,
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
#ifdef CONFIG_FB_SSL_BPP32
					.addr = 0x53D00000,		// set float windows framebuffer address
#else
					.addr = 0x53E88000,
#endif
					.width = 800,
					.height = 480,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,					// bit/pixel (5=32bpp)
#else						
				.pixel = 4,					// bit/pixel (4=16bpp)
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
					.addr = 0x53DCC000,
#endif
					.width = 800,
					.height = 480,
				},
				.stride = 800,
#ifdef CONFIG_FB_SSL_BPP32
				.pixel = 5,					// bit/pixel (5=32bpp)
#else													
				.pixel = 4,					// bit/pixel (4=16bpp)
#endif				
			},
			.x = 0,
			.y = 0,
		},
		.vheight = 480,
		.enable = 1,
	},
};

#endif


/*about windows framebuffer assignment*/
/*
            ------------------------------ 53FFFFFF  (64M memory)

            53FFFFFF-53E80000 >= (800*480*4)

            ----------------53E80000 (main windows address)

            53E80000-53D00000 >= (800*480*4)

            ----------------53D00000 (float windows_1 address)

            53D00000-53B80000 >= (800*480*4)

            ----------------53B80000 (float windows_2 address)

            application use memory

            ------------------------------ 50000000 (base address)
*/


/*
            ------------------------------ 53FFFFFF  (64M memory)

            53FFFFFF-53F44000 >= (800*480*2)

            ----------------53F44000 (main windows address)

            53F44000-53E88000 >= (800*480*2)

            ----------------53E88000 (float windows_1 address)

            53E88000-53DCC000 >= (800*480*2)

            ----------------53DCC000 (float windows_2 address)

            application use memory

            ------------------------------ 50000000 (base address)
*/

