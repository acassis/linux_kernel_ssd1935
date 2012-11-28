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
int		lcdc_stop(lcdc_p t);
int		lcdc_dumb_dis(lcdc_p t, int dis);

int		lcdc_set_color(lcdc_p t, int id, const uint32_t *pal, int len);

int		lcdc_effect(lcdc_p t, uint32_t effect);

//int	lcdc_smart_cfg(lcdc_p *t, lcdc_scfg_p c);

#if 0

#define LCDC_TRUE                1
#define LCDC_FALSE               0

/**
 * @Error codes
 */
#define LCDC_OK                  0 /**< Operation performed without error */
#define LCDC_ERROR              -1 /**< Error occured in the operation */
#define LCDC_ERR_PARAM          -2 /**< Error due to incorrect parameters */
#define LCDC_ERR_TIMEOUT        -3 /**< Error due to time out */
#define LCDC_ERR_NOT_SUPPORT	-4 /**< This feature is not supported */

/**
 * @brief Hardware Limit
 */
#define LCDC_CURSOR_MAX_CURSOR_NUM  2
#define LCDC_CURSOR_MAX_CURSOR_COLOR_INDEX  3  /**< Number Of Cursor Color Index */
#define LCDC_CURSOR_MAX_WIDTH   2047
#define LCDC_CURSOR_MAX_HEIGHT  2047
#define LCDC_CURSOR_BPP 2
#define LCDC_FLOAT_WINDOW_MAX_NUM 2
#define LCDC_COLOR_LUT_MAX_SIZE 256

/* Denominator of pixel clock ratio (represented as a fractional number) */
#define PIXEL_CLOCK_RATIO_DEN 1048576


/**
 * @brief LCDC orientation
 */
typedef enum
{
	SSD_LCDC_ROTATE_0_DEG = 0x0, /**< no rotation */
	SSD_LCDC_ROTATE_90_DEG = 0x1, /**< rotates clockwise 90 degree */
	SSD_LCDC_ROTATE_180_DEG = 0x2, /**< rotates clockwise 180 degree */
	SSD_LCDC_ROTATE_270_DEG = 0x3, /**< rotates clockwise 270 degree */
} ssd_lcdc_orientation_e;


/**
 * @brief LCDC Color Depth Enum
 */
typedef enum
{
	SSD_LCDC_1BPP = 0x00, /**< 1 bpp look-up-table */
	SSD_LCDC_2BPP = 0x01, /**< 2 bpp look-up-table */
	SSD_LCDC_4BPP = 0x02, /**< 4 bpp look-up-table */
	SSD_LCDC_8BPP = 0x03, /**< 8 bpp look-up-table or RGB-332 */
	SSD_LCDC_16BPP_565 = 0x04, /**< 16 bpp RGB-565 */
	SSD_LCDC_16BPP_ALPHA = 0x05, /**< 16 bpp ARGB-1555 with alpha */
	SSD_LCDC_32BPP = 0x06, /**< 32 bpp ARGG-8888 with alpha */
} ssd_lcdc_bpp_e;


/**
 * @brief This function calculates the bit size of bits_per_pixel
 * @parm[in] bpp Bits per pixel
 * @return the number of bits per pixel
 **/
int32_t ssd_lcdc_bitsizeof_bpp(ssd_lcdc_bpp_e bpp);


/**
 * @brief This macro calculates the byte size of a bits_per_pixel_type
 */
#define LCDC_VIRTUAL_WIDTH(width, bpp) \
	((width) + 1) * ssd_lcdc_bitsizeof_bpp(bpp) / 8


/**
 * @brief LCDC Color Structure
 */
typedef struct
{
/* little endian */
	uint8_t b; /**< blue value */
	uint8_t g; /**< green value */
	uint8_t r; /**< red value */
	uint8_t a; /**< alpha value */
} ssd_lcdc_color_t;


/**
 * @brief LCDC Post Processing Structure
 */
typedef struct
{
	uint8_t contrast; /**< Contrast value from 0x00 to 0x7f */
	uint8_t brightness; /**< Brightness value from 0x00 to 0xff */
	uint8_t satuation; /**< Satuation value from 0x00 to 0x7f */
} ssd_lcdc_ppctrl_t;


/**
 * @brief This function controls Post Processing Control Register
 * @param[in] ppctrl Post processing control parameters (0 means disable pp)
 * @return 0 if register set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_set_post_processing_control(
	const ssd_lcdc_ppctrl_t *ppctrl);


/**
 * @brief This function controls Post Processing Control Register
 * @param[out] ppctrl Post processing control parameters
 * @return 0 if all contrast, brightness and satuation values are read
 *         successfully, or -1 otherwise
 */
