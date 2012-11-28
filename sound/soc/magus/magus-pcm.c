/*
 * linux/sound/soc/magus/magus-pcm.c -- ALSA PCM interface for the Magus chip
 *
 * Author:	JF Liu
 * Created:	Nov 30, 2007
 * Copyright:	(C) 2007 Solomon Systech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "magus-pcm.h"

// external function prototypes
typedef void (*alsa_cb)(void*);
extern void alsa_cb_register(alsa_cb cb);
extern int piu_tx(uint32_t msg, piu_msg_p body);
extern void alsa_piu_enable(int flag);

/* enable this flag for a timer-interrupt based PCM device test */
//#define DUMMY_TIMER_TEST

/* the flag for debugging, it will print out all the dbg messages */
#define MAGUS_PCM_DEBUG 0

#if MAGUS_PCM_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

/* the base physical address of DSP Tx/Rx register map */
static unsigned int magus_dsp_tdm_base_addr;

/* the static pointer to store the runtime data */
static struct magus_runtime_data *g_prtd = NULL; 

static const struct snd_pcm_hardware magus_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= 128,
	.period_bytes_max	= 8192,
	.periods_min		= 2,
	.periods_max		= 1024,
	.buffer_bytes_max	= 128 * 1024,
	.fifo_size		= 32,
};

struct magus_runtime_data {
	spinlock_t lock;
#ifdef DUMMY_TIMER_TEST	
	struct timer_list timer;
	unsigned int pcm_bps;		/* bytes per second */
	unsigned int pcm_jiffie;	/* bytes per one jiffie */
#endif
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_irq_pos;	/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	struct snd_pcm_substream *substream;
};

/* local function prototype */
static inline int magus_pcm_tdm_config(struct snd_pcm_substream *substream, uint8_t flag);
static inline int magus_pcm_tdm_sync(struct snd_pcm_substream *substream);

#ifdef DUMMY_TIMER_TEST	
static inline void magus_pcm_timer_start(struct magus_runtime_data *prtd)
{
	prtd->timer.expires = 1 + jiffies;
	add_timer(&prtd->timer);
}

static inline void magus_pcm_timer_stop(struct magus_runtime_data *prtd)
{
	del_timer(&prtd->timer);
}

static void magus_pcm_timer_function(unsigned long data)
{
	struct magus_runtime_data *prtd = (struct magus_runtime_data *)data;
	unsigned long flags;
	dbg("enter magus_pcm_timer_function func\n");
	spin_lock_irqsave(&prtd->lock, flags);
	prtd->timer.expires = 1 + jiffies;
	add_timer(&prtd->timer);
	prtd->pcm_irq_pos += prtd->pcm_jiffie;
	prtd->pcm_buf_pos += prtd->pcm_jiffie;
	prtd->pcm_buf_pos %= prtd->pcm_size;
	if (prtd->pcm_irq_pos >= prtd->pcm_count) {
		prtd->pcm_irq_pos %= prtd->pcm_count;
		spin_unlock_irqrestore(&prtd->lock, flags);
		snd_pcm_period_elapsed(prtd->substream);
		dbg("sent snd_pcm_period_elapsed\n");
	} else
		spin_unlock_irqrestore(&prtd->lock, flags);
}

#endif
/* the callback function serves as the PCM interface interrupt service routine */
void magus_pcm_callback(void* data)
{
  magus_audio_data_finish_msg_t* dsp_msg = (magus_audio_data_finish_msg_t*)data;
  unsigned long flags;
  struct magus_runtime_data *prtd = g_prtd;
  uint32_t position = dsp_msg->buf_position;
  uint32_t size = dsp_msg->filled_size;
  
  if(dsp_msg->msgid != MAGUS_TDM_FRAME_DONE_MSG_TYPE)
  {
  	printk("Error: invalid dsp msg id = %d\n", dsp_msg->msgid);
  	return;
  }
  
  //dbg("DSP audio frame msg rxed with data size %d, position 0x%x\n", size, position);
  
  //spin_lock_irqsave(&prtd->lock, flags);
  prtd->pcm_irq_pos += size;
  prtd->pcm_buf_pos += size;
  prtd->pcm_buf_pos %= prtd->pcm_size;
  if (prtd->pcm_irq_pos >= prtd->pcm_count) {
	  prtd->pcm_irq_pos %= prtd->pcm_count;
	  //spin_unlock_irqrestore(&prtd->lock, flags);
	  //dbg("period elapsed \n");
	  snd_pcm_period_elapsed(prtd->substream);
  } 
	//spin_unlock_irqrestore(&prtd->lock, flags);

}

