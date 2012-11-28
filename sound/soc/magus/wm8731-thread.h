/*
 * linux/sound/magus/magus-pcm-thread.h -- ALSA PCM interface for the Magus chip
 *
 * Author:	JF Liu
 * Created:	Nov 30, 2007
 * Copyright:	(C) 2007 Solomon Systech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef wm8731_THREAD_H
#define wm8731_THREAD_H

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <asm/unistd.h>
#include <asm/semaphore.h>

/* a structure to store all information we need
   for our thread */
typedef struct wm8731_thread_struct
{
    /* private data */
    /* Linux task structure of thread */
    struct task_struct *thread_task;

    /* function to be started as thread */
    void (*function) (struct wm8731_thread_struct *wm8731_thread);
       
    /* flag to tell thread whether to die or not.
       When the thread receives a signal, it must check
       the value of terminate and call exit_wm8731_thread and terminate
       if set.
    */
    int terminate;
    
    /* register cmd complete indication flag */
    struct completion	flag;
    /* additional data to pass to kernel thread */
    unsigned int data;
    /* completed flag to inform the parent to kill the thread */
	  struct completion	thread_notifier;

} wm8731_thread_t;

/* audio control callback function */
void wm8731_audio_ctl_callback_func(void* data);

/* audio control main thread function */
int wm8731_thread(void* data);

/* wm8731 thread data */
extern wm8731_thread_t wm8731_thread_data;
extern u16 wm8731_reg_dsp_cache[];

#endif
/********************************END_OF_FILE*********************************/
