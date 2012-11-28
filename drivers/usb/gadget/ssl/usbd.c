/*
@file		usbd
@author		sasin@solomon-systech.com
@version	1.01
*/

#include "os.h"

#include "usbd.h"
#include "usbdr.h"

#if USBD_DEBUG
#define dbg(format, arg...) printk(format, ##arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif

//#define USB_NOSLEEP

#pragma pack(1)

/* USB spec endpoint descriptor */
typedef struct _usb_ep_t
{
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;
	uint8_t  bRefresh;
	uint8_t  bSynchAddress;
}
usb_ep_t, *usb_ep_p;

#pragma pack()


static int	usbd_reset(usbdr_p);
static int	usbd_pio(usbd_p, usbd_tc_p, uint32_t, uint32_t);
static void	usbd_pio_evt(usbd_p, uint32_t, uint32_t);
static int	usbd_cfg0(usbd_p);
static void	usbd_td_cfg(usbd_p, usbd_tc_p);
void	tst_sof(usbdr_p, uint32_t);
#if _USBD_TST_SOF
static uint32_t	_sof;
static uint32_t	_frm;
#else
#define tst_sof(r, st)
#endif


/* device based API */

static void usbd_reinit(usbd_p t)
{
	int	i;

	/* in verbose mode, clear NAK area & adjust config memory pointer */
	if (t->fnak)
	{
		volatile uint32_t *	nak;
		uint32_t	d;

		nak = (volatile uint32_t *)((usbdr_p)t->r)->buf;
		d = (t->ntds + 3) >> 2;
		while (d--)
		{
			io_wr32(nak[d], 0);
		}
		t->memp = USBD_TD_MAX;
	}
	else
	{
		t->memp = 0;
	}

	for (i = 0; i < USBD_TC_MAX; i++)
	{
		t->tc[i].rq = 0;
		t->tc[i].x = 0;
		t->tc[i].ptr = 0;
	}
}


usbd_err  usbd_init(usbd_p t)
/*
initialize usbd & context
@param[in] t	context (t->r must be set)
@return USBD_ERR_XXX
*/
{
	usbdr_p	r;
	uint32_t		d;

	r = t->r;

	/* check ID register */
	d = io_rd32(r->id);
	if (USBD_ID_CLSDEV(d) != _USBD_CLSDEV)
	{
		dbg("usb: init err - class/dev id mismatch exp=%08X got=%08X\n", 
			_USBD_CLSDEV, USBD_ID_CLSDEV(d));
		return USBD_ERR_HW;
	}
	t->maj = USBD_ID_MAJ(d);
	t->min = USBD_ID_MIN(d);

	/* check caps register */
	d = io_rd32(r->caps);
	t->ntds = USBD_CAP_TDS(d);
	t->size = USBD_CAP_BUF(d);
	t->align = (d & USBD_CAP_ALIGN) ? 0 : ((d & USBD_CAP_64) ? 7 : 3);
	t->dma = d & USBD_CAP_DMA;

	dbg("usb: init info - ver=%d,%d tds=%d size=%d dma=%d align=%d\n", 
		t->maj, t->min, t->ntds, t->size, t->dma, t->align);

	if (!t->fdma && t->dma)
	{
		dbg("usb: init warn - forcing dma off\n");
	}

	/* enable */
#ifndef USB_NOSLEEP
	io_wr32(r->ctl, USBD_CTL_ENA | USBD_CTL_SLEEP);
#else
	io_wr32(r->ctl, USBD_CTL_ENA);
#endif

	/* reset & check reset values */
	if (usbd_reset(r))
	{
		return USBD_ERR_HW;
	}

	usbd_reinit(t);

	if (t->fphy16 && t->maj == 1 && t->min >= 4 && 
		(d & (USBD_CAP_PHY16 | USBD_CAP_PHY8)) != 
			(USBD_CAP_PHY16 | USBD_CAP_PHY8))
	{
		t->fphy16 = 0;
	}

	/* set operation mode according to user flags */
	d = USBD_OP_BURST(_USBD_BURST);
	if (!t->fpingpong)
	{
		d |= USBD_OP_SBUF;
	}
	if (t->fhspd)
	{
		d |= USBD_OP_HSPD;
	}
	if (t->fphy16)
	{
		d |= USBD_OP_PHY16;
	}
	if (t->fdma)
	{
		d |= USBD_OP_DMA;
	}
	io_wr32(r->op, d);

//	io_wr32(r->intr, USBD_STA_DISC | 
	io_wr32(r->intr, USBD_STA_DMAE | USBD_STA_HSPD 
		| USBD_STA_RST | USBD_STA_RSM | USBD_STA_SUSP 
		| USBD_STA_DONE | USBD_STA_BUF
#if _USBD_TST_SOF
		| USBD_STA_SOF
#endif
	);

	return USBD_ERR_NONE;
}