int32_t ssd_lcdc_get_post_processing_control(ssd_lcdc_ppctrl_t *ppctrl);


/**
 * @brief LCDC scaler enum
 *
 * Scaling will not affect timing. It will only affect the region of interest (ROI)
 * read from the framebuffer. For example, if the main window is set to 640x480
 * with a scaler of 2x, then it will only read the 320x240 window from the 
 * framebuffer.
 */
typedef enum
{
	SSD_LCDC_SCALER_8X = 0x3,  /* scale up 8x both vertically and horizontally */
	SSD_LCDC_SCALER_4X = 0x2,  /* scale up 4x both vertically and horizontally */
	SSD_LCDC_SCALER_2X = 0x1,  /* scale up 2x both vertically and horizontally */
	SSD_LCDC_SCALER_1X = 0x0,  /* scaler is turned off */
} 
ssd_lcdc_scaler_e;


/**
 * @brief Dithering contorl structure
 */
typedef struct
{
	uint8_t dither_enable;  /**< For CSTN only **/
	uint8_t dynamic_dither;  /* For both CSTN & TFT */
} ssd_lcdc_dither_t;


/**
 * @brief Data swap setting is dervied from bpp
 *
 * The memory bus is 64-bit and data is stored as little endian type.
 * However the LCDC is big endian internally. Thus data swap is required.
 *   32bpp: swap32 = 1, swap16 = 0, swap8 = 0
 *   16bpp: swap32 = 1, swap16 = 1, swap8 = 0
 *    8bpp: swap32 = 1, swap16 = 1, swap8 = 1
 */
/* Need to swap for all <= 32bpp */
#define SWAP32(bpp) 1
/* Need to swap for all <= 16bpp */
#define SWAP16(bpp) (\
	  (bpp == SSD_LCDC_16BPP_565 \
	|| bpp == SSD_LCDC_16BPP_ALPHA \
	|| bpp == SSD_LCDC_8BPP \
	|| bpp == SSD_LCDC_4BPP \
	|| bpp == SSD_LCDC_2BPP \
	|| bpp == SSD_LCDC_1BPP) ? 1 : 0)
/* Need to swap for all <= 8bpp */
#define SWAP8(bpp) (\
	  (bpp == SSD_LCDC_8BPP \
	|| bpp == SSD_LCDC_4BPP \
	|| bpp == SSD_LCDC_2BPP \
	|| bpp == SSD_LCDC_1BPP) ? 1 : 0)


/**
 * @brief LCDC data swap 
 */
typedef enum
{
	SSD_LCDC_SWAP32 = 0x1,
	SSD_LCDC_SWAP16 = 0x2,
	SSD_LCDC_SWAP8 = 0x4 
}
ssd_lcdc_swap_bit_e;


/**
 * @brief LCDC special effects structure
 */
typedef struct
{
/* Special Effects Register */
	ssd_lcdc_scaler_e scaler;  /**< scaler value. Refers to ssd_lcdc_scaler_e */
	ssd_lcdc_swap_bit_e swap;
	uint8_t mirror;	/**< horizontal mirror */
	ssd_lcdc_orientation_e orientation;  /**< rotation mode */

/* Display Configuration Register */
	ssd_lcdc_dither_t dither;	/**< dithering mode */
	uint8_t color_invert;	/**< color invert */
} ssd_lcdc_effect_t;


/*
 * @brief This function controls the Special Effect Register & part of Display Configuration Register
 * @param[in] effect Special effect control paramters
 * @return 0 if register set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_set_effect(ssd_lcdc_effect_t *effect);


/**
 * @brief This function controls Special Effect Control Register & part of Display Configuration Register
 * @param[out] effect Special effect control parameters
 * @return 0 if all scalar, swap, mirror, rotate, values are read
 *         successfully, or -1 otherwise
 */
int32_t ssd_lcdc_get_effect(ssd_lcdc_effect_t *effect);


/**
 * @brief LCDC Frame Rate Control structure
 */
