/*
 * linux/sound/magus/magus-pcm-thread.c -- ALSA PCM interface for the Magus chip
 *
 * Author:	JF Liu
 * Created:	Nov 30, 2007
 * Copyright:	(C) 2007 Solomon Systech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/signal.h>

#include <asm/semaphore.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "magus-pcm.h"
#include "wm8731.h"
#include "wm8731-thread.h"

extern int piu_tx(uint32_t msg, piu_msg_p body);

#define WM8731_THREAD_DEBUG 0

#if WM8731_THREAD_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif
/* Local func prototype */

/* wm8731 thread data */
wm8731_thread_t wm8731_thread_data;

/* codec register cache from DSP */
u16 wm8731_reg_dsp_cache[WM8731_CACHEREGNUM];

u8 wm8731_reg_dsp_cache_flag = 0;

/* This callback function will be invoked when receive cmd from DSP */
/* This func will directly configure codec registers */
void wm8731_audio_ctl_callback_func(void* data)
{
  struct acmsg_t* acmsg = (struct acmsg_t*)data;
	char cmd;
  struct snd_soc_codec *codec = g_socdev->codec;

	if (acmsg->msgid != PIUMSG_AUDIOCTL_TX)
	{
    printk("WM8731 Codec Err: Invalid msg from DSP %d\n", acmsg->msgid);
		return;
  }
	cmd = (acmsg->data >> 16) & 0xFF;
	if(cmd == WM8731_INIT_CMD) /* the first command */
  {
    dbg("WM8731_thread started\n");
    wm8731_reg_dsp_cache_flag = 1;
	  codec->restore_flag = 1;   
  }
#if 0
  else if(cmd == WM8731_SHUTDOWN_CMD)
  {
    dbg("WM8731_thread killed \n");
    wm8731_reg_dsp_cache_flag = 0;
    stop_wm8731_thread(&wm8731_thread_data);
    init_completion(&wm8731_thread_data.flag);
    return;      
	}
#endif
  /* set the completion flag to wakeup the thread */
  wm8731_thread_data.data = acmsg->data;
  complete(&wm8731_thread_data.flag);	
  
  return;
}