void  usbd_exit(usbd_p t)
/*
reset usbd
@param[in] t	context
*/
{
	usbd_reset(t->r);
	io_wr32(((usbdr_p)t->r)->ctl, 0);
}


void  usbd_connect(usbd_p t, int conn)
/*
(dis)connect (from)to USB
@param[in] t	context
@param[in] conn	0:disconnect, else connect 
@warning must be protected from interrupts
*/
{
	volatile uint32_t *	r = &((usbdr_p)t->r)->op;
	uint32_t	d;

	d = io_rd32(*r);
	if (conn)
	{
		d |= USBD_OP_CONN;
	}
	else
	{
		d &= ~USBD_OP_CONN;
	}
	io_wr32(*r, d);
}


void  usbd_wakeup(usbd_p t)
/*
request USB host to resume (>5ms after suspend)
@param[in] t	context
@return USBD_ERR_XXX
@warning can be called only >5ms after suspend
*/
{
	volatile uint32_t *	r = &((usbdr_p)t->r)->op;

	io_wr32(*r, USBD_OP_RSM | io_rd32(*r));
}


int  usbd_frame(usbd_p t)
/*
get current frame number
@param[in] t	 context
@return frame number
*/
{
	return io_rd32(((usbdr_p)t->r)->frame) >> 3;
}


int  usbd_mframe(usbd_p t)
/*
get current microframe number
@param[in] t	 context
@return microframe number
*/
{
	return io_rd32(((usbdr_p)t->r)->frame);
}


void  usbd_addr(usbd_p t, uint8_t addr)
/*
set device address
@param[in] t	 context
@param[in] addr	 USB host allocated device address
@warning must be called right after SET_ADDRESS
*/
{
//dbg("usb: addr info - %02X\n", addr);
dbg("&%02X\n", addr);
	io_wr32(((usbdr_p)t->r)->addr, addr);
}


void  usbd_test(usbd_p t, uint8_t mode)
/*
set compliance test mode
@param[in] t	 context
@param[in] mode	 USB spec test mode (1-4)
*/
{
	uint32_t	d;
	volatile uint32_t *	r = &((usbdr_p)t->r)->tst;

	d = io_rd32(*r);
	d &= ~USBD_TST_MODEMASK;
	d |= mode;
	io_wr32(*r, d);
}


/* EP based API */