typedef struct
{
/* Frame Rate Control Seed Register[1:0] */
	uint8_t cseed[16];  /**< CSTN seed value from 0x0 to 0xF */
/* TFT Frame Rate Control Register */
	uint8_t tseed_botton_right;  /**< TFT seed value (bottom right) from 0x0 to 0x3 */
	uint8_t tseed_bottom_left;  /**< TFT seed value (bottom left) from 0x0 to 0x3 */
	uint8_t tseed_top_right;  /**< TFT seed value (top right) from 0x0 to 0x3 */
	uint8_t tseed_top_left;  /**< TFT seed value (top left) from 0x0 to 0x3 */
/* Display Configuration Register */
	uint8_t frc_seed_rotate_enable;
	uint8_t frc_period_select;
} ssd_lcdc_frc_t;


/*
 * @brief This function turns on frame rate control algorithm. 
 *
 * This is set through Frame Rate Control Seed Register[1:0] for CSTN panel, 
 * or TFT Frame Rate Control Register for TFT panels
 *
 * @param[in] frc Frame rate control paramters
 * @return 0 if frc is turned on successfully, or -1 otherwise
 */
int32_t ssd_lcdc_frc_enable(ssd_lcdc_frc_t *frc);


/*
 * @brief This function turns off frame rate control algorithm.
 * @return 0 if frc is turned off successfully, or -1 otherwise
 */
int32_t ssd_lcdc_frc_disable(void);



/*
 * @brief DMA Burst Length 
 */
typedef enum
{
	INCR = 0x0,      /**< Burst length = 1 word */
	INCR4_FIX = 0x2, /**< Burst length = 4 word, without unspecified length burst*/
	INCR4_VAR = 0x3, /**< Burst length = 4 word, with unspecified length burst*/
	INCR8_FIX = 0x4, /**< Burst length = 8 word, without unspecified length burst*/
	INCR8_VAR = 0x5, /**< Burst length = 8 word, with unspecified length burst*/
	INCR16_FIX = 0x6,/**< Burst length = 16 word, without unspecified length burst*/
	INCR16_VAR = 0x7,/**< Burst length = 16 word, with unspecified length burst*/
} ssd_lcdc_dma_burst_e;


/*
 * @brief This function sets the dma burst length 
 * @param[in] dma_burst DMA Burst length
 * @return 0 if register is written successfully, or -1 otherwise
 */
int32_t ssd_lcdc_set_dma_burst(ssd_lcdc_dma_burst_e dma_burst);


/*
 * @brief This function gets the dma burst length 
 * @param[out] dma_burst DMA Burst length
 * @return 0 if register is read successfully, or -1 otherwise
 */
int32_t ssd_lcdc_get_dma_burst(ssd_lcdc_dma_burst_e *dma_burst);


/*
 * @brief Interrupt type definition
 */
typedef enum
{
	SSD_LCDC_IRQ_UNDERRUN = 0x8,  /**< FIFO underrun irq enable */
	SSD_LCDC_IRQ_DONE = 0x4,      /**< Smart panel Transfer done irq enable */
	SSD_LCDC_IRQ_DMA_ERROR = 0x2, /**< DMA error irq enable */
	SSD_LCDC_IRQ_EOF = 0x1,       /**< End Of Frame irq enable */
}
ssd_lcdc_irq_e;


/*
 * @brief This function set interrupt configuration for LCDC
 * @param[in] irq Interrupt type (OR-ed) 
 * @return 0 if register is set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_irq_enable(uint32_t irq);


/*
 * @brief This function gets interrupt configuration for LCDC
 * @param[in] irq Interrupt type (OR-ed)
 * @return 0 if register is read successfully, or -1 otherwise
 */
int32_t ssd_lcdc_irq_disable(uint32_t irq);


/*
 * @brief This function clears interrupt status for LCDC
 * @param[in] irq Interrupt type (OR-ed)
 * @return 0 if register is set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_irq_clear(uint32_t irq);


/*
 * @brief This function reads the interrupt status of LCDC
 * @return the interrupt status (OR-ed) of LCDC 
 */
uint32_t ssd_lcdc_irq_status(void);


/**
 * @brief This function turn on dump display
 * @return 0 if dump display is turned on successfully, or -1 otherwise
 */
int32_t ssd_lcdc_dumb_display_on(void);


/**
 * @brief This function turn off dump display
 * @return 0 if dump display is turned off successfully, or -1 otherwise
 */
int32_t ssd_lcdc_dumb_display_off(void);


/**
 * @brief This function enables LCDC module
 * @return 0 if successful, or -1 otherwise
 */
int32_t ssd_lcdc_enable(void);


/**
 * @brief This function disables LCDC module
 * @return 0 if successful, or -1 otherwise
 */
int32_t ssd_lcdc_disable(void);


/**
 * @brief This function detects LCDC module
 * @return 0 if successful, or -1 otherwise
 */