int wm8731_thread(void *data)
{
  char cmd, data1,data2;  
  piu_msg_t	ac_piu_r;
  struct acmsg_rep_t	acmsg_rep;
  int ret, reg, i;
  struct snd_soc_codec *codec = g_socdev->codec;
  wm8731_thread_t* thread_data = data;
  
  /* initialise termination flag */
  thread_data->terminate = 0;
  dbg("wm8731 thread started\n");
  
  /* store all register values configured from DSP */
  memcpy(wm8731_reg_dsp_cache, codec->reg_cache, sizeof(wm8731_reg_dsp_cache));      

  /* main loop to handle DSP codec config requests */
  for(;;)
  {
    /* fall asleep for one second */
    ret = wait_for_completion_interruptible_timeout(&thread_data->flag, HZ);
    if (ret==0)		// time out
	  {
      /* check if a terminate signal received or not */
		  if (thread_data->terminate)
        break;    
      else
        continue;		
    }
	  if (ret == -ERESTARTSYS)
			break;
	            
    /* command received */
    cmd = (thread_data->data >> 16) & 0xFF;
	  data1 = (thread_data->data) & 0xFF;
	  data2 = (thread_data->data >>8) & 0xFF;
  	switch(cmd)
	  {
		case WM8731_INIT_CMD: /* codec init command */
      /* turn on codec */
      wm8731_write(codec, WM8731_PWR, 0x4e);
	      
      /* enable DAC */	    
      reg = wm8731_read_reg_cache(codec, WM8731_APANA);
	    wm8731_write(codec, WM8731_APANA, reg | 0x0010);

      /* Disable Digital Mute */
      reg = wm8731_read_reg_cache(codec, WM8731_APDIGI) & 0xfff7;
  		wm8731_write(codec, WM8731_APDIGI, reg);
#if 1
    	/* enable input line */
      reg = wm8731_read_reg_cache(codec, WM8731_LINVOL) & 0xff7f;
	    wm8731_write(codec, WM8731_LINVOL, reg);
	    reg = wm8731_read_reg_cache(codec, WM8731_RINVOL) & 0xff7f;
	    wm8731_write(codec, WM8731_RINVOL, reg);
     
	    /* set ADC input (default is line-in) */
	    
	    /* enable Microphone */
#endif 	    
	    /* set to 16bits format */      
      reg = wm8731_read_reg_cache(codec, WM8731_IFACE) & 0xfff3;
	    wm8731_write(codec, WM8731_IFACE, reg);

	    /* set to Master mode */      
      reg = wm8731_read_reg_cache(codec, WM8731_IFACE);
	    wm8731_write(codec, WM8731_IFACE, reg | 0x0040);

	    /* set Data format to DSP */
      reg = wm8731_read_reg_cache(codec, WM8731_IFACE);
	    wm8731_write(codec, WM8731_IFACE, reg | 0x0003);

    	/* set sample rate for default 48k, it will be changed later base on stream info */
      /* Normal mode, Mclk:12.288Mhz, BOSR:0 (256fs), SR3-SR0:0000 */
      wm8731_write(codec, WM8731_SRATE, 0x0000);
#if 0	    
  	  /* disable bypass */
      reg = wm8731_read_reg_cache(codec, WM8731_APANA)& 0xFFF7;
	    wm8731_write(codec, WM8731_APANA, reg);
#endif
	    /* activate Digital Interface */
      wm8731_write(codec, WM8731_ACTIVE, 0x1);

		  dbg("codec Init ok\n");

      break;
		case WM8731_POWER_DOWN_SET_CMD:
      /* Set power down register */
      if(data1 == 0xff)
      {
        /* de-activate Digital Interface */
        wm8731_write(codec, WM8731_ACTIVE, 0x0);
        wm8731_write(codec, WM8731_PWR, 0x4e); /* put codec standby */  
        codec->restore_flag = 0;
      }
      else
        wm8731_write(codec, WM8731_PWR, data1); /* power up codec */
		  dbg("codec power down set 0x%x\n", data1);
      break;
		case WM8731_SAMPLE_RATE_SET_CMD:
			switch(data1)
			{
				case SAMPLE_RATE_ADC_96_DAC_96: 
          /*Rate: 96Khz USB mode, Mclk:12Mhz, BOSR:0 (256fs), SR3-SR0:0111 */
          wm8731_write(codec, WM8731_SRATE, 0x001d);	    
          break;
				case SAMPLE_RATE_ADC_48_DAC_48: 
          /*Rate: 48Khz USB mode, Mclk:12Mhz, BOSR:0 (256fs), SR3-SR0:0000 */
          wm8731_write(codec, WM8731_SRATE, 0x0001);	    
          break;
				case SAMPLE_RATE_ADC_44_1_DAC_44_1: 
          /*Rate: 44.1Khz USB mode, Mclk:12Mhz, BOSR:1 (384fs), SR3-SR0:1000 */
          wm8731_write(codec, WM8731_SRATE, 0x0023);	    
          break;
				case SAMPLE_RATE_ADC_32_DAC_32: 
          /*Rate: 32Khz USB mode, Mclk:12Mhz, BOSR:0 (256fs), SR3-SR0:0110 */
          wm8731_write(codec, WM8731_SRATE, 0x0019);	    
          break;
				case SAMPLE_RATE_ADC_8_DAC_8:
          /*Rate: 8Khz USB mode, Mclk:12Mhz, BOSR:0 (256fs), SR3-SR0:0011 */
          wm8731_write(codec, WM8731_SRATE, 0x000d);	    
          break;
				default:
          printk("WM8731 Codec: Wrong sampling rate from DSP, use default 48KHz\n");
          wm8731_write(codec, WM8731_SRATE, 0x0001);	    
          break;
			}
			dbg("sampling rate set =%d\n",data1);
      break;
		case WM8731_OUTPUT_VOLUME_SET_CMD:
      dbg("output volume value is %d, for ch %d\n", data1, data2);
      data1 += 48;
      if(data1 > 127) 
        data1 = 127;
      if(data2 == 0) //left channel 
      {
        reg = wm8731_read_reg_cache(codec, WM8731_LOUT1V) & 0xff80; // clear bits 6:0
	      wm8731_write(codec, WM8731_LOUT1V, reg | data1);
      }
      else if(data2 == 1) //right channel
      {
	      reg = wm8731_read_reg_cache(codec, WM8731_ROUT1V) & 0xff80;
	      wm8731_write(codec, WM8731_ROUT1V, reg | data1);
      }
      else if(data2 == 2) //both channels
      {
        reg = wm8731_read_reg_cache(codec, WM8731_LOUT1V) & 0xff80;
	      wm8731_write(codec, WM8731_LOUT1V, reg | data1);
  	    reg = wm8731_read_reg_cache(codec, WM8731_ROUT1V) & 0xff80;
	      wm8731_write(codec, WM8731_ROUT1V, reg | data1);
      }
      break;
		case WM8731_INPUT_VOLUME_SET_CMD:
      dbg("line-in volume value is %d, for ch %d\n", data1, data2);
      if(data1 > 0x1F)  // maximum +12dB
        data1 = 0x1F;
      if(data2 == 0) //left channel 
      {
        reg = wm8731_read_reg_cache(codec, WM8731_LINVOL) & 0xffe0; // clear bits 4:0;
	      wm8731_write(codec, WM8731_LINVOL, reg | data1);
      }
      else if(data2 == 1) //right channel
      {
	      reg = wm8731_read_reg_cache(codec, WM8731_RINVOL) & 0xffe0;;
	      wm8731_write(codec, WM8731_RINVOL, reg | data1);
      }
      else if(data2 == 2) //both channels
      {
        reg = wm8731_read_reg_cache(codec, WM8731_LINVOL) & 0xffe0;;
	      wm8731_write(codec, WM8731_LINVOL, reg | data1);
	      reg = wm8731_read_reg_cache(codec, WM8731_RINVOL) & 0xffe0;;
	      wm8731_write(codec, WM8731_RINVOL, reg | data1);
      }
      break;
    case WM8731_GENERIC_REGISTER_SET_CMD:
      dbg("write codec register 0x%x,with value=0x%x\n", data2, data1);
      wm8731_write(codec, data2, data1);
      break;
    case WM8731_GENERIC_REGISTER_GET_CMD:
      reg = wm8731_read_reg_cache(codec, data2);
      dbg("read codec register 0x%x value=0x%x\n", data2, reg);
      break;
#if 0
    case WM8731_REGISTER_RESTORE_CMD:
      /* restore the previous register values */
      for(i=0;i<WM8731_CACHEREGNUM;i++)
        wm8731_write(codec, i, wm8731_reg_dsp_cache[i]);
      dbg("codec regisgters restored\n");
      break;
#endif
		default:
      printk("WM8731 Error: Invalid audio ctl cmd 0x%x from DSP\n", cmd);
      break;
	  } 
    /* store all register values configured from DSP */
    if( wm8731_reg_dsp_cache_flag == 1)
      memcpy(wm8731_reg_dsp_cache, codec->reg_cache, sizeof(wm8731_reg_dsp_cache));
    /* send out a response */  
	  acmsg_rep.msgid = PIUMSG_AUDIOCTL_TX;
	  acmsg_rep.ret = 1;
    /* for register read cmd, put the register value on lowest 8 bits */
    if(cmd == WM8731_GENERIC_REGISTER_GET_CMD)
      acmsg_rep.ret = (1<<8) + reg;
	  ac_piu_r.type = PIU_REP;
	  ac_piu_r.len = sizeof(acmsg_rep);
	  memcpy(ac_piu_r.p, &acmsg_rep, ac_piu_r.len);
    /* transmit the message to DSP */
    if( piu_tx(PIU_AUDIO_CTL_QID, &ac_piu_r) != 0)
      printk ("WM8731 Codec: Error tx PIU msg error\n");    
  }
  codec->restore_flag = 0;
  /* here we go only in case of termination of the thread */

  /* cleanup the thread, leave */
  dbg("wm8731_thread killed\n");
  //thread_data->terminate = 1;    
  thread_data->thread_task = NULL;
  // tell the parent process the thread is killed by itself
  complete_and_exit(&thread_data->thread_notifier, 0);
} 
/********************************END_OF_FILE*********************************/

