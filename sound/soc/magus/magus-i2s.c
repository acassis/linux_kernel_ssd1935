/*
 * magus-i2s.c  --  ALSA Soc Audio Layer
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    12th Aug 2005   Initial version.
 *
 * JF Liu Updated for Magus Aphrodite platform, 10 Dec, 2007
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "magus-pcm.h"
#include "magus-i2s.h"

#define MAGUS_I2S_DEBUG 0

#if MAGUS_I2S_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

static int magus_i2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;

	return 0;
}

/* wait for I2S controller to be ready */
static int magus_i2s_wait(void)
{
	int i;
	
	/* check the response from DSP on BTDMP configuration message */
	for(i = 0; i < 1000; i++);

	return 0;
}

static int magus_i2s_set_dai_fmt(struct snd_soc_cpu_dai *cpu_dai,
		unsigned int fmt)
{
	/* interface format */
	/* not used in Magus platform */
	return 0;
}  

static int magus_i2s_set_dai_sysclk(struct snd_soc_cpu_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int magus_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	dbg("enter magus_i2s_hw_params func \n");
	
	return 0;
}

static int magus_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* start I2S configuration here */
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
  /* stop I2S configuration here */
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void magus_i2s_shutdown(struct snd_pcm_substream *substream)
{

	dbg("enter magus_i2s_shutdown func\n");
	return;

}

#ifdef CONFIG_PM
static int magus_i2s_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{

	return 0;
}

static int magus_i2s_resume(struct platform_device *pdev,
	struct snd_soc_cpu_dai *dai)
{

	return 0;
}

#else
#define magus_i2s_suspend	NULL
#define magus_i2s_resume	NULL
#endif
#if 0
#define MAGUS_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)
#endif

#define MAGUS_I2S_RATES (SNDRV_PCM_RATE_8000 |\		
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
		SNDRV_PCM_RATE_96000)

struct snd_soc_cpu_dai magus_i2s_dai = {
	.name = "magus-i2s",
	.id = 0,
	.type = SND_SOC_DAI_I2S,
	.suspend = magus_i2s_suspend,
	.resume = magus_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = MAGUS_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = MAGUS_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = {
		.startup = magus_i2s_startup,
		.shutdown = magus_i2s_shutdown,
		.trigger = magus_i2s_trigger,
		.hw_params = magus_i2s_hw_params,},
	.dai_ops = {
		.set_fmt = magus_i2s_set_dai_fmt,
		.set_sysclk = magus_i2s_set_dai_sysclk,
	},
};

EXPORT_SYMBOL_GPL(magus_i2s_dai);

/* Module information */
MODULE_AUTHOR("JF Liu");
MODULE_DESCRIPTION("magus I2S SoC Interface");
MODULE_LICENSE("GPL");