int32_t ssd_lcdc_detect(void);


/**
 * @brief This function resets LCDC module
 * @return 0 if successful, or -1 otherwise
 */
int32_t ssd_lcdc_reset(void);


/**
 * @brief This function aborts current data transmission
 * @return 0 if successful, or -1 otherwise
 */
int32_t ssd_lcdc_abort(void);


/**
 * @defgroup Main Window Functions 
 */

/**
 * @brief LCDC Main Window Configuration Structure
 */
typedef struct
{
	uint16_t x; /**< x offset (software implementation) */
	uint16_t y; /**< y offset (software implementation) */

/* Main Window Display Size Register */
	uint16_t width;         /**< Display width value (pixels) (0-2047) representing (1-2048) */
	uint16_t height;        /**< Display height value (pixels) (0-2047) representing (1-2048) */
/* Main Window Virtual Width Register */
	uint16_t virtual_width; /**< Virtual width value (bytes) (0-65535) */
/* Main Window Display Start Address Register */
	uint32_t mem_start;      /**< memory buffer start addr (physical) */
/* Display Configuration Register */
	ssd_lcdc_bpp_e bpp;  /**< bits per pixel (enum) */
} ssd_lcdc_main_window_t;


/**
 * @brief Floating Window structure
 */
typedef struct
{
	ssd_lcdc_main_window_t	win;
	ssd_lcdc_alpha_t alpha;  /**< Alpha configuration */
}
ssd_lcdc_float_window_t;


/**
 * @brief This function sets the basic parameters for Main Window. 
 * 
 * This is set through Main Window Virtual Width Register, 
 * Main Window Display Size Register, and Pixel Clock Frequency Ratio Register
 *
 * @param[in] main_window Main window control paramters
 * @return 0 if register set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_set_main_window(ssd_lcdc_main_window_t *main_window);


/**
 * @brief This function gets the basic parameters for Main Window. 
 *
 * This is get from Main Window Virtual Width Register, Main Window Display Size 
 * Register, and Pixel Clock Frequency Ratio Register
 *
 * @param[out] main_window Main window control paramters
 * @return 0 if register set successfully, or -1 otherwise
 */
int32_t ssd_lcdc_get_main_window(ssd_lcdc_main_window_t *main_window);


/**
 * @brief This function sets the refresh rate for whole screen.
 * @param[in] fps Refresh rate (in frame per second)
 * @return the actual frame rate being set, or -1 otherwise
 */
int32_t ssd_lcdc_set_refresh_rate(int32_t fps);


/**
 * @brief This function gets the refresh rate for whole screen.
 * @return the actual frame rate being set, or -1 otherwise
 */
int32_t ssd_lcdc_get_refresh_rate(void);


/**
 * @defgroup Panel Timing Functions
 */


/**
 * @brief TFT Serial Panel RGB Sequence Enum
 */
typedef enum
{
	SSD_LCDC_RGB_SEQ_RGB = 0x0, /**< Sequence as R,G,B */
	SSD_LCDC_RGB_SEQ_RBG = 0x1, /**< Sequence as R,B,G */
	SSD_LCDC_RGB_SEQ_GRB = 0x2, /**< Sequence as G,R,B */
	SSD_LCDC_RGB_SEQ_GBR = 0x3, /**< Sequence as G,B,R */
	SSD_LCDC_RGB_SEQ_BRG = 0x4, /**< Sequence as B,R,G */
	SSD_LCDC_RGB_SEQ_BGR = 0x5, /**< Sequence as B,G,R */
} ssd_lcdc_rgb_seq_e;


/**
 * @brief Panel Data Width
 */
typedef enum
{
	SSD_LCDC_CSTN_WIDTH_4_BIT = 0x0,  /**< CSTN panel 4 bit data width */
	SSD_LCDC_CSTN_WIDTH_8_BIT = 0x1,  /**< CSTN panel 8 bit data width */
	SSD_LCDC_CSTN_WIDTH_16_BIT = 0x2, /**< CSTN panel 16 bit data width */
	SSD_LCDC_TFT_WIDTH_9_BIT = 0x0,   /**< TFT panel 9 bit data width */
	SSD_LCDC_TFT_WIDTH_12_BIT = 0x1,  /**< TFT panel 12 bit data width */
	SSD_LCDC_TFT_WIDTH_18_BIT = 0x2,  /**< TFT panel 18 bit data width */
	SSD_LCDC_TFT_WIDTH_24_BIT = 0x3,  /**< TFT panel 24 bit data width */
} ssd_lcdc_panel_width_e;


