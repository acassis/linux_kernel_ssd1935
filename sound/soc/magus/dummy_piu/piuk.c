/*======================================================================================
* piuk.c 	- dummy PIU driver, the real PIU driver will need to be built sepeartely to support ALSA
========================================================================================*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <asm/siginfo.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

typedef void (*alsa_cb)(void*);
void alsa_cb_register(alsa_cb cb)
{
}
EXPORT_SYMBOL(alsa_cb_register);

typedef void (*audio_ctl_cb)(void*);
void audio_ctl_cb_register(audio_ctl_cb cb)
{
}
EXPORT_SYMBOL(audio_ctl_cb_register);

void alsa_piu_enable(int flag)
{
}
EXPORT_SYMBOL(alsa_piu_enable);

typedef struct 
{
	uint32_t	type;
	uint32_t	len;
	uint8_t		p[64];
}
piu_msg_t, *piu_msg_p;

/* module driver */
int piu_tx(uint32_t msg, piu_msg_p body)
{
  return 0;
}
EXPORT_SYMBOL(piu_tx);

MODULE_DESCRIPTION("dummy PIU");
MODULE_AUTHOR("JF Liu, Solomon Systech Ltd");
MODULE_LICENSE("GPL");