usbd_err  usbd_ep_enable(usbd_p t, void *_ep)
/*
configure non-zero ep
@param[in] t	context
@param[in] _ep	USB spec endpoint descriptor
@return USBD_ERR_XXX
*/
{
static const uint8_t	_type[4] = 
{
	USBD_TD_TYP_CTL, USBD_TD_TYP_ISO, USBD_TD_TYP_BLK, USBD_TD_TYP_INT
//USBD_TD_TYP_ISO
};
	usb_ep_p	ep = _ep;
	usbd_tc_p	tc;
	uint16_t	mps, size;
	uint8_t		npkt, id;

	/* check endpoint descriptor */
	if (!ep || ep->bLength < 6 || ep->bDescriptorType != 5 
		|| !(ep->bEndpointAddress & 0x7F) || (ep->bmAttributes & 0xC0)
		|| (ep->wMaxPacketSize & 0xE000))
	{
dbg("len=%02X desc=%02X attr=%02X mps=%d\n", 
ep->bLength, ep->bDescriptorType, ep->bmAttributes, ep->wMaxPacketSize);
l_err:
		return USBD_ERR_PARM;
	}
	mps = ep->wMaxPacketSize;
	npkt = mps >> 11;
	mps &= 0x7FF;
	if (mps > 1024)
	{
dbg("bad mps=%d\n", mps);
		goto l_err;
	}
	if ((ep->bmAttributes & 3) != 1 && (ep->bmAttributes > 3 || npkt))
	{
		/* bits reserved for isochronous */
dbg("not iso: attr=%d pkt=%d\n", ep->bmAttributes, npkt);
		goto l_err;
	}

	/* DMA aligned buffer size */
	npkt++;
	size = mps * npkt;
	if (t->align)
	{
		size = (size + t->align) & ~t->align;
	}

	id = ep->bEndpointAddress;
	id = USBD_EP2HW(id);
	tc = t->tc + id;
	tc->adr = id;
	tc->cfg = _type[ep->bmAttributes & 3];
	tc->npkt = npkt;
	tc->size = size;
	tc->mps = mps;
	if (!tc->ptr)
	{
		tc->ptr = t->memp;

		t->memp += (t->fpingpong) ? (size << 1) : size;
		if (t->memp > t->size)
		{
			/* exceeded buffer memory */
			dbg("usb: cfg err - exceeded cfg mem, mps=%d npkt=%d size=%d\n", 
				mps, npkt, size);
			return USBD_ERR_CFG;
		}
#if 0
#warning "padding"
t->memp += 256;
#endif
	}
	else
	{
		/* preconfigured - just reenable */
		tc->ptr -= (uint32_t)((usbdr_p)t->r)->buf;
	}

dbg("+%X\n", id);
	usbd_td_cfg(t, tc);

	return USBD_ERR_NONE;
}


void  usbd_ep_disable(usbd_p t, uint8_t ep)
/*
disable non-zero ep
@param[in] t	context
@param[in] ep	USB spec endpoint address
*/
{
	volatile uint32_t *	r = &((usbdr_p)t->r)->ena;

//dbg("usb: ep_disable info - ep=%02X\n", ep);
	ep = USBD_EP2HW(ep);
dbg("-%X\n", ep);
	ep = USBD_TD_BIT(ep);
	io_wr32(*r, io_rd32(*r) & ~ep);
}


void  usbd_ep_pid(usbd_p t, uint8_t ep, int pid)
/*
set data pid value (0, 1, 2, 3=M)
@param[in] t	context
@param[in] ep	USB spec endpoint address
@param[in] pid	0,1,2,3 for DATA0,1,2,M
*/
{
	usbdr_p	r = (usbdr_p)t->r;
	usbdr_td_p	td;
	uint32_t			tgl;

	ep = USBD_EP2HW(ep);
	td = t->dma ? &r->td.d[ep] : (usbdr_td_p)&r->td.p[ep];
	tgl = io_rd32(td->ctl);
	if (USBD_TD_DAT_G(tgl) != pid)
	{
		tgl &= ~USBD_TD_DAT(3);
		tgl |= USBD_TD_DAT(pid);
		io_wr32(td->ctl, tgl);
	}
}


void  usbd_ep_stall(usbd_p t, uint8_t ep, int stall)
/*
(un)stall ep
@param[in] t		context
@param[in] ep		USB spec endpoint address
@param[in] stall	0:unstall, else stall 
@warning must be protected from interrupts
*/
{
	usbdr_p	r = (usbdr_p)t->r;
	uint32_t		d, v;
	uint8_t		_ep;

//dbg("usb: ep_stall info - ep=%02X stall=%d\n", ep, stall);
	_ep = USBD_EP2HW(ep);
if (stall) dbg("_%X\n", _ep); else dbg("^%X\n", _ep);

	if (!stall)
	{
		/* clear data toggle when stall is cleared */
		usbd_ep_pid(t, ep, 0);
	}

	d = USBD_TD_BIT(_ep);
	v = io_rd32(r->stall);
	if (!stall != !(v & d))
	{
		v ^= d;
		io_wr32(r->stall, v);
	}

	/* ep0 protocol stall - signal done on start of stall */
	if (stall && (_ep  < 2))
	{
		io_wr32(r->rdy, d);
	}
}


