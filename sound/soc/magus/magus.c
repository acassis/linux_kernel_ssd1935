/*
 * magus.c  --  SoC audio for Magus Aphrodite
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2005 Openedhand Ltd.
 *
 * Authors: Liam Girdwood <liam.girdwood@wolfsonmicro.com>
 *          Richard Purdie <richard@openedhand.com>
 *
 *
 * JF Liu updated for Magus Aphrodite platform, 10 Dec, 2007
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "wm8731.h"
#include "magus-pcm.h"
#include "magus-i2s.h"


#define MAGUS_DEBUG 0

#if MAGUS_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

static int magus_startup(struct snd_pcm_substream *substream)
{
	dbg("enter magus_startup func \n");
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	dbg("exit magus_startup func \n");

	return 0;
}

static int magus_shutdown(struct snd_pcm_substream *substream)
{
	dbg("enter magus_shutdown func \n");

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->codec;

	dbg("exit magus_shutdown func\n");

	return 0;
}

static int magus_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int clk = 0;
	int ret = 0;
  dbg("enter magus_hw_params func\n");
	switch (params_rate(params)) {
	case 8000:
	//case 16000:
  case 32000:
	case 48000:
	case 96000:
		clk = 12000000;
		break;
	//case 11025:
	//case 22050:
	case 44100:
	case 88200: //Jianfeng added
		clk = 12000000;
		break;
	}

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration (unused) */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFM);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8731_SYSCLK, clk,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, MAGUS_I2S_SYSCLK, 0,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

  dbg("exit magus_hw_params func ok \n");
	return 0;
}

static struct snd_soc_ops magus_ops = {
	.startup = magus_startup,
	.hw_params = magus_hw_params,
	.shutdown = magus_shutdown,
};

/*
 * Logic for a wm8731 as connected on a ARM Device
 */
static int magus_wm8731_init(struct snd_soc_codec *codec)
{
	int i, err;
  dbg("enter magus_wm8731_init func\n");
#if 0
	snd_soc_dapm_set_endpoint(codec, "LLINEIN", 0);
	snd_soc_dapm_set_endpoint(codec, "RLINEIN", 0);
	snd_soc_dapm_set_endpoint(codec, "MICIN", 1);

	/* Add magus specific controls here, if any */
	snd_soc_dapm_set_endpoint(codec, "DAC", 1);
	
	/* sync the registers */
	snd_soc_dapm_sync_endpoints(codec);
#endif	
	dbg("exit magus_wm8731_init func\n");
	return 0;
}

/* magus digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link magus_dai = {
	.name = "WM8731",
	.stream_name = "WM8731",
	.cpu_dai = &magus_i2s_dai,
	.codec_dai = &wm8731_dai,
	.init = magus_wm8731_init,
	.ops = &magus_ops,
};

/* magus audio machine driver */
static struct snd_soc_machine snd_soc_machine_magus = {
	.name = "magus_audio",
	.dai_link = &magus_dai,
	.num_links = 1,
};

/* magus audio private data */
static struct wm8731_setup_data magus_wm8731_setup = {
	.i2c_address = 0x1b,
};

/* magus audio subsystem */
static struct snd_soc_device magus_snd_devdata = {
	.machine = &snd_soc_machine_magus,
	.platform = &magus_soc_platform,
	.codec_dev = &soc_codec_dev_wm8731,
	.codec_data = &magus_wm8731_setup,
};

static struct platform_device *magus_snd_device;
  
static int __init magus_init(void)
{
	int ret;
  dbg("enter magus_init func\n");
  /* add magus to device */
	magus_snd_device = platform_device_alloc("soc-audio", -1);
	if (!magus_snd_device)
		return -ENOMEM;
  
	platform_set_drvdata(magus_snd_device, &magus_snd_devdata);
	magus_snd_devdata.dev = &magus_snd_device->dev;

	ret = platform_device_add(magus_snd_device);
  if(!ret)
  {
      dbg("exit magus_init func OK, device added\n");
      return 0;
  }
  /* adding device error */
	platform_device_put(magus_snd_device);

  dbg("exit magus_init func Error\n");
	return ret;
}

static void __exit magus_exit(void)
{
	platform_device_unregister(magus_snd_device);
}

module_init(magus_init);
module_exit(magus_exit);

/* Module information */
MODULE_AUTHOR("JF Liu");
MODULE_DESCRIPTION("ALSA SoC Magus");
MODULE_LICENSE("GPL");
