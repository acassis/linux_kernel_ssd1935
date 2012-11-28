/*
 * linux/sound/arm/magus-pcm.h -- ALSA PCM interface for the SSL Magus platform
 *
 * Author:	Nicolas Pitre
 * Created:	Nov 30, 2004
 * Copyright:	MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MAGUS_PCM_H
#define _MAGUS_PCM_H

/* definitions for Magus ARM-DSP interface for audio control */
#define MAGUS_DSP_TDM_TX2_BASE_ADDR 0xD011C000
#define MAGUS_DSP_TDM_RX2_BASE_ADDR 0xD0114000

#define PIU_ALSA_CTL_QID   5 

#define MAGUS_DSP_TDM_TX 0
#define MAGUS_DSP_TDM_RX 1

#define MAGUS_TDM_CONFIG_MSG_TYPE     0x51
#define MAGUS_TDM_SYNC_MSG_TYPE       0x52
#define MAGUS_TDM_FRAME_DONE_MSG_TYPE 0x53

typedef struct  // audio control msg to DSP
{
	uint8_t		msgid;      	// message id
	uint8_t   flag;     	  // start or stop the interface
	uint8_t   interface;    // TDM Tx or Rx interface
	uint32_t  sampling_rate; // sampling rate in Hz
	uint32_t	buf_addr;	    // start address for audio frames
	uint32_t  buf_size;     // total buf size in bytes
	uint32_t  frame_size;   // period size in bytes
}magus_audio_ctl_msg_t;	    

typedef struct  // audio data process down msg from DSP to ARM
{
	uint8_t		msgid;      	// message id	    
	uint32_t	buf_position; // start address for audio frames
	uint32_t  filled_size;  // total buf size in bytes
}magus_audio_data_finish_msg_t;	 

#define PIU_INTERNAL_MEM	64
   
typedef struct 
{
	 uint32_t	type;
   uint32_t	len;
	 uint8_t		p[PIU_INTERNAL_MEM];
}piu_msg_t, *piu_msg_p;

/* platform data */
extern struct snd_soc_platform magus_soc_platform;

#endif