void ddump(void *p2, int len)
{
	char	*p = (char *)p2;
	int	i;

	for (i = 0; i < len; i++)
	{
		dbg("%02X ", *p);
		p++;
		if ((i & 15) == 15)
		{
			dbg("\n");
		}
	}
	if (i & 15)
	{
		dbg("\n");
	}
}


usbd_err  usbd_ep_io(usbd_p t, uint8_t ep, usbd_rq_p rq)
/*
r/w data from/to an ep
@param[in] t	context
@param[in] ep	USB spec endpoint address
@param[in] rq	data xfer request
@return USBD_ERR_XXX
*/
{
	usbdr_td_p	td;
	usbd_tc_p	tc;
	uint32_t			d;
	usbdr_p		r = t->r;

	ep = USBD_EP2HW(ep);
	d = USBD_TD_BIT(ep);
	td = t->dma ? &r->td.d[ep] : (usbdr_td_p)&r->td.p[ep];
	tc = &t->tc[ep];
	tc->rq = rq;

	/* set xfer length */
	io_wr32(td->size, USBD_TDSIZ_PKT(tc->mps) | USBD_TDSIZ_XFR(rq->len)
		| USBD_TDSIZ_NPKT(tc->npkt));

//dbg("<%X\n", ep);
//if (ep > 1) dbg("<%X\n", ep);
//dbg("<%X_L%d_T%d_X%d\n", ep, rq->len, 
//	USBD_TD_DAT_G(io_rd32(td->ctl)), tc->x); 
//dbg("usb: ep_io info - ep=%d len=%d tgl=%d xsel=%d\n",
//ep, rq->len, USBD_TD_DAT_G(io_rd32(td->ctl)), tc->x);

	/* set dma address */
	if (t->fdma)
	{
		io_wr32(td->adr, (uint32_t)rq->data);
	}

	/* set ready */
	rq->actual = 0;
	io_wr32(r->rdy, d);

	return USBD_ERR_NONE;
}


void  usbd_ep_abort(usbd_p t, uint8_t ep)
/*
abort an ep transaction
@param[in] t	context
@param[in] ep	USB spec endpoint address
@warning must be protected from interrupts
*/
{
	usbd_tc_p	tc;

	ep = USBD_EP2HW(ep);
	tc = t->tc + ep;
	if (tc->rq)
	{
		uint32_t	d;
		int	tout = 5000;
		volatile uint32_t * r = &((usbdr_p)t->r)->abort;

		tc->rq = 0;
		d = USBD_TD_BIT(ep);
		io_wr32(*r, d);
//dbg("usb: ep_abort info - abort=%08X d=%08X\n", io_rd32(*r), d);
dbg("x%X\n", ep);
		tc->x = 0;
		while (io_rd32(*r) & d)
		{
			tout--;
					
			if (!tout)
			{
				dbg("usb: ep%d abort tout\n", ep);
				break;
			}
		}
	}
}