typedef enum
{
	SSD_LCDC_PANEL_TYPE_DUMB_STN = 0x0,   /**< dumb STN */
	SSD_LCDC_PANEL_TYPE_DUMB_TFT = 0x1,   /**< dumb TFT */
	SSD_LCDC_PANEL_TYPE_SERIAL_TFT = 0x2, /**< dumb serial TFT */
	SSD_LCDC_PANEL_TYPE_MINI_RGB = 0x3,   /**< mini RGB */
	SSD_LCDC_PANEL_TYPE_SMART = 0x4,      /**< smart panel */
} ssd_lcdc_panel_type_e;


/**
 * @brief Panel Type Parameter Structure 
 */
typedef struct
{
	uint8_t name[64];  /**< Panel name (for ref only. Not to be used by hardware) */

/* Panel Type Register */
	ssd_lcdc_rgb_seq_e seq_even;      /**< rgb sequence for serial TFT panel */
	ssd_lcdc_rgb_seq_e seq_odd;       /**< rgb sequence for serial TFT panel */
	int32_t cstn_panel_data_format; /**< CSTN type 0:strip type, 1:delta type*/
	uint8_t color_panel_select;     /**< color panel select, 0:mono, 1:color */
	ssd_lcdc_panel_width_e panel_data_width;  /**< panel data bus width */
	uint8_t csid;  /**< chip select id value from 0 - 1 */
	ssd_lcdc_panel_type_e panel_type;  /**< panel type */
} ssd_lcdc_panel_type_t;


/**
 * @brief This function sets the panel type
 * @parm[in] type Panel type configuration paramters
 * @return 0 if registers are set successfully, otherwise -1 
 */
int32_t ssd_lcdc_set_panel_type(ssd_lcdc_panel_type_t *type);


/**
 * @brief This function gets the panel type
 * @parm[out] type Panel type configuration paramters
 * @return 0 if registers are set successfully, otherwise -1 
 */
int32_t ssd_lcdc_get_panel_type(ssd_lcdc_panel_type_t *type);


/**
 * @brief Panel Timing Configuration Structure
 */
typedef struct
{
/* MOD -- TODO */

/* Horizontal Display Configuration Register */
	uint16_t htotal; /**< Horizontal total, value 0 - 2047 */
	uint16_t hstart; /**< Horizontal display period start, value 0 - 2047 */

/* Veritcal Display Configuration Register*/
	uint16_t vtotal; /**< Vertical total, value 0 - 2047 */
	uint16_t vstart; /**< Vertical display period start, value 0 - 2047 */

/* Line Pulse Configuration Register */
	uint16_t lp_width; /**< Line pulse width, value of hsync_width - 1 */
	uint16_t lp_sub_pixel_pos; /**< Line pulse sub-pixel position (delay), value 0 - 3 */
	uint16_t lp_start_pos;

/* Frame Pulse Configruation Register */
	uint16_t fp_width;     /**< frame pulse width value of vsync_width - 1 */
	uint16_t fp_start_pos; /**< frame pulse start position */
/* Frame Pulse Offset Register */
	uint16_t fp_stop_offset;  /**< frame pulse stop offset value 0 - 2047 */
	uint16_t fp_start_offset; /**< frame pulse start offset value 0 - 2047 */

/* LSHIFT Signal Position Register */
	uint16_t lshift_stop_pos;  /**< LSHIFT signal stop position value from 0 - 2047 pixel clocks */
	uint16_t lshift_start_pos; /**< LSHIFT signal start position value from 0 - 2047 pixel clocks */

/* Display Configuration Register */
	uint8_t lp_pol;     /**< Line pulse polarity, 0:active low, 1:active high*/
	uint8_t fp_pol;     /**< Frame pulse polarity, 0:active low, 1:active high */
	uint8_t lshift_pol; /**< LSHIFT polarity, 0:rising edge trigger, 1:falling edge trigger */

} ssd_lcdc_timing_t;


/**
 * @brief This function sets the timing for dump panel
 * @parm[in] timing Timing configuration paramters
 * @return 0 if registers are set successfully, otherwise -1 
 */
int32_t ssd_lcdc_set_timing(ssd_lcdc_timing_t *timing);


/**
 * @brief This function gets the timing for dump panel
 * @parm[in] timing Timing configuration paramters
 * @return 0 if registers are set successfully, otherwise -1 
 */
int32_t ssd_lcdc_get_timing(ssd_lcdc_timing_t *timing);