static int magus_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
  struct snd_pcm_runtime *runtime = substream->runtime;
  struct magus_runtime_data *prtd = runtime->private_data;
  int ret;
		  
  dbg("enter magus_pcm_hw_params func \n");

  ret = snd_pcm_lib_malloc_pages(substream,
				 params_buffer_bytes(params));
  if(ret < 0)
  {
    printk("Error: snd_pcm_lib_malloc_pages failed\n");
    return ret;
  }
 
  dbg("exit magus_pcm_hw_params func OK\n");
  return ret;
}

static int magus_pcm_hw_free(struct snd_pcm_substream *substream)
{
  struct magus_runtime_data *prtd = substream->runtime->private_data;
  int ret;
	
  dbg("enter magus_pcm_hw_free func\n");
		 
  ret = snd_pcm_lib_free_pages(substream);
  snd_pcm_set_runtime_buffer(substream, NULL);
	
	dbg("exit magus_pcm_hw_free func %d\n", ret);
	return ret;
}

static int magus_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct magus_runtime_data *prtd = substream->runtime->private_data;
	unsigned int bps;

  dbg("enter magus_pcm_prepare func\n");

#ifdef DUMMY_TIMER_TEST	
	bps = runtime->rate * runtime->channels;
	bps *= snd_pcm_format_width(runtime->format);
	bps /= 8;
	if (bps <= 0)
		return -EINVAL;
		
	prtd->pcm_bps = bps;
	prtd->pcm_jiffie = bps / HZ;
#endif	
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	prtd->pcm_buf_pos = 0;
 
	/* sync with DSP tdm driver in case for underrun/overrun error recovery */
  alsa_cb_register(magus_pcm_callback);
	/* configure TDM */
  magus_pcm_tdm_config(substream, 1);
  /* wait for the TDM to be configured */
  mdelay(100);

  dbg("exit magus_pcm_prepare func\n");
  
	return 0;
}

static int magus_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct magus_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;
  int i;
	
  dbg("enter magus_pcm_trigger function\n");
  spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
    dbg("start cmd received\n");
  case SNDRV_PCM_TRIGGER_RESUME:
  case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#ifdef DUMMY_TIMER_TEST    
	  magus_pcm_timer_start(prtd);    
#else
    /* register callback function */
    alsa_cb_register(magus_pcm_callback);

    /* enable DSP BTDMP by setting its TDM EN Register */
    *((unsigned int *)(magus_dsp_tdm_base_addr+4)) = 1;

    /* start TDM interface */
    //magus_pcm_tdm_config(substream, 1);        
#endif    
		break;

	case SNDRV_PCM_TRIGGER_STOP:
    dbg("stop cmd received\n");
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:

#ifdef DUMMY_TIMER_TEST    
	  magus_pcm_timer_stop(prtd);    
#else 
    /* disable DSP BTDMP by clearing its TDM EN Register */
    *((unsigned int *)(magus_dsp_tdm_base_addr+4)) = 0;

    /* stop TDM interface */
    //magus_pcm_tdm_config(substream, 1);        
#endif    
  	break;
	default:
		dbg("invalid cmd received\n");
		//spin_unlock(&prtd->lock);
		ret = -EINVAL;
		break;
	}
  dbg("exit magus_pcm_trigger func, %d\n", ret);
  spin_unlock(&prtd->lock);
	return ret;
}

