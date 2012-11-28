/*
 * wm8731.h  --  WM8731 Soc Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on wm8753.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8731_H
#define _WM8731_H

#include <sound/soc.h>
#include <sound/soc-dapm.h>

/* WM8731 register space */

#define WM8731_LINVOL   0x00
#define WM8731_RINVOL   0x01
#define WM8731_LOUT1V   0x02
#define WM8731_ROUT1V   0x03
#define WM8731_APANA    0x04
#define WM8731_APDIGI   0x05
#define WM8731_PWR      0x06
#define WM8731_IFACE    0x07
#define WM8731_SRATE    0x08
#define WM8731_ACTIVE   0x09
#define WM8731_RESET	0x0f

#define WM8731_CACHEREGNUM 	10

#define WM8731_SYSCLK	0
#define WM8731_DAI		0

#define WM8731_INIT_CMD              10
#define WM8731_POWER_DOWN_SET_CMD    20
#define WM8731_SAMPLE_RATE_SET_CMD   30
#define WM8731_OUTPUT_VOLUME_SET_CMD 40
#define WM8731_INPUT_VOLUME_SET_CMD  50
#define WM8731_SHUTDOWN_CMD          60
#define WM8731_REGISTER_RESTORE_CMD  70

#define WM8731_GENERIC_REGISTER_SET_CMD   80
#define WM8731_GENERIC_REGISTER_GET_CMD   90


/* Audio control related message Id */
#define PIUMSG_AUDIOCTL_TX	0x13
#define PIUMSG_AUDIOCTL_RX	0x14

#define PIU_AUDIO_CTL_QID      2

#define PIU_CMD	1
#define PIU_REP	2
/* Sample Rates */
typedef enum
{
	SAMPLE_RATE_ADC_96_DAC_96		    = 0x07,
	SAMPLE_RATE_ADC_88_2_DAC_88_2		= 0x0F,
	SAMPLE_RATE_ADC_48_DAC_48		    = 0x00,
	SAMPLE_RATE_ADC_44_1_DAC_44_1		= 0x08,
	SAMPLE_RATE_ADC_32_DAC_32		    = 0x06,
	SAMPLE_RATE_ADC_8_021_DAC_8_021	= 0x0B,
	SAMPLE_RATE_ADC_8_DAC_8		      = 0x03
} SAMPLE_RATES;

struct acmsg_t
{
  unsigned char		msgid;	// msg id
  unsigned int  	data;	// one 32 bits data for sending
  unsigned int	  buf_addr;	// data buf address	
  unsigned int    len;	// data length
};

struct acmsg_rep_t
{
 unsigned char		msgid;	// msg id
 unsigned int		ret;	// return
};

struct wm8731_setup_data {
	unsigned short i2c_address;
};
extern struct snd_soc_codec_dai wm8731_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8731;
extern struct snd_soc_device *g_socdev;

unsigned int wm8731_read_reg_cache(struct snd_soc_codec *codec,
	                                 unsigned int reg);
int wm8731_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value);


#endif