/**
 * @defgroup Cursors Functions
 */


/**
 * @brief Cursor blinking structure
 */
typedef struct
{
/* Curosr Blink Configuration Register */
	/** Duration of blinking period (blink-on + blink-off time)
	 *  (0-1023 of unknown unit) */
	uint16_t blink_on_and_off_duration;
	/** Duration of blinking-on time (0-1023 of unknown unit) */
	uint16_t blink_on_duration;
} ssd_lcdc_cursor_blink_t;


/**
 * @brief Cursor structure
 */
typedef struct
{
/* Cursor XY Offsets Register */
	uint16_t x; /**< X-coordinate of cursor position offset (0-2047) */
	uint16_t y; /**< Y-coordinate of cursor position offset (0-2047) */
/* Cursor Size Register */
	uint16_t width; /**< Width of cursor size
	                 *   (0-2047 representing size of 1-2048 pixel) */
	uint16_t height; /**< Height of cursor size
	                  *   (0-2047 representing size of 1-2048 pixel) */
//	ssd_lcdc_orientation_e orient; /**< cursor orientation */
/* Curosr Blink Configuration Register */
	ssd_lcdc_cursor_blink_t blink;
	uint8_t is_blink; /**< Whether cursor is blinking */
	uint8_t is_hidden; /**< Whether cursor is hidden */
/* Cursor Memory Start Address Register */
	uint32_t mem_start;  /**< 2bpp bitmap for cursor */
/* Cursor Color Index Register[1,2,3] */
        ssd_lcdc_color_t color_lut[LCDC_COLOR_LUT_MAX_SIZE];  /**< Index of color index register (0,1,2) */
	uint8_t color_lut_size;
} ssd_lcdc_cursor_t;


/**
 * @brief This function enables a specific cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] cursor Parameters for enabling a cursor.
 *            If this is NULL, cursor will be enabled without initializing
 * @return 0 if specific cursor is enabled, or -1 otherwise
 */
int32_t ssd_cursor_enable(
	uint8_t cursor_index, ssd_lcdc_cursor_t *cursor);

/**
 * @brief This function disables a specific cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @return 0 if specific cursor is disabled, or -1 otherwise
 */
int32_t ssd_cursor_disable(uint8_t cursor_index);


/**
 * @brief This function changes blinking period of specific cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] blink Blinking parameters
 * @return 0 if blinking period is changed successfully, or -1 otherwise
 */
int32_t ssd_cursor_set_blinking_period(
	uint8_t cursor_index, const ssd_lcdc_cursor_blink_t *blink);

/**
 * @brief This function retrieves blinking period of specific cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[out] blink Blinking parameters
 * @return 0 if blinking period is retrieved successfully, or -1 otherwise
 */
int32_t ssd_cursor_get_blinking_period(
	uint8_t cursor_index, ssd_lcdc_cursor_blink_t *blink);


/**
 * @brief This function blinks cursor
 *
 * This function will show cursor automatically if it was hidden.
 *
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] blink Optional parameters for blinking.
 *            If this is NULL, it will use last blinking parameters.
 * @param[in] cursor Cursor top level structure
 *            If new blink is not given, then this blink will be used 
 * @return 0 if cursor blinks successfully, or -1 otherwise
 */
int32_t ssd_cursor_blink(
	uint8_t cursor_index, ssd_lcdc_cursor_blink_t *blink, 
	ssd_lcdc_cursor_t *cursor);


/**
 * @brief This function blinks cursor
 *
 * This function will show cursor automatically if it was hidden.
 *
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] cursor Curosr top level structure
 * @return 0 if cursor un-blinks successfully, or -1 otherwise
 */
int32_t ssd_cursor_unblink(uint8_t cursor_index, ssd_lcdc_cursor_t *cursor);


/**
 * @brief This function shows cursor
 *
 * Cursor will remain blinking if it was blinking before hidden
 *
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] cursor Cursor top level structure
 * @return 0 if it shows successfully, or -1 otherwise
 */
int32_t ssd_cursor_show(uint8_t cursor_index,  ssd_lcdc_cursor_t *cursor);


/**
 * @brief This function hides cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] cursor Cursor top level structure
 * @return 0 if it hides successfully, or -1 otherwise
 */
int32_t ssd_cursor_hide(uint8_t cursor_index,  ssd_lcdc_cursor_t *cursor);