static inline int magus_pcm_tdm_config(struct snd_pcm_substream *substream, uint8_t flag)
{
	magus_audio_ctl_msg_t msg;
	piu_msg_t body;
	int ret;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct magus_runtime_data *prtd = substream->runtime->private_data;
    
	dbg("enter magus_pcm_tdm_config func\n");
	
	/* send a message to DSP to start BTDMP interface */
	msg.msgid = MAGUS_TDM_CONFIG_MSG_TYPE;
	msg.flag = flag; //start or stop the tdm

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msg.interface = MAGUS_DSP_TDM_TX;
	else
		msg.interface = MAGUS_DSP_TDM_RX;
	
	msg.sampling_rate = substream->runtime->rate;
	msg.buf_addr = (uint32_t) buf->addr;
	msg.buf_size = 	snd_pcm_lib_buffer_bytes(substream);
	
	/* make sure frame size is aligned to 4 */
	msg.frame_size = (snd_pcm_lib_period_bytes(substream)/4) * 4;
	
	dbg("sent out TDM CONFIG MSG:flag=%d,interface=%d,sample rate=%d,buf_addr=0x%x,buf_size=%d,frm_size=%d\n",
	     flag, msg.interface, msg.sampling_rate, msg.buf_addr,msg.buf_size, msg.frame_size);
  /* fill the generic PIU message body */
  body.type = 1;
  body.len = sizeof(msg);
  memcpy(body.p, &msg, body.len);
  /* transmit the message to DSP */
#if 1
 
  if( piu_tx(PIU_ALSA_CTL_QID, &body) != 0)
  {
		printk ("Error: Tx PIU msg failed\n");
		return -1;
	}
#endif	
  dbg("exit magus_pcm_tdm_config func OK\n");
	return 0;
}

static inline int magus_pcm_tdm_sync(struct snd_pcm_substream *substream)
{
	magus_audio_ctl_msg_t msg;
	piu_msg_t body;
	int ret;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct magus_runtime_data *prtd = substream->runtime->private_data;
    
	dbg("enter magus_pcm_tdm_sync func\n");
	
	/* send a message to DSP to start BTDMP interface */
	msg.msgid = MAGUS_TDM_SYNC_MSG_TYPE;

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msg.interface = MAGUS_DSP_TDM_TX;
	else
		msg.interface = MAGUS_DSP_TDM_RX;
	
	msg.sampling_rate = substream->runtime->rate;
	msg.buf_addr = (uint32_t) buf->addr;
	msg.buf_size = 	snd_pcm_lib_buffer_bytes(substream);
	
	/* make sure frame size is aligned to 4 */
	msg.frame_size = (snd_pcm_lib_period_bytes(substream)/4) * 4;
	
	dbg("sent out TDM SYNC MSG:interface=%d,sample rate=%d,buf_addr=0x%x,buf_size=%d,frm_size=%d\n",
	     msg.interface, msg.sampling_rate, msg.buf_addr,msg.buf_size, msg.frame_size);
  /* fill the generic PIU message body */
  body.type = 1;
  body.len = sizeof(msg);
  memcpy(body.p, &msg, body.len);
  /* transmit the message to DSP */

  if( piu_tx(PIU_ALSA_CTL_QID, &body) != 0)
  {
		printk ("Error: Tx PIU msg failed\n");
		return -1;
	}

  dbg("exit magus_pcm_tdm_sync func OK\n");
	return 0;
}

static snd_pcm_uframes_t
magus_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct magus_runtime_data *prtd = runtime->private_data;
	snd_pcm_uframes_t tmp;
  tmp = bytes_to_frames(runtime, prtd->pcm_buf_pos);
  //dbg("enter magus_pcm_pointer func tmp=%d, position=0x%x\n", tmp, prtd->pcm_buf_pos);
	return tmp;
}