int  usbd_isr(usbd_p t)
{
	usbdr_p	r;
	uint32_t	st, d, x, y;	/* status, done, xbuf, ybuf */

	r = t->r;
	st = io_rd32(r->sta);
	st &= io_rd32(r->intr);
	if (!st)
	{
		dbg("usb: isr err - no sta\n"); 
		return 0;
	}
//dbg("usb: isr info - sta=%08X\n", st);

	/* clear done & buffer statuses */
	if (st & USBD_STA_DONE)
	{
		d = io_rd32(r->done);
		if (d)
		{
//printk("done=%08X\n", d);
			io_wr32(r->done, d);
		}
		else
		{
			dbg("usb: isr err - done irq none\n");
		}
	}
	else
	{
		d = 0;
	}
	if (st & USBD_STA_BUF)
	{
		/* PIO only - buffer fill/empty status */
		x = io_rd32(r->xsta);
		y = io_rd32(r->ysta);
		if (!(x | y))
		{
			dbg("usb: isr err - buf irq none\n");
		}
		else
		{
			uint32_t	x0, y0;

			/* loop till no more changes in buffer status */
			do
			{
				x0 = x;
				y0 = y;
				x = io_rd32(r->xsta);
				y = io_rd32(r->ysta);
			}
			while ((x ^ x0) || (y ^ y0));
//dbg("xsta=%08X ysta=%08X rdy=%08X\n", x, y, io_rd32(r->rdy));
			io_wr32(r->xsta, x);
			io_wr32(r->ysta, y);
		}
	}
	else
	{
		x = 0;
		y = 0;
	}

	/* clear main status & hence irq */
	io_wr32(r->sta, st);
//dbg("usb: isr info - post-sta=%08X\n", io_rd32(r->sta));

#if _USBD_TST_SOF
	/* testing only - occurs every 1ms or 125us for FS/HS */
	if (st & USBD_STA_SOF)
	{
		_sof++;
		if (_sof == _USBD_TST_SOF + 1)
		{
			uint32_t	frm;

			frm = io_rd32(r->frame);
			if (frm - _frm != _USBD_TST_SOF)
			{
				dbg("soferr\n");
			}
			dbg("usb: sof %d: %08X %08X\n", _USBD_TST_SOF, frm - _frm, frm);
			_sof = 0;
			_frm = io_rd32(r->frame);
		}
	}
#endif

	/* device-based */
	if (st & USBD_STA_DISC)
	{
		/* disconnect - enter sleep, client will free pkts */
//		dbg("usb: isr info - disconnect\n");
dbg("d#");
		t->evt(t->ctx, USBD_EV_DISC);
#ifndef USB_NOSLEEP
		io_wr32(r->ctl, USBD_CTL_ENA | USBD_CTL_SLEEP);
#endif
	}
	if (st & USBD_STA_RST)
	{
		/* reset - configure ep0 & enable USBD */
//		dbg("usb: isr info - reset\n");
#if _USBD_TST_SOF
		_sof = _frm = 0;
#endif
dbg("x#");
		usbd_reinit(t);
		usbd_cfg0(t);
#ifndef USB_NOSLEEP
		io_wr32(r->ctl, USBD_CTL_ENA);
#endif
		t->evt(t->ctx, USBD_EV_RST);
	}
	if (st & USBD_STA_SUSP)
	{
		/* suspend - enter sleep */
//		dbg("usb: isr info - suspend\n");
dbg("s#");
#ifndef USB_NOSLEEP
		io_wr32(r->ctl, USBD_CTL_ENA | USBD_CTL_SLEEP);
#endif
		t->evt(t->ctx, USBD_EV_SUSP);
	}
	if (st & USBD_STA_RSM)
	{
		uint32_t	rsm;

		/* resume - exit sleep & clear wakeup bit if set */
//		dbg("usb: isr info - resume\n");
dbg("r#");
		rsm = io_rd32(r->op); 
		if (rsm & USBD_OP_RSM)
		{
			io_wr32(r->op, ~USBD_OP_RSM & rsm);
		}
#ifndef USB_NOSLEEP
		io_wr32(r->ctl, USBD_CTL_ENA);
#endif
		t->evt(t->ctx, USBD_EV_RSM);
	}
	if (st & USBD_STA_HSPD)
	{
dbg("h#");
		t->evt(t->ctx, USBD_EV_HSPD);
	}

	/* ep-based status */
	if (st & USBD_STA_BUF)
	{
		/* PIO only - buffer fill/empty status */
//dbg("b#");
		usbd_pio_evt(t, x, y);
	}
	if (d)
	{
		int	i;

		/* xfer complete */
		for (i = 0; d; i++, d >>= 1)
		{
			usbd_tc_p	tc;
			usbd_rq_p	rq;

			if (!(d & 1))
			{
				continue;
			}

			tc = t->tc + i;
			rq = tc->rq;
			if (!rq)
			{
				if (i < 2)
				{
					uint32_t	stall;

					stall = io_rd32(r->stall);
					if ((1 << i) & stall)
					{
						/* ep0 protocol stall applied - clear stall bit */
						io_wr32(r->stall, stall & ~(1 << i));
dbg("p#");
						t->evt(t->ctx, USBD_EV_STALL);
						continue;
					}
				}
				dbg("usb: isr err - no rq for done\n");
				continue;
			}

			tc->rq = 0;
			rq->actual = rq->len;
			if (!(i & 1))
			{
				/* out pipe - read */
				rq->actual -= USBD_TDSIZ_XFR(io_rd32(*tc->rem));
			}
//dbg(">%X\n", i);
//if (!t->fdma && rq->actual) {dbg("ep%d: ", i); ddump(rq->data, rq->actual);}
//if (i > 1) dbg(">%X\n", i);
//dbg("<1>!%X_L%d\n", i, rq->actual);
//dbg("usb: isr info - ep=%d len=%d\n", i, rq->actual);
			t->evt(t->ctx, rq);
		}
	}

	/* error */
	if (st & USBD_STA_DMAE)
	{
		/* list out DMA addresses of readied EPs */
		usbdr_td_p	td = r->td.d;
		int	i;

		dbg("usb: isr err - dma");
		for (i = 0, d = 0; d < t->ntds; d++, td++)
		{
			if (r->rdy & (1 << d))
			{
				if (!(i & 3))
				{
					dbg("\n");
				}
				i++;
				dbg("%02d:%08X ", d, td->adr);
			}
		}
	}

	return 1;
}