/**
 * @brief This function changes cursor position offset
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] x X-coordinate of cursor position offset (0-2047)
 * @param[in] y Y-coordinate of cursor position offset (0-2047)
 * @return 0 if offset is changed successfully, or -1 otherwise
 */
int32_t ssd_cursor_set_xy(uint8_t cursor_index, uint16_t x, uint16_t y);


/**
 * @brief This function changes cursor image
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] mem_start Cursor image, must be 2bpp, pixel value of 0 is transparent
 * @param[in] width Width of cursor size
 *            (0-2047 representing size of 1-2048 pixel)
 * @param[in] height Height of cursor size
 *            (0-2047 representing size of 1-2048 pixel)
 * @return 0 if image is changed successfully, or -1 otherwise
 */
int32_t ssd_cursor_set_image(
	uint8_t cursor_index, uint32_t mem_start, uint16_t width, uint16_t height);


/**
 * @brief This function clears cursor image
 * @param[in] cursor_index Cursor identification number (0-1)
 * @return 0 if image is cleared successfully, or -1 otherwise
 */
int32_t ssd_cursor_clear_image(uint8_t cursor_index);


#if 0
/**
 * @brief This function changes cursor orientation
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] orient Orientation
 * @return 0 if orientation is changed successfully, or -1 otherwise
 */
int32_t ssd_cursor_set_orientation(
	uint8_t cursor_index, ssd_lcdc_orientation_e orient);
#endif


/**
 * @brief This function set color of specified color index of a cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] color_index Color index specified
 * @param[in] color Color value for specified color index
 * @return 0 if color is set successfully, or -1 otherwise
 */
int32_t ssd_cursor_set_color(
	uint8_t cursor_index,
	uint8_t color_index, const ssd_lcdc_color_t *color);


/**
 * @brief This function sets the Map of Color Look-up Table of a Cursor
 *
 * There are 2 levels of lut for cursor. The first level is formed by
 * Color Index Register [3,2,1], which is called "map" here. Each entry
 * of this map is pointed to the second level of lut, which is the one
 * and only one lut. That lut can be shared by Main Window, Float Window,
 * and Cursor.
 *
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[in] color_lut Color look up table map 
 * @param[in] color_lut_size Size of color look up table array
 * @return 0 if orientation is changed successfully, or -1 otherwise
 *
 */
int32_t ssd_cursor_set_color_map(
	uint8_t cursor_index,
	ssd_lcdc_color_t *color_lut, uint8_t color_lut_size);


/**
 * @brief This function gets the Map of Color Look-up Table of a Cursor
 * @param[in] cursor_index Cursor identification number (0-1)
 * @param[out] color_lut Buffer for retrieving color look up table map 
 * @param[in] color_lut_size Size of color look up table array buffer
 * @return Size of color look up table array retrieved, or -1 when error occures
 */
int32_t ssd_cursor_get_color_map(
	uint8_t cursor_index,
	ssd_lcdc_color_t *color_lut, uint8_t color_lut_size);


/**
 * @defgroup Floating Window Functions
 */


/**
 * @brief Alpha Blending type for Floating Window
 */
typedef enum
{
	SSD_LCDC_ALPHA_OFF = 0x0,
	SSD_LCDC_ALPHA_LOCAL = 0x2,
	SSD_LCDC_ALPHA_GLOBAL = 0x3,
} ssd_lcdc_alpha_e;


/**
 * @brief Alpha Blending Configuration structure 
 */
typedef struct
{
	uint8_t alpha_value;  /**< Alpha value (0 - 255) */
	ssd_lcdc_alpha_e alpha_type;  /**< Alpha blending type */
} ssd_lcdc_alpha_t;


/**
 * @brief This function sets up the Floating Window
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @return 0 when succesfully initialized the window, otherwise -1
 */
int32_t ssd_lcdc_set_float_window(
	uint8_t window_index, ssd_lcdc_float_window_t* float_window);


/**
 * @brief This function gets the setting of Floating Window
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @return 0 when succesfully initialized the window, otherwise -1
 */
int32_t ssd_lcdc_get_float_window(
	uint8_t window_index, ssd_lcdc_float_window_t* float_window);


/**
 * @brief This function enables the Floating Window 
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @return 0 when succesfully enabled the window, otherwise -1
 */
int32_t ssd_lcdc_float_window_enable(uint8_t window_index);


/**
 * @brief This function disables the Floating Window 
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @return 0 when succesfully disabled the window, otherwise -1
 */
int32_t ssd_lcdc_float_window_disable(uint8_t window_index);


