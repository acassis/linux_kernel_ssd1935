/*
 * ssl_udc.c -- for Solomon Systech dual high/full speed udc
 *
 * Copyright (C) 2005 Solomon Systech Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <asm/mach-types.h>

#include "usbd.h"
//#define dbg	printk
#if UDC_DEBUG
#define dbg(format, arg...) printk(format, ##arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif

#if IO_MAP == 3
#warning "using ebm interface"
#include "ebm_mem.h"
#include "io.h"
typedef	int	gfp_t;
#endif

#if INTC
#warning "using intc as a module"
#include "intck.h"
#define request_irq	intc_req
#define free_irq	intc_free
#define enable_irq	intc_enable
#define disable_irq	intc_disable
#endif

//#define NFSTOP

#ifndef CONFIG_ARCH_MAGUS_FPGA
static uint32_t	flag;
#else
static uint32_t	flag;
//static uint32_t	flag = 3;	/* full speed & pio */
#endif
module_param(flag, uint, 0);

#define DRV_DESC	"usbd"
static const char driver_name [] = DRV_DESC;

struct udc;
typedef struct udc	udc_t, *udc_p;

typedef struct udc_ep 
{
	struct usb_ep	ep;
	udc_p			udc;

	const struct usb_endpoint_descriptor	*desc;
	struct list_head	queue;
	uint8_t		bEndpointAddress;
	uint8_t		bmAttributes;

#ifndef NFSTOP
	unsigned	fstopped : 1;
#endif
	unsigned	fstall : 1;
}
udc_ep_t, *udc_ep_p;

typedef struct udc_request 
{
	struct usb_request	req;
	struct list_head	queue;
	udc_ep_p			ep;
	usbd_rq_t			rq;
	unsigned			fmapped : 1;
}
udc_req_t, *udc_req_p;

#define UDC_EP0_SETUP	0
#define UDC_EP0_IN		1
#define UDC_EP0_DAT		2
#define UDC_EP0_STA		4
#define UDC_EP0_STG_SETUP	UDC_EP0_SETUP
#define UDC_EP0_STG_DAT_OUT	UDC_EP0_DAT
#define UDC_EP0_STG_DAT_IN	(UDC_EP0_DAT | UDC_EP0_IN)
#define UDC_EP0_STG_STA_OUT	UDC_EP0_STA
#define UDC_EP0_STG_STA_IN	(UDC_EP0_STA | UDC_EP0_IN)

struct udc 
{
	struct usb_ctrlrequest		setup;	/* internal buffer for ep0 */

	struct usb_gadget			gdt;
	struct usb_gadget_driver	*drv;
	spinlock_t					lock;
	struct completion			*release;

	usbd_t		hw;			/* hardware  context */
	usbd_rq_t	rq;			/* internal req for ep0 */
	uint8_t		stage;		/* ep0 stage UDC_EP0_STG_XXX */

	udc_ep_t	ep[32];

	/* device get_status handling */
	unsigned	fwakeup : 1,
				fspower : 1;
};


//#define UDC_SIMPNP
#ifdef UDC_SIMPNP
static uint32_t	addr;
static int	irq;
module_param(addr, uint, 0);
module_param(irq, int, 0);
static struct resource _rc[2];
static struct platform_device dv = 
{
	.name = DRV_DESC,
#if IO_MAP == 1
	.num_resources = 2,
	.resource = _rc,
#endif
};
#endif