/* Internal routines */


static void  usbd_td_cfg(usbd_p t, usbd_tc_p tc)
{
	usbdr_p		r = t->r;
	usbdr_td_p	td;
	uint8_t		adr;
	uint32_t	ctl;

//dbg("#%X c%d s%d p%d\n", tc->adr, tc->cfg, tc->size, tc->ptr);
	adr = tc->adr;
	td = (t->dma) ?  &r->td.d[adr] : (usbdr_td_p)&r->td.p[adr];
	ctl = USBD_TD_TYP(tc->cfg) | USBD_TD_XPTR(tc->ptr)
#if 0
#warning "padding"
			| USBD_TD_YPTR(tc->ptr + tc->size + 128) | USBD_TD_NMIC(tc->npkt);
#else
			| USBD_TD_YPTR(tc->ptr + tc->size) | USBD_TD_NMIC(tc->npkt);
#endif
	io_wr32(td->ctl, ctl);
	io_wr32(td->size, USBD_TDSIZ_PKT(tc->mps) | USBD_TDSIZ_XFR(tc->mps)
		| USBD_TDSIZ_NPKT(tc->npkt));

	tc->rem = (volatile uint32_t *)&td->size;
	tc->ptr += (uint32_t)r->buf;

	/* enable ep */
	io_wr32(r->ena, io_rd32(r->ena) | (1 << adr));
}


static int  usbd_cfg0(usbd_p t)
{
	usbd_tc_p	tc;

	tc = t->tc;

	/* setup ep0 out */
	tc->mps = tc->size = 64;
	tc->npkt = 1;
	tc->ptr = t->memp;

	/* setup ep0 in */
	tc[1] = tc[0];
	tc++;
	tc->adr = 1;
	if (t->fpingpong)
	{
		tc->ptr += tc->size << 1;
		t->memp += tc->size << 2;
#if 0
#warning "padding"
tc->ptr += 128;
t->memp += 256;
#endif
	}
	else
	{
		tc->ptr = tc->size;
		t->memp += tc->size << 1;
#if 0
#warning "padding"
tc->ptr += 128;
t->memp += 128;
#endif
	}

	/* setup td h/w */
	usbd_td_cfg(t, tc - 1);
	usbd_td_cfg(t, tc);
	return 0;
}