static int magus_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct magus_runtime_data *prtd;
	int ret;
  unsigned int dsp_phy_addr;
  dbg("enter magus_pcm_open func\n");

	snd_soc_set_runtime_hwparams(substream, &magus_pcm_hardware);

  /*
	 * Due to the BTDMP implementation on DSP side, the buffer size and frame size will need to aligned to 32 bytes,
   * which is the TDM buffer transfer per interrupt. Let's add a rule to enforce that.
	 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	prtd = kzalloc(sizeof(struct magus_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		goto out;
	}
#ifdef DUMMY_TIMER_TEST	
	init_timer(&prtd->timer);
	prtd->timer.data = (unsigned long) prtd;
	prtd->timer.function = magus_pcm_timer_function;
#endif	
	spin_lock_init(&prtd->lock);
	prtd->substream = substream;		

	runtime->private_data = prtd;
	
	g_prtd = prtd;

	/* enable PIU communication */
  dbg("enable PIU Tx and RX\n"); 
  alsa_piu_enable(1);

  dbg("register callback\n"); 
  alsa_cb_register(magus_pcm_callback);

  if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dsp_phy_addr = MAGUS_DSP_TDM_TX2_BASE_ADDR;
	else
		dsp_phy_addr = MAGUS_DSP_TDM_RX2_BASE_ADDR;
  
  /* IO re-map for TDM register block memory */
  magus_dsp_tdm_base_addr = (unsigned int)ioremap_nocache(dsp_phy_addr, 0x2000);
	if (0 == magus_dsp_tdm_base_addr)
	{
    kfree(prtd);
    ret = -ENOMEM;
    goto out;
	}

  dbg("TDM phy addr= 0x%x, virt addr=0x%x\n", dsp_phy_addr, magus_dsp_tdm_base_addr);	
  dbg("exit magus_pcm_open func OK\n");
	return 0;

 out:
 	dbg("exit magus_pcm_open func error, %d\n",ret);
	return ret;
}

static int magus_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct magus_runtime_data *prtd = runtime->private_data;
  int i = 0;

 	dbg("enter magus_pcm_close func\n");
	
  /* configure TDM to disconnect the IRQ, disable TDMs*/
  magus_pcm_tdm_config(substream, 0);
  /* unregister callback if it havenot */
  alsa_cb_register(NULL); 
	
  /* disable PIU communication */
  dbg("Disable PIU Tx and RX\n"); 
  alsa_piu_enable(0);

  /* IO unmap */
  if(magus_dsp_tdm_base_addr)
		iounmap((void *)magus_dsp_tdm_base_addr);

	kfree(prtd);
	g_prtd = NULL;
	dbg("exit magus_pcm_close func\n");
	return 0;
}

static int magus_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
  
  dbg("enter magus_pcm_mmap func\n");
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

struct snd_pcm_ops magus_pcm_ops = {
	.open		= magus_pcm_open,
	.close		= magus_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= magus_pcm_hw_params,
	.hw_free	= magus_pcm_hw_free,
	.prepare	= magus_pcm_prepare,
	.trigger	= magus_pcm_trigger,
	.pointer	= magus_pcm_pointer,
	.mmap		= magus_pcm_mmap,
};

static int magus_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = magus_pcm_hardware.buffer_bytes_max;
	
	dbg("enter magus_pcm_preallocate_dma_buffer func\n");
	
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
	{
		dbg("exit magus_pcm_preallocate_dma_buffer func Failed\n");
		return -ENOMEM;
	}
	buf->bytes = size;
	
	dbg("buf allocated at buf->addr=0x%x, buf->area=0x%x, size=%d\n", (unsigned int)buf->addr,(unsigned int) buf->area, (unsigned int)buf->bytes);

	dbg("exit magus_pcm_preallocate_dma_buffer func OK\n");
	return 0;
}

static void magus_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static u64 magus_pcm_dmamask = DMA_32BIT_MASK;

int magus_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;
 	 
  dbg("enter magus_pcm_new func\n");
  
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &magus_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_32BIT_MASK;

	if (dai->playback.channels_min) {
		ret = magus_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		ret = magus_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

 out:
 	dbg("exit magus_pcm_new func %d\n", ret);
	return ret;
}

struct snd_soc_platform magus_soc_platform = {
	.name		= "magus-audio",
	.pcm_ops 	= &magus_pcm_ops,
	.pcm_new	= magus_pcm_new,
	.pcm_free	= magus_pcm_free_dma_buffers,
};

EXPORT_SYMBOL_GPL(magus_soc_platform);

MODULE_AUTHOR("JF Liu");
MODULE_DESCRIPTION("Magus PCM module");
MODULE_LICENSE("GPL");
