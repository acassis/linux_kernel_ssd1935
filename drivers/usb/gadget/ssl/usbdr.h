#ifndef _USBDR_SASI
#define _USBDR_SASI


#define _USBD_CLSDEV		(0x0c03 << 6)


/* drv configuration */
#define _USBD_POLL	0			/* 1=poll 0=intr */
/* s/w configuration */
#define _USBD_BURST	8			/* 1,4,8,16 */
/* test config */
#define _USBD_TST_SOF	0		/* else=sof count debug, 0=none */
//#define _USBD_TST_SOF	8000	/* else=sof count debug, 0=none */

#define USBD_MEM_MAX	(8 << 10)
#define USBD_TD_MAX		32
/* sanity checking */
#if (_USBD_BURST!=1 && _USBD_BURST!=4  &&_USBD_BURST!=8 && _USBD_BURST!=16)
#error _USBD_BURST must be 1,4,8 or 16
#endif


/* transfer descriptor format in USBD memory */
typedef volatile struct _usbdr_tdp_t
{
	uint32_t	ctl;
	uint32_t	size;
}
usbdr_tdp_t, *usbdr_tdp_p;
typedef volatile struct _usbdr_td_t
{
	uint32_t	ctl;
	uint32_t	size;
	/* additional for h/w with DMA caps */
	uint32_t	adr;
	uint32_t	_rsv;
}
usbdr_td_t, *usbdr_td_p;

/* USBD register map */
typedef volatile struct _usbdr_t
{
	/* ro regs 0x0 */
	uint32_t	id;
	uint32_t	caps;

	/* device control & status - 0x08 */
	uint32_t	ctl;
	uint32_t	rst;
	uint32_t	op;
	uint32_t	sta;
	uint32_t	intr;
	uint32_t	addr;
	uint32_t	frame;
	uint32_t	_rsv_dev[3];

	/* ep control & status - 0x30 */
	uint32_t	rdy;
	uint32_t	ena;
	uint32_t	done;
	uint32_t	stall;
	uint32_t	abort;
	uint32_t	_rsv_ep[3];

	/* ep buffer status - 0x50 */
	uint32_t	xsta;
	uint32_t	ysta;
	uint32_t	xfill;
	uint32_t	yfill;

	uint32_t	_rsv_buf[4];

	/* test register - 0x70 */
	uint32_t	tst;

	uint32_t	_rsv1[(0x800 - 0x74) >> 2];

	/* td config - ep0o->td0, ep1i->td1, ep2o->td2, .. - 0x800 */
	union
	{
		usbdr_td_t	d[USBD_TD_MAX];
		usbdr_tdp_t	p[USBD_TD_MAX];
	}
	td;

	uint32_t	_rsv2[(0x1000 - 0xA00) >> 2];

	/* transfer data buffer (shared with NAK in .tst.VERB mode) */
	uint8_t	buf[USBD_MEM_MAX];	
}
*usbdr_p;


#define USBD_EP2HW(ep)		(((ep) << 1) | ((ep) >> 7))
#define USBD_HW2EP(ep)		((((ep) & 1) << 7) | (((ep) & 15) >> 1))

/* usbdr_p->id */
#define USBD_ID_CLS(r)		((r) >> 16)
#define USBD_ID_DEV(r)		BF_GET(r, 10, 6)
#define USBD_ID_CLSDEV(r)	((r) >> 10)
#define USBD_ID_MAJ(r)		BF_GET(r, 6, 4)
#define USBD_ID_MIN(r)		BF_GET(r, 0, 6)

/* usbdr_p->caps */
#define USBD_CAP_DMA		1
#define USBD_CAP_64			2
#define USBD_CAP_ALIGN		4
#define USBD_CAP_TDS(r)		(BF_GET(r, 3, 5) + 1)
#define USBD_CAP_BUF(r)		(BF_GET(r, 8, 13) + 1)
/* version 1.4 onwards */
#define USBD_CAP_PHY16		(1 << 21)
#define USBD_CAP_PHY8		(1 << 22)

/* usbdr_p->ctl */
#define USBD_CTL_ENA	1
#define USBD_CTL_SLEEP	2

/* usbdr_p->rst */
#define USBD_RST_RST	1

/* usbdr_p->op */
#define USBD_OP_HSPD	1
#define USBD_OP_RSM		2
#define USBD_OP_CONN	4
#define USBD_OP_DMA		0x20
#define USBD_OP_SBUF	0x40
#define USBD_OP_PHY16	0x80
/* burst=1,4,8,16 */
#define USBD_OP_BURST(v)	((((v) > 8 ? 15 : v) >> 2) << 3)
#define USBD_OP_BURST_G(r) \
	(((((r) & 0x18) == 0x18) ? 0x20 : ((r) & 0x18)) >> 1)
#define USBD_OP_ULB		0x100		/* unspec len burst */

/* usbdr_p->sta */
#define USBD_STA_SOF	1
#define USBD_STA_RSM	2
#define USBD_STA_SUSP	4
#define USBD_STA_HSPD	8
#define USBD_STA_RST	0x10
#define USBD_STA_DISC	0x20
#define USBD_STA_BUF	0x100
#define USBD_STA_DONE	0x200
#define USBD_STA_UNRE	0x10000
#define USBD_STA_OVRE	0x20000
#define USBD_STA_RXE	0x40000
#define USBD_STA_SEQE	0x80000
#define USBD_STA_DMAE	0x100000

/* usbdr_p->frame */
#define USBD_FRM_MIC(r)	((r) & 7)
#define USBD_FRM_FRM(r)	BF_GET(r, 3, 11)

/* usbdr_p->rdy .. dmae */
#define USBD_TD_BIT(td)		(1 << (td))

/* usbdr_p->tst */
#define USBD_TST_MODEMASK		7
#define USBD_TST_MODE_OFF		0
#define USBD_TST_MODE_J			1
#define USBD_TST_MODE_K			2
#define USBD_TST_MODE_SE0NAK	3
#define USBD_TST_MODE_PKT		4
#define USBD_TST_VERB			8
#define USBD_TST_VLDM			0x100
#define USBD_TST_VCTL(v)		BF_SET(v, 9, 4)
#define USBD_TST_VSTA(r)		BF_GET(r, 13, 8)

/* usbdr_p->td[].ctl */
#define USBD_TD_XPTR(v)		BF_SET(v, 0, 13)
#define USBD_TD_YPTR(v)		BF_SET(v, 13,13)
#define USBD_TD_DAT(v)		BF_SET(v, 26, 2)	/* v = USBD_TDDAT_x */
#define USBD_TD_DAT_G(v)	BF_GET(v, 26, 2)
#define USBD_TD_NMIC(v)		BF_SET(v, 28, 2)	/* 1-3 */
#define USBD_TD_TYP(v)		((v) << 30)			/* v = USBD_TDTYP_xxx */

#define USBD_TD_DAT_0	0
#define USBD_TD_DAT_1	1
#define USBD_TD_DAT_2	2
#define USBD_TD_DAT_M	3

#define USBD_TD_TYP_CTL	0
#define USBD_TD_TYP_BLK	1
#define USBD_TD_TYP_INT	2
#define USBD_TD_TYP_ISO	3

/* usbdr_p->td[].size */
#define USBD_TDSIZ_XFR(v)	((v) & 0x0007FFFF)
#define USBD_TDSIZ_NPKT(v)	BF_SET((v) - 1, 19, 2)	/* 1-4 */
#define USBD_TDSIZ_PKT(v)	((v) <<  21)


#endif