static int  usbd_pio(usbd_p t, usbd_tc_p tc, uint32_t x, uint32_t y)
/*
perform programmed IO on buffers 
@param[in] t context
@param[in] tc transfer configuration
@param[in] x,y ganged buffer status
@return USBD_ERR_XXX
*/
{
	usbd_rq_p	rq;
	uint8_t		*rbuf;
	uint8_t		*data;
	int			len;
	int			sz;
	uint8_t		wr;
	int			rt;

	rq = tc->rq;
	if (!rq)
	{
		dbg("usb: pio err - no rq\n");
		return 0;
	}
	len = rq->actual;
	data = rq->data + len;
	len = rq->len - len;
	if (!len)
	{
		return 0;
	}
	wr = tc->adr & 1;
	sz = tc->size;

	if (!t->fpingpong)
	{
		/* get X ptr & length */
		rt = 1;
		rbuf = (uint8_t *)tc->ptr;
		if (len > sz)
		{
			len = sz;
		}
		/* r/w X buffer */
		if (!wr)
		{
#if IO_MAP == 1
			memcpy(data, rbuf, len);
#else
			io_rds(rbuf, data, len);
#endif
		}
		else
		{
#if IO_MAP == 1
			memcpy(rbuf, data, len);
#else
			io_wrs(rbuf, data, len);
#endif
		}
	}
	else
	{
		uint8_t	*rbuf2;
		int		len2;

		/* toggle pingpong index */
		tc->x ^= 1;

		/* set cur_ptr=x/y, next_ptr=y/x, y=next_buf_stat */
		if (!tc->x)
		{
			rbuf2 = (uint8_t *)tc->ptr;
			rbuf = rbuf2 + sz;
#if 0
#warning "padding"
rbuf += 128;
#endif
			y = x;
			rt = 2;
		}
		else
		{
			rbuf = (uint8_t *)tc->ptr;
			rbuf2 = rbuf + sz;
#if 0
#warning "padding"
rbuf2 += 128;
#endif
			rt = 1;
		}

		/* get cur_len, next_len */
		len2 = 0;
		if (len > sz)
		{
			if (y & USBD_TD_BIT(tc->adr))
			{
				/* 2nd buffer needs servicing as well */
				rt = 3;
				len2 = len - sz;
				if (len2 > sz)
				{
					len2 = sz;
				}
			}
			len = sz;
		}

		/* r/w X/Y buffer */
		if (!wr)
		{
#if IO_MAP == 1
			memcpy(data, rbuf, len);
#else
			io_rds(rbuf, data, len);
#endif
		}
		else
		{
#if IO_MAP == 1
			memcpy(rbuf, data, len);
#else
			io_wrs(rbuf, data, len);
#endif
		}

		/* service 2nd pingpong buffer */
		if (len2)
		{
			tc->x ^= 1;
			data += len;
			len += len2;
			if (!wr)
			{
#if IO_MAP == 1
				memcpy(data, rbuf2, len);
#else
				io_rds(rbuf2, data, len2);
#endif
			}
			else
			{
#if IO_MAP == 1
				memcpy(rbuf2, data, len);
#else
				io_wrs(rbuf2, data, len2);
#endif
			}
		}
	}

	rq->actual += len;
	return rt;
}


static void  usbd_pio_evt(usbd_p t, uint32_t xsta, uint32_t ysta)
/*
handle fifo r/w events for all tds
@param[in] t	context
@param[in] mask	td service mask
*/
{
	int			i;
	uint32_t			mask;
	int			rt;
	usbdr_p		r = t->r;
	usbd_tc_p	tc = t->tc;

	mask = xsta | ysta;
	for (i = 0; mask; i++, mask >>= 1)
	{
		if (mask & 1)
		{
			rt = usbd_pio(t, tc + i, xsta, ysta);
			if (!(rt & 1))
			{
				xsta &= ~(1 << i);
			}
			if (!(rt & 2))
			{
				ysta &= ~(1 << i);
			}
		}
	}
//if (xsta | ysta) dbg("usb: pio_evt info - set ");
	if (xsta)
	{
		io_wr32(r->xfill, xsta);
//dbg("xfill=%08X ", xsta);
	}
	if (ysta)
	{
		io_wr32(r->yfill, ysta);
//dbg("yfill=%08X", ysta);
	}
//if (xsta | ysta) dbg("\n");
}


#define USBD_TOUT_5u	(133 * 5) /* should be max_AHB_MHz * 5us */

static int  usbd_reset(usbdr_p r)
/*
reset usbd
@param[in] r register base logical address
*/
{
	uint32_t	tout;

//dbg("reset\n");
	io_wr32(r->rst, USBD_RST_RST);
	/* tout for 5 micros */
	for (tout = 0;  tout < USBD_TOUT_5u;  tout++)
	{
		if (io_rd32(r->rst) != USBD_RST_RST)
		{
			dbg("usbd: rst err - autocleared\n");
			return -1;
		}
		tout++;
	}
	io_wr32(r->rst, 0);
	if (io_rd32(r->rst))
	{
		dbg("usbd: rst err - not cleared\n");
		return -1;
	}
	return 0;
}