/**
 * @brief This function sets the XY Offset of the Floating Window 
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @param[in] x Offset X
 * @param[in] y Offset Y
 * @return 0 when the XY Offset is succesfully set, otherwise -1
 */
int32_t ssd_lcdc_float_window_set_xy(uint8_t window_index, uint16_t x, uint16_t y);


/**
 * @brief This function gets the XY Offset of the Floating Window 
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @param[in] x Offset X
 * @param[in] y Offset Y
 * @return 0 when the XY Offset is succesfully get, otherwise -1
 */
int32_t ssd_lcdc_float_window_get_xy(uint8_t window_index, uint16_t *x, uint16_t *y);


/**
 * @brief This function sets the Alpha configuration of the Floating Window 
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @param[in] alpha Alpha configuration structure
 * @return 0 when Alpha value is succesfully set, otherwise -1
 */
int32_t ssd_lcdc_float_window_set_alpha(
	uint8_t window_index, const ssd_lcdc_alpha_t *alpha);


/**
 * @brief This function gets the Alpha configuration of the Floating Window
 * @param[in] window_index Floating Window Identification Number (0-1)
 * @param[in] alpha Alpha configuration structure
 * @return 0 when Alpha value is succesfully get, otherwise -1
 */
int32_t ssd_lcdc_float_window_get_alpha(
	uint8_t window_index, ssd_lcdc_alpha_t *alpha);


/**
 * @brief This function sets the Color Look-up Table array
 * @param[in] color_lut Color look up table array
 * @param[in] color_lut_offset Index of the first palette to set the color lut
 * @param[in] color_lut_size Size of color look up table array
 * @return LCDC_OK if orientation is changed successfully, or error otherwise
 */
int32_t ssd_lcdc_set_color_lut(
	const ssd_lcdc_color_t *color_lut,
	uint8_t color_lut_offset, uint16_t color_lut_size);

/**
 * @brief This function sets one entry in the Color Look-up Table
 * @param[in] color RGB color
 * @param[in] palette_index Palette index of color
 * @return LCDC_OK if orientation is changed successfully, or error otherwise
 */
int32_t ssd_lcdc_set_color(
	const ssd_lcdc_color_t *color, uint8_t palette_index);

/**
 * @brief This function gets the Color Look-up Table array
 * @param[out] color_lut Buffer for retrieving color look up table array
 * @param[in] color_lut_size Size of color look up table array buffer
 * @return Size of color look up table array retrieved, or -1 when error occures
 */
int32_t ssd_lcdc_get_color_lut(
	ssd_lcdc_color_t *color_lut, uint8_t color_lut_size);


/** XXX
 * @TODO
 */
int32_t ssd_lcdc_smart_set_burst_mode(uint8_t enable_burst);


/**
 * @defgroup Miscelleanous Functions
 */


/**
 * @brief LCDC version structure
 */
struct ssd_lcdc_version
{
	uint16_t ident_num;
	uint8_t  design_num;
	uint8_t  major_rev;
	uint8_t  minor_rev;
};

/**
 * @brief This function retrieves identification numbers from
 *        LCDC Device Identification Register
 * @param[out] ssd_lcdc_version Version structure retrieved
 * @return TRUE if gets revision number successfully, or FALSE otherwise
 */
int32_t ssd_lcdc_get_identification_numbers(
	struct ssd_lcdc_version *lcdc_version);


/**
 * @brief Capabilities of this LCD controller
 */
typedef struct
{
	uint8_t read_back_support;
	uint8_t pp_support;
	uint8_t lut_support;
	uint8_t stn_support;
	uint8_t dma_capable;
} ssd_lcdc_cap_t;


/**
 * @brief This function checks the capabilities of LCDC
 * @param[out] cap Capabilities of this LCD controller
 * @return 0 if success, -1 otherwise
 */
int32_t ssd_lcdc_get_cap(ssd_lcdc_cap_t *cap);


/**
 * @brief This functions generates the default configuration for LCDC
 * @param[in] lcdc Top level LCDC object
 * @return 0 when a default config is generated, -1 otherwise
 *
 * Usually this should be called after ssd_panel_default_config().
 */
int32_t ssd_lcdc_default_config(ssd_lcdc_t *lcdc);


/**
 * @brief This function sets up the basic parameter of floating window based on main window
 * @param[in] float_window Float window data structure
 * @return 0 if the setting is successfully done, -1 otherwise
 **/
int32_t ssd_lcdc_float_window_default_config(
	uint8_t window_index, ssd_lcdc_float_window_t *float_window);


#endif
#endif