static int udc_ep_enable(struct usb_ep *_ep,
		const struct usb_endpoint_descriptor *desc)
{
	udc_ep_p	ep = container_of(_ep, udc_ep_t, ep);
	udc_p			udc;
	unsigned long	flags;
	uint16_t		mps;
	uint8_t			nmic;
	int				rt;

	/* catch various bogus parameters */
	if (!_ep || !desc || ep->desc
			|| desc->bDescriptorType != USB_DT_ENDPOINT
			|| ep->bEndpointAddress != desc->bEndpointAddress
			|| _ep->maxpacket < (le16_to_cpu(desc->wMaxPacketSize) & 0x7FF)) 
	{
printk("ena: bad ep ep=%X desc=%X epdesc=%X eadr=%X dadr=%X maxpkt=%d\n",
(int)_ep, (int)desc, (int)ep->desc, ep->bEndpointAddress, 
desc->bEndpointAddress, _ep->maxpacket);
l_err:
		return -EINVAL;
	}
	udc = ep->udc;

	mps = le16_to_cpu (desc->wMaxPacketSize);
	nmic = mps >> 11;
	mps &= 0x7FF;
	if (mps > 1024 || nmic > 2)
	{
printk("ena: mps=%d nmic=%d\n", mps, nmic);
		goto l_err;
	}
	rt = desc->bmAttributes & 3;
	if (nmic && !(udc->gdt.speed == USB_SPEED_HIGH
			&& (rt == USB_ENDPOINT_XFER_ISOC || 
			 	rt == USB_ENDPOINT_XFER_INT)))
	{
printk("ena: nmic=%d spd=%d type=%d\n", nmic, udc->gdt.speed, rt);
		goto l_err;
	}

	if (!udc->drv || udc->gdt.speed == USB_SPEED_UNKNOWN) 
	{
dbg("%s, bogus device state\n", __FUNCTION__);
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(&udc->lock, flags);

	rt = usbd_ep_enable(&udc->hw, (void *)desc);
	if (rt)
	{
printk("ena: err %d\n", rt);
		return -EINVAL;
	}
	_ep->maxpacket = mps;
	ep->bmAttributes = desc->bmAttributes;
	ep->desc = desc;
#ifndef NFSTOP
	ep->fstopped = 0;
#endif

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static void nuke(udc_ep_p, int status);

static int udc_ep_disable(struct usb_ep *_ep)
{
	udc_ep_p	ep = container_of(_ep, udc_ep_t, ep);
	udc_p		udc;
	unsigned long	flags;

	if (!_ep || !ep->desc) 
	{
		return -EINVAL;
	}
	ep->desc = 0;
#ifndef NFSTOP
	ep->fstopped = 1;
#endif
	udc = ep->udc;
	spin_lock_irqsave(&udc->lock, flags);
	nuke (ep, -ESHUTDOWN);
	usbd_ep_disable(&udc->hw, ep->bEndpointAddress);
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}


static struct usb_request *udc_alloc_request(struct usb_ep *ep, gfp_t flags)
{
	udc_req_p	req;

	req = kmalloc(sizeof(*req), flags);
	if (!req) 
	{
		return NULL;
	}
	memset (req, 0, sizeof(*req));
	INIT_LIST_HEAD (&req->queue);
	return &req->req;
}

static void udc_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	udc_req_p	req = container_of(_req, udc_req_t, req);

	if (_req)
	{
		kfree (req);
	}
}


static void done(udc_ep_p ep, udc_req_p req, int status)
{
#ifndef NFSTOP
	unsigned	stopped = ep->fstopped;
#endif

	list_del_init(&req->queue);
//if (ep->bEndpointAddress & 15)
//dbg("Q%d_%d_%d\n", 
//(ep->bEndpointAddress >> 7) | ((ep->bEndpointAddress & 15) << 1),
//ep->bmAttributes, req->req.actual);

	if (req->req.status == -EINPROGRESS)
		req->req.status = status;
	else
		status = req->req.status;

#if USBD_DMA == 1
	if (ep->udc->hw.fdma) 
	{
#if IO_MAP == 1
		if (req->fmapped) 
		{
			dma_unmap_single(ep->udc->gdt.dev.parent,
				req->req.dma, req->req.length,
				(ep->bEndpointAddress & USB_DIR_IN)
					? DMA_TO_DEVICE : DMA_FROM_DEVICE);
			req->req.dma = 0;
			req->fmapped = 0;
		}
		else
		{
			dma_sync_single_for_cpu(ep->udc->gdt.dev.parent,
				req->req.dma, req->req.length,
				(ep->bEndpointAddress & USB_DIR_IN)
					? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		}
#else
		/* copy to/from local memory through EBM for DMA */
		if (ep->bEndpointAddress && !(ep->bEndpointAddress & USB_DIR_IN) 
				&& ep->udc->hw.fdma && req->req.actual)
		{
			io_rds(req->req.dma, req->req.buf, req->req.actual);
		}
#endif
	}
#endif

	/* don't modify queue heads during completion callback */
#ifndef NFSTOP
	ep->fstopped = 1;
#endif
	spin_unlock(&ep->udc->lock);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&ep->udc->lock);
#ifndef NFSTOP
	ep->fstopped = stopped;
#endif
}


/* USB Gadget EP API */


void ddump(void *, int);
static int udc_ep_queue(struct usb_ep *_ep, struct usb_request *_req, 
				gfp_t flags)
{
	udc_ep_p	ep = container_of(_ep, udc_ep_t, ep);
	udc_req_p	req = container_of(_req, udc_req_t, req);
	udc_p		udc;
	unsigned long	iflags;
	int		rt;
	uint8_t	adr;

	/* catch various bogus parameters */
	if (!_req || !req->req.complete || !req->req.buf
			|| !list_empty(&req->queue))
	{
udc_req_p	r;
r = list_entry(ep->queue.next, udc_req_t, queue);
dbg("bad parm len=%d\n", r->req.length);
ddump(r->req.buf, r->req.length);
		return -EINVAL;
	}
	adr = ep->bEndpointAddress;
	if (!_ep || (!ep->desc && adr)) 
	{
dbg("q: bad ep ep=%X epdesc=%X adr=%X\n", (int)_ep, (int)ep->desc, adr);
		return -EINVAL;
	}

	udc = ep->udc;
	if (!udc->drv || udc->gdt.speed == USB_SPEED_UNKNOWN)
	{
dbg("bad drv/spd\n");
		return -ESHUTDOWN;
	}

	if (!adr && (udc->stage & UDC_EP0_IN))
	{
		/* we use 2 simplex eps (0 & 0x80) for ep0 */
		adr = USB_DIR_IN;
	}

#if USBD_DMA == 1
	if (udc->hw.fdma)
	{
#if IO_MAP == 1
		if (!_req->dma) 
		{
			_req->dma = dma_map_single(ep->udc->gdt.dev.parent,
				_req->buf, _req->length,
				(adr & USB_DIR_IN) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
			req->fmapped = 1;
		}
		else
		{
			dma_sync_single_for_device(ep->udc->gdt.dev.parent,
				_req->dma, _req->length,
				(adr & USB_DIR_IN) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
			req->fmapped = 0;
		}
#else
		/* copy to/from local memory through EBM for DMA */
		if ((adr & USB_DIR_IN) && _req->length)
		{
			io_wrs(_req->dma, _req->buf, _req->length);
		}
#endif
	}
#endif

// lock was here
	_req->status = -EINPROGRESS;
	_req->actual = 0;

	req->rq.data = udc->hw.fdma ? (uint8_t *)_req->dma : _req->buf;
	req->rq.len = _req->length;
	req->rq.actual = 0;
	req->rq.ctx = req;
	req->ep = ep;
//if (adr & 15)
//dbg("q%d_%d_%d_%d\n", (adr >> 7) | ((adr & 15) << 1), 
//ep->bmAttributes, _req->length, ep->fstopped);

	spin_lock_irqsave(&udc->lock, iflags);

	/* kickstart queues */
// 070717 - added fstop check back
	if (list_empty(&ep->queue) 
#ifndef NFSTOP
			&& !ep->fstopped
#endif
		)
	{
		rt = usbd_ep_io(&udc->hw, adr, &req->rq);
	}

	/* irq handler advances the queue */
	list_add_tail(&req->queue, &ep->queue);

	spin_unlock_irqrestore(&udc->lock, iflags);
	return 0;
}

static int udc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	udc_ep_p		ep = container_of(_ep, udc_ep_t, ep);
	udc_req_p		req;
	udc_p			udc;
	unsigned long	flags;

	if (!_ep || !_req)
	{
		return -EINVAL;
	}

	udc = ep->udc;
	spin_lock_irqsave(&udc->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry (req, &ep->queue, queue) 
	{
		if (&req->req == _req)
		{
			break;
		}
	}
	if (&req->req != _req) 
	{
		spin_unlock_irqrestore(&udc->lock, flags);
		return -EINVAL;
	}

	if (ep->queue.next == &req->queue) 
	{
		usbd_ep_abort(&udc->hw, ep->bEndpointAddress);
	}

	done(ep, req, -ECONNRESET);
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int udc_ep_set_halt(struct usb_ep *_ep, int value)
{
	udc_ep_p		ep = container_of(_ep, udc_ep_t, ep);
	unsigned long	flags;
	int				rt;

	spin_lock_irqsave(&ep->udc->lock, flags);
	if (value && (ep->bEndpointAddress & USB_DIR_IN) && !list_empty(&ep->queue))
	{
		/* don't stall if any packets are queued */
		rt = -EAGAIN;
		goto l_rt;
	}
	ep->fstall = value;
	usbd_ep_stall(&ep->udc->hw, ep->bEndpointAddress, value);
	rt = 0;
l_rt:
	spin_unlock_irqrestore(&ep->udc->lock, flags);
	return rt;
}

static struct usb_ep_ops udc_ep_ops = 
{
	.enable			= udc_ep_enable,
	.disable		= udc_ep_disable,
	.alloc_request	= udc_alloc_request,
	.free_request	= udc_free_request,
	.queue			= udc_ep_queue,
	.dequeue		= udc_ep_dequeue,
	.set_halt		= udc_ep_set_halt,
};


/* USB gadget device API */

static int udc_get_frame(struct usb_gadget *gdt)
{
	udc_p	udc;

	udc = container_of(gdt, udc_t, gdt);
	return usbd_frame(&udc->hw);
}

static int udc_wakeup(struct usb_gadget *gdt)
{
	udc_p			udc;
	unsigned long	flags;

	udc = container_of(gdt, udc_t, gdt);
	if (!udc->fwakeup)
	{
		return 0;
	}
	spin_lock_irqsave(&udc->lock, flags);
	usbd_wakeup(&udc->hw);
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int udc_pullup(struct usb_gadget *gdt, int is_on)
{
	udc_p			udc;
	unsigned long	flags;

	udc = container_of(gdt, udc_t, gdt);
	spin_lock_irqsave(&udc->lock, flags);
	usbd_connect(&udc->hw, is_on);
	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int udc_set_selfpowered(struct usb_gadget *gdt, int on)
{
	udc_p	udc;

	udc = container_of(gdt, udc_t, gdt);
	udc->fspower = on;
	return 0;
}

static struct usb_gadget_ops udc_ops = 
{
	.get_frame			= udc_get_frame,
	.wakeup				= udc_wakeup,
	.set_selfpowered	= udc_set_selfpowered,
	.pullup				= udc_pullup,
};


/* dequeue ALL requests; caller holds udc->lock */
static void nuke(udc_ep_p ep, int status)
{
	/* abort any transactions */
	if (ep->desc)
	{
#ifndef NFSTOP
		if (ep->bEndpointAddress)
		{
			ep->fstopped = 1;
		}
#endif
		usbd_ep_abort(&ep->udc->hw, ep->bEndpointAddress);
		/* disable non-0 ep config */
		if (ep->bEndpointAddress)
		{
			ep->desc = 0;
			usbd_ep_disable(&ep->udc->hw, ep->bEndpointAddress);
		}
	}
	while (!list_empty(&ep->queue)) 
	{
		udc_req_p	req;

		req = list_entry(ep->queue.next, udc_req_t, queue);
		done(ep, req, status);
	}
}


/* caller holds udc->lock */
static void udc_quiesce(udc_p udc)
{
	udc_ep_p	ep;

	udc->gdt.speed = USB_SPEED_UNKNOWN;
	nuke(udc->ep, -ESHUTDOWN);
	list_for_each_entry (ep, &udc->gdt.ep_list, ep.ep_list)
	{
		nuke(ep, -ESHUTDOWN);
	}
}


int  udc_setup(udc_p udc)
/**<
filter control messages
@param[in]	udc		context
@return		1-handled internally	0-pass to class driver
*/
{
	uint8_t	typ = udc->setup.bRequestType;
	uint8_t	flag = 0;

#if 1
if ((udc->setup.bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS)
dbg("c%X\n", udc->setup.bRequest);
else
if (udc->setup.bRequest == 6)
if ((udc->setup.wValue >> 8) == 3)
dbg("ss%X\n", udc->setup.wValue & 0xff);
else
dbg("sd%X\n", udc->setup.wValue >> 8);
else if (udc->setup.bRequest == 9)
dbg("sc%X\n", udc->setup.wValue);
else if (udc->setup.bRequest == 11)
dbg("si%X_%X\n", udc->setup.wIndex, udc->setup.wValue);
else
dbg("s%X\n", udc->setup.bRequest);
#endif
	switch (udc->setup.bRequest)
	{
		case USB_REQ_SET_ADDRESS:
			if (typ)
			{
				break;
			}
			goto l_sta;

		case USB_REQ_GET_STATUS:
			switch (typ & USB_RECIP_MASK)
			{
				case USB_RECIP_INTERFACE:
					*(uint16_t *)&udc->setup = 0;
					break;

				case USB_RECIP_DEVICE:
					*(uint16_t *)&udc->setup = (udc->fwakeup << 1) | udc->fspower;
					break;

				case USB_RECIP_ENDPOINT:
					/* stall status of ep */
					typ = (uint8_t)udc->setup.wIndex;
					typ = (typ >> 7) | (typ << 1);
					*(uint16_t *)&udc->setup = udc->ep[typ].fstall;
			}
			udc->rq.len = 2;
#if USBD_DMA == 1
			/* copy to/from local memory through EBM for DMA */
			if (udc->hw.fdma)
			{
#if IO_MAP == 1
				dma_sync_single_for_device(udc->gdt.dev.parent,
					(uint32_t)udc->rq.data, 2, DMA_TO_DEVICE);
#else
				io_wr32(udc->rq.data, *(uint32_t *)&udc->setup);
#endif
			}
#endif
			goto l_dat;

		case USB_REQ_SET_FEATURE:
			flag++;
			/* fall through */

		case USB_REQ_CLEAR_FEATURE:
			switch (typ & USB_RECIP_MASK)
			{
				case USB_RECIP_DEVICE:
					/* device wakeup & test modes 1-4 */
					switch (udc->setup.wValue)
					{
						case USB_DEVICE_REMOTE_WAKEUP:
							udc->fwakeup = flag;
							goto l_sta;

						case USB_DEVICE_TEST_MODE:
							if (udc->setup.wIndex < 1 && udc->setup.wIndex > 4)
							{
								break;
							}
							usbd_test(&udc->hw, udc->setup.wIndex);
							goto l_sta;

#ifdef SSL_OTG
						case USB_DEVICE_B_HNP_ENABLE:
							otg_hnp_ena(flag);
							goto l_sta;
#endif
					}
					break;

				case USB_RECIP_ENDPOINT:
					/* ep halt */
					typ = (uint8_t)udc->setup.wIndex;
					if (udc->setup.wValue != USB_ENDPOINT_HALT)
					{
						break;
					}
					usbd_ep_stall(&udc->hw, typ, flag);
					typ = (typ >> 7) | (typ << 1);
					udc->ep[typ].fstall = flag;
					goto l_sta;
			}
	}
	return 0;

l_sta:
	udc->rq.len = 0;
l_dat:
	usbd_ep_io(&udc->hw, USB_DIR_IN, &udc->rq);
	return 1;
}


static void  udc_evt(void *ctx, usbd_rq_p rq)
{
	int		rt;
	udc_p	udc = ctx;

	if (rq > USBD_EV_MAX)
	{
		/* data */
		udc_req_p	req;

		req = rq->ctx;
		if (!req)
		{
			/* internal ep0 xfers */
			if (udc->stage == UDC_EP0_SETUP)
			{
#if USBD_DMA == 1
				/* copy to/from local memory through EBM for DMA */
				if (udc->hw.fdma)
				{
#if IO_MAP == 1
					dma_sync_single_for_cpu(udc->gdt.dev.parent,
						(uint32_t)rq->data, rq->actual, DMA_FROM_DEVICE);
#else
					io_rds(rq->data, &udc->setup, rq->actual);
#endif
				}
#endif
				/* stage: setup -> data [in/out] or status[in] */
				udc->stage = udc->setup.wLength ?
					((udc->setup.bRequestType & USB_DIR_IN) ? 
						(UDC_EP0_DAT | UDC_EP0_IN) : UDC_EP0_DAT)
					: (UDC_EP0_STA | UDC_EP0_IN);
				if (!udc_setup(udc))
				{
					spin_unlock(&udc->lock);
					rt = udc->drv->setup(&udc->gdt, &udc->setup);
					spin_lock(&udc->lock);
					if (rt < 0)
					{
						/* protocol stall - done to auto-clear */
dbg("handle setup failed %d\n", rt);
ddump(&udc->setup, 8);
						rt = udc->setup.bRequestType & USB_DIR_IN;
						usbd_ep_stall(&udc->hw, rt & ~1, 1);
					}
				}
			}
			else if (udc->stage & UDC_EP0_DAT)
			{
				/* queue status */
				udc->stage = UDC_EP0_STA;
				udc->rq.len = 0;
				rt = usbd_ep_io(&udc->hw, 0, &udc->rq);
			}
			else
			{
				/* set addr after status stage of SET_ADDR */
				if (udc->setup.bRequest == USB_REQ_SET_ADDRESS && 
					!udc->setup.bRequestType && !(udc->setup.wValue >> 8))
				{
					usbd_addr(&udc->hw, (uint8_t)udc->setup.wValue);
				}

				/* queue setup for next control xfer */
l_qsetup:
				udc->stage = UDC_EP0_SETUP;
				usbd_ep_pid(&udc->hw, 0, 0);
				usbd_ep_pid(&udc->hw, USB_DIR_IN, 1);
				udc->rq.len = 8;
				rt = usbd_ep_io(&udc->hw, 0, &udc->rq);
				if (rt < 0)
				{
					dbg("ssl_udc: evt err - qsetup %d\n", rt);
				}
			}
		}
		else
		{
			udc_ep_p	ep;

			/* update transfered length for non-zero length packets */
			if (req->req.length == rq->len)
			{
				req->req.actual = rq->actual;
			}

			ep = req->ep;
			if (!ep->bEndpointAddress)
			{
				/* EP0: only status & data post-stages gets here */
				if (udc->stage & UDC_EP0_STA)
				{
					done(ep, req, 0);
					goto l_qsetup;
				}

				/* generate zero length IN packet */
				if (req->req.zero && rq->len && (udc->stage & UDC_EP0_IN)
/* added 070716 - class driver does not check this anymore */
					&& !(rq->len & (ep->ep.maxpacket - 1)))
				{
					rq->len = 0;	/* will not come through here again */
					rt = usbd_ep_io(&udc->hw, USB_DIR_IN, rq);
					if (rt < 0)
					{
						dbg("ssl_udc: evt err - q zlp %d\n", rt);
					}
					return;
				}

#if USBD_DMA == 1
				/* copy to/from local memory through EBM for DMA */
				if (udc->hw.fdma && !(udc->stage & UDC_EP0_IN))
				{
#if IO_MAP == 1
					dma_sync_single_for_cpu(udc->gdt.dev.parent,
						(uint32_t)rq->data, rq->actual, DMA_FROM_DEVICE);
#else
					io_rds(req->req.dma, req->req.buf, rq->actual);
#endif
				}
#endif
				udc->stage ^= UDC_EP0_STA | UDC_EP0_DAT | UDC_EP0_IN;
				done(ep, req, 0);

				/*
					class driver only queues 1 response, data or status
					for cases with data, we must queue status
				*/
				udc->rq.len = 0;
				rt = usbd_ep_io(&udc->hw, (udc->stage & UDC_EP0_IN) ? 
						USB_DIR_IN : 0, &udc->rq);
				if (rt < 0)
				{
					dbg("ssl_udc: evt err - q sta out %d\n", rt);
				}
				return;
			}
#if 0
if (ep->bEndpointAddress == 1 && req->req.actual == 31) {
uint8_t *cdb = ((uint8_t *)req->req.buf) + 15;
if (cdb[0] == 0x1A)
Dbg("b1A_%X\n", cdb[2]);
else if (cdb[0] == 0x28 || cdb[0] == 0x2A)
dbg("b%X_&%X_%d\n", cdb[0], 
(cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5], 
(cdb[7] << 8) | cdb[8]);
else
dbg("b%X\n", cdb[0]);
}
#endif
			/* generate zero length IN packet */
			if (req->req.zero && rq->len 
					&& (ep->bEndpointAddress & USB_DIR_IN)
/* added 070716 - class driver does not check this anymore */
					&& !(rq->len % ep->ep.maxpacket))
			{
				rq->len = 0;	/* will not come through here again */
				rt = usbd_ep_io(&udc->hw, ep->bEndpointAddress, rq);
				if (rt < 0)
				{
					dbg("ssl_udc: evt err - q zlp2 %d\n", rt);
				}
				return;
			}

			done(ep, req, 0);

			/* process next request in queue */
			if (!list_empty(&ep->queue))
			{
				req = list_entry(ep->queue.next, udc_req_t, queue);
				rt = usbd_ep_io(&udc->hw, ep->bEndpointAddress, &req->rq);
				if (rt < 0)
				{
					dbg("ssl_udc: evt err - q next %d\n", rt);
				}
			}
		}
	}
	else 
	{
		/* signalling events */
		if (rq == USBD_EV_RST)
		{
			udc_quiesce(udc);
			if (udc->drv->disconnect)
			{
				udc->gdt.speed = USB_SPEED_UNKNOWN;
				spin_unlock(&udc->lock);
				udc->drv->disconnect(&udc->gdt);
				spin_lock(&udc->lock);
			}
			udc->gdt.speed = USB_SPEED_FULL;
			goto l_qsetup;
		}
		else if (rq == USBD_EV_HSPD)
		{
			udc->gdt.speed = USB_SPEED_HIGH;
		}
		else if (rq == USBD_EV_STALL)
		{
			/* protocol stall started - get ready for setup to clear stall */
			goto l_qsetup;
		}
		else if (udc->gdt.speed != USB_SPEED_UNKNOWN)
		{
			void						(*fn)(struct usb_gadget *);
			struct usb_gadget_driver	*drv;

			drv = udc->drv;
			if (rq == USBD_EV_DISC)
			{
				udc_quiesce(udc);
				fn = drv->disconnect;
			}
			else
			{
				fn = rq == USBD_EV_SUSP ? drv->suspend : drv->resume;
			}

			if (fn)
			{
				spin_unlock(&udc->lock);
				fn(&udc->gdt);
				spin_lock(&udc->lock);
			}
		}
	}
}


static irqreturn_t udc_irq(int irq, void *_udc
#if IO_MAP==3
						, struct pt_regs *r)
#else
						)
#endif
{
	udc_p			udc = _udc;
	unsigned long	flags;
	int				rt;

	spin_lock_irqsave(&udc->lock, flags);
	rt = usbd_isr(&udc->hw);
	spin_unlock_irqrestore(&udc->lock, flags);
	return rt ? IRQ_HANDLED : IRQ_NONE;
}


static udc_p	udc;

int usb_gadget_register_driver(struct usb_gadget_driver *drv)
{
	int				rt;
	unsigned long	flags;

	/* basic sanity tests */
	if (!udc)
	{
		return -ENODEV;
	}
	if (!drv || drv->speed == USB_SPEED_UNKNOWN 
			|| !drv->bind || !drv->unbind || !drv->setup)
	{
		return -EINVAL;
	}
	spin_lock_irqsave(&udc->lock, flags);
	if (udc->drv) 
	{
		spin_unlock_irqrestore(&udc->lock, flags);
		return -EBUSY;
	}

	/* hook up the driver */
	drv->driver.bus = 0;
	udc->drv = drv;
	udc->gdt.dev.driver = &drv->driver;
	spin_unlock_irqrestore(&udc->lock, flags);

	rt = drv->bind(&udc->gdt);
	if (rt) 
	{
		udc->gdt.dev.driver = 0;
		udc->drv = 0;
	}
	else
	{
		usbd_connect(&udc->hw, 1);
	}
	return rt;
}


int usb_gadget_unregister_driver (struct usb_gadget_driver *drv)
{
	unsigned long	flags;
	int		status = -ENODEV;

	if (!udc)
	{
		return -ENODEV;
	}
	if (!drv || drv != udc->drv)
	{
		return -EINVAL;
	}

	spin_lock_irqsave(&udc->lock, flags);
	udc_quiesce(udc);
	spin_unlock_irqrestore(&udc->lock, flags);

	drv->unbind(&udc->gdt);
	udc->gdt.dev.driver = 0;
	udc->drv = 0;

	usbd_connect(&udc->hw, 0);
	return status;
}


EXPORT_SYMBOL(usb_gadget_register_driver);
EXPORT_SYMBOL(usb_gadget_unregister_driver);


#ifdef CONFIG_USB_GADGET_DEBUG_FILES

#include <linux/seq_file.h>

static const char proc_filename[] = "driver/udc";

static void proc_ep_show(struct seq_file *s, udc_ep_p ep)
{
	uint16_t	stat_flg;
	udc_req_p	req;
	char		buf[20];

	if (list_empty (&ep->queue))
		seq_printf(s, "\t(queue empty)\n");
	else
	{
		list_for_each_entry (req, &ep->queue, queue) 
		{
			unsigned	length = req->req.actual;

			seq_printf(s, "\treq %p len %d/%d buf %p\n",
					&req->req, length,
					req->req.length, req->req.buf);
		}
	}
}

static int proc_udc_show(struct seq_file *s, void *_)
{
	udc_ep_p		ep;
	unsigned long	flags;

	spin_lock_irqsave(&udc->lock, flags);

	proc_ep_show(s, udc->ep);
	list_for_each_entry (ep, &udc->gdt.ep_list,
			ep.ep_list) 
	{
		if (ep->desc)
		{
			proc_ep_show(s, ep);
		}
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

static int proc_udc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_udc_show, 0);
}

static struct file_operations proc_ops = 
{
	.open		= proc_udc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_proc_file(void)
{
	struct proc_dir_entry *pde;

	pde = create_proc_entry (proc_filename, 0, NULL);
	if (pde)
	{
		pde->proc_fops = &proc_ops;
	}
}

static void remove_proc_file(void)
{
	remove_proc_entry(proc_filename, 0);
}

#else

static inline void create_proc_file(void) {}
static inline void remove_proc_file(void) {}

#endif


/* module PnP */

static void udc_release(struct device *dev)
{
	complete(udc->release);
	kfree (udc);
	udc = 0;
}


static const char	ep_names[30][8] =
{
	"ep1out", "ep1in", "ep2out", "ep2in", 
	"ep3out", "ep3in", "ep4out", "ep4in",
	"ep5out", "ep5in", "ep6out", "ep6in",
	"ep7out", "ep7in", "ep8out", "ep8in",
	"ep9out", "ep9in", "ep10out", "ep10in",
	"ep11out", "ep11in", "ep12out", "ep12in",
	"ep13out", "ep13in", "ep14out", "ep14in",
	"ep15out", "ep15in"
};


static int udc_probe(struct platform_device *dev)
{
	udc_ep_p				ep;
#if IO_MAP == 1
	struct resource			*rc = dev->resource;
#else
	struct resource			*rc = _rc;
#endif
	int						rt;
//	unsigned long			flags;

	/* alloc device context */
	udc = kzalloc (sizeof(*udc), GFP_KERNEL);
	if (!udc)
	{
dbg("ssl_udc: probe err - kmalloc\n");
		return -ENOMEM;
	}

#if IO_MAP == 1
	/* acquire resources */
	if (!request_mem_region(rc[0].start, rc[0].end - rc[0].start + 1, 
			driver_name)) 
	{
dbg("ssl_udc: probe err - req_mem_rgn\n");
		rt = -EBUSY;
		goto l_mem_err;
	}
	udc->hw.r = ioremap_nocache(rc[0].start, rc[0].end - rc[0].start + 1);
	if (!udc->hw.r)
	{
dbg("ssl_udc: probe err - remap\n");
		rt = -EBUSY;
		goto l_map_err;
	}
#else
	udc->hw.r = (void *)rc[0].start;
#endif

	spin_lock_init (&udc->lock);

	rt = request_irq(rc[1].start, udc_irq, IRQF_DISABLED, driver_name, udc);
	if (rt)
	{
dbg("ssl_udc: probe err - req_irq\n");
		goto l_irq_err;
	}

	/* check h/w */
	udc->hw.evt = udc_evt;
	udc->hw.ctx = udc;
	udc->hw.fhspd = (flag & 1) ? 0 : 1;
	udc->hw.fdma = (flag & 2) ? 0 : 1;
	udc->hw.fnak = (flag & 4) ? 0 : 1;
	udc->hw.fpingpong = (flag & 8) ? 0 : 1;
	udc->hw.fphy16 = (flag & 0x10) ? 0 : 1;
	rt = usbd_init(&udc->hw);
	if (rt)
	{
		dbg("ssl_udc: probe err - usbd_init %d\n", rt);
		rt = -ENODEV;
		goto l_hw_err;
	}

	INIT_LIST_HEAD(&udc->gdt.ep_list);
	udc->gdt.ops = &udc_ops;
	udc->gdt.ep0 = &udc->ep[0].ep;
	udc->gdt.name = driver_name;
	udc->gdt.speed = USB_SPEED_UNKNOWN;
	udc->gdt.is_dualspeed = 1;

	device_initialize(&udc->gdt.dev);
	strcpy (udc->gdt.dev.bus_id, "gadget");
	udc->gdt.dev.release = udc_release;
	udc->gdt.dev.parent = &dev->dev;

	/* device/ep0 records init */
	INIT_LIST_HEAD (&udc->gdt.ep_list);
	INIT_LIST_HEAD (&udc->gdt.ep0->ep_list);
	/* basic endpoint records init */
	for (ep = udc->ep, rt = 0; rt < udc->hw.ntds; rt++, ep++)
	{
		if (rt == 1)
		{
			continue;
		}
		if (rt)
		{
			list_add_tail (&ep->ep.ep_list, &udc->gdt.ep_list);
			ep->ep.name = ep_names[rt - 2];
			ep->ep.maxpacket = 1024;
		}
		else
		{
			ep->ep.name = "ep0";
			ep->ep.maxpacket = 64;
		}
		INIT_LIST_HEAD(&ep->queue);
		ep->ep.ops = &udc_ep_ops;
		ep->bEndpointAddress = (rt >> 1) | (rt << 7);
		ep->udc	= udc;
	}
#if USBD_DMA == 1
	if (udc->hw.fdma)
	{
#if IO_MAP == 1
		udc->rq.data = (uint8_t *)dma_map_single(udc->gdt.dev.parent,
				&udc->setup, sizeof(udc->setup), DMA_BIDIRECTIONAL);
#else
		/* copy to/from local memory through EBM for DMA */
		udc->rq.data = ebm_malloc(8);
#endif
	}
	else
#endif
	{
		udc->rq.data = (uint8_t *)&udc->setup;
	}

	create_proc_file();
	rt = device_add(&udc->gdt.dev);
	if (rt)
	{
		goto l_add_err;
	}

	return 0;

l_add_err:
	usbd_exit(&udc->hw);
l_hw_err:
	free_irq(rc[1].start, udc);
l_irq_err:
#if IO_MAP == 1 
	iounmap(udc->hw.r);
l_map_err:
	release_mem_region(rc[0].start, rc[0].end - rc[0].start + 1);
l_mem_err:
#endif
	kfree (udc);
	udc = 0;

	return rt;
}


static int udc_remove(struct platform_device *dev)
{
#if IO_MAP == 1
	struct resource	*rc = dev->resource;
#else
	struct resource	*rc = _rc;
#endif
	DECLARE_COMPLETION_ONSTACK(release);

	if (!udc)
	{
		return -ENODEV;
	}

	usbd_exit(&udc->hw);

#if USBD_DMA == 1
	if (udc->hw.fdma)
	{
#if IO_MAP == 1
		dma_unmap_single(udc->gdt.dev.parent,
				(int)udc->rq.data, sizeof(udc->setup), DMA_BIDIRECTIONAL);
#else
		/* copy to/from local memory through EBM for DMA */
		ebm_mfree(udc->rq.data);
#endif
	}
#endif

	remove_proc_file();

	disable_irq(rc[1].start);
	free_irq(rc[1].start, udc);
#if IO_MAP == 1 
	iounmap(udc->hw.r);
#endif
	release_mem_region(rc[0].start, rc[0].end - rc[0].start + 1);

	udc->release = &release;
	device_unregister(&udc->gdt.dev);
	wait_for_completion(&release);
	return 0;
}


#ifdef CONFIG_PM

static int udc_suspend(struct platform_device *dev, pm_message_t state)
{
dbg("suspend\n");
	udc->gdt.dev.power.power_state = PMSG_SUSPEND;
	udc->gdt.dev.parent->power.power_state = PMSG_SUSPEND;
	usbd_exit(&udc->hw);
	return 0;
}

static int udc_resume(struct platform_device *dev)
{
dbg("resume\n");
	udc->gdt.dev.parent->power.power_state = PMSG_ON;
	udc->gdt.dev.power.power_state = PMSG_ON;
	usbd_init(&udc->hw);
	return 0;
}

#endif


/* module exports */

static struct platform_driver udc_driver = 
{
	.driver =
	{
		.name	= (char *)driver_name,
		.owner	= THIS_MODULE,
	},
	.probe		= udc_probe,
//	.remove		= __exit_p(udc_remove),
	.remove		= udc_remove,
#ifdef CONFIG_PM
	.suspend	= udc_suspend,
	.resume		= udc_resume,
#endif
};


static int __init udc_init(void)
{
#ifdef UDC_SIMPNP
	int	rt;

	_rc[0].start = addr;
	_rc[0].end = addr + 0x3000 - 1;
	_rc[0].flags = IORESOURCE_MEM;
	_rc[1].start = irq;
	_rc[1].end = irq;
	_rc[1].flags = IORESOURCE_IRQ;

	rt = platform_device_register(&dv);
	if (rt)
	{
dbg("ssl_udc: init err - plat_dev_reg\n");
		return rt;
	}
	dbg("addr=%08X irq=%d flag=%08X\n", addr, irq, flag);
#endif
	return platform_driver_register(&udc_driver);
}


static void __exit udc_exit(void)
{
	platform_driver_unregister(&udc_driver);
#ifdef UDC_SIMPNP
	platform_device_unregister(&dv);
#endif
}


module_init(udc_init);
module_exit(udc_exit);

MODULE_AUTHOR("Sasi");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");

