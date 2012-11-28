/*
@file		usbd
@author		sasin@solomon-systech.com
@version	1.01
*/

#ifndef _USBD_SASI
#define _USBD_SASI


/** USBD API returns */
typedef enum
{
	USBD_ERR_NONE	= 0,		/**< successful */
	USBD_ERR_HW		= -1,		/**< hardware error */
	USBD_ERR_PARM	= -2,		/**< parameter error */
	USBD_ERR_MEM	= -3,		/**< insufficient memory */
	USBD_ERR_CFG	= -4,		/**< configuration error */
}
usbd_err;

/** tc.cfg values */
typedef enum 
{
	USBD_EP_CFG_CTL	= 0,		/**< control */
	USBD_EP_CFG_BLK	= 1,		/**< bulk */
	USBD_EP_CFG_INT	= 2,		/**< interrupt */
	USBD_EP_CFG_ISO	= 3,		/**< isochronous */
}
usbd_xfer_cfg;

#define USBD_EP_ADDR(addr, dir_in)	(((addr) << 1) | !!(dir_in))

#define USBD_TC_MAX	32


/** data request */
typedef struct
{
	char	*data;		/**< data logical address */
	int		len;		/**< data length */

	int		actual;		/**< data length transfered */
	void	*ctx;		/**< request context */
}
usbd_rq_t, *usbd_rq_p;

/** opaque transfer context */
typedef struct
{
	uint8_t		adr;	/**< USBD_EP_ADDR(addr, dir_in) */
	uint8_t		cfg;	/**< usbd_xfer_cfg */
	uint16_t	mps;	/**< max pkt size */
	uint16_t	size;	/**< buffer size, npkt * mps */
	uint8_t		npkt;	/**< number of packets in buffer */
	uint8_t		x;		/**< pingpong buffer index toggle */
	uint32_t	ptr;	/**< logical address of current usbd buffer */
	volatile uint32_t	*rem;/**< logical address of usbd transfer count reg */
	usbd_rq_p	rq;		/**< currently processed request */
}
usbd_tc_t, *usbd_tc_p;


/** events in event callback */
#define USBD_EV_DISC	((usbd_rq_p)0)	/**< USB disconnected */
#define USBD_EV_RST		((usbd_rq_p)1)	/**< USB host resetted device */
#define USBD_EV_RSM		((usbd_rq_p)2)	/**< USB host resumed device */
#define USBD_EV_SUSP	((usbd_rq_p)3)	/**< USB host suspended device */
#define USBD_EV_HSPD	((usbd_rq_p)4)	/**< USB host high speed */
#define USBD_EV_STALL	((usbd_rq_p)5)	/**< ep0 stalled */
#define USBD_EV_MAX		USBD_EV_STALL	/**< beyond this it is a packet */

/* USBD context */
typedef struct
{
/** public - initialized by client */
	void		*r;				/* logical register base address */
	void		*ctx;			/* context for evt callback */
	/* event callback, rq - USBD_EV_XXX or packet address */
	void		(*evt)(void *ctx, usbd_rq_p rq);
	/* config flags */
	uint8_t			fdma : 1;		/**< use bus mastering */
	uint8_t			fpingpong : 1;	/**< use pingpong buffers */
	uint8_t			fhspd : 1;		/**< high/full speed */
	uint8_t			fnak : 1;		/**< maintain NAK counters */
	uint8_t			fphy16 : 1;		/**< use 16/8 bit PHY interface */

/** private - zero-d by client on init & opaque after that */
	/** context */
	usbd_tc_t	tc[USBD_TC_MAX];
	uint8_t		ep0st;			/**< stage: 0 - setup 1 - data/status */
	uint16_t	memp;			/**< config memory carving ptr */
	/** version */
	uint8_t		maj;			/**< major version */
	uint8_t		min;			/**< minor version */
	/** capabilities */
	uint8_t		ntds;			/**< # eps in h/w */
	uint8_t		align;			/**< align mask 3=32-bit, 7=64-bit */
	uint8_t		dma : 1;		/**< h/w DMA capable */
	uint16_t	size : 14;		/**< xfer memory size */
}
usbd_t, *usbd_p;


/** Device APIs */


usbd_err  usbd_init(usbd_p t);
/**<
initialize usbd & context
@param[in] t	context
@return			usbd_err
*/

void  usbd_exit(usbd_p t);
/**<
reset usbd & release resources
@param[in] t	context
*/

void  usbd_connect(usbd_p t, int conn);
/**<
(dis)connect (from/)to USB
@param[in] t	context
@param[in] conn	0:disconnect, else connect 
@warning must be protected from interrupts
*/

int  usbd_frame(usbd_p t);
/**<
get current frame number
@param[in] t	context
@return			frame number
*/

int  usbd_mframe(usbd_p t);
/**<
get current microframe number
@param[in] t	context
@return			microframe number
*/

void  usbd_addr(usbd_p t, uint8_t addr);
/**<
set device address
@param[in] t	context
@param[in] addr	USB allocated device address
*/

void  usbd_wakeup(usbd_p t);
/**<
request USB host to resume (>5ms after suspend)
@param[in] t	context
*/

void  usbd_test(usbd_p t, uint8_t mode);
/**<
set compliance test mode
@param[in] t	context
@param[in] mode	USB test modes (1-4)
*/

int  usbd_isr(usbd_p t);
/**<
interrupt service routine to call on interrupt
@param[in] t	context
@return			0 = not processed, else processed
*/


/** EP APIs */


usbd_err  usbd_ep_enable(usbd_p t, void *epdt);
/**<
configure non-zero ep
@param[in] t	context
@param[in] epdt	USB spec endpoint descriptor
@return			USBD_ERR_XXX
*/

void  usbd_ep_disable(usbd_p t, uint8_t ep);
/**<
disable non-zero ep
@param[in] t		context
@param[in] ep		USB spec endpoint address field
*/

usbd_err  usbd_ep_io(usbd_p t, uint8_t ep, usbd_rq_p rq);
/**<
r/w data from/to an ep
@param[in] t	context
@param[in] ep	USB spec endpoint address field
@param[in] rq	data xfer request
@return			usbd_err
@warning		only 1 request can be in transit for any ep
*/

void  usbd_ep_abort(usbd_p t, uint8_t ep);
/**<
abort an ep transaction
@param[in] t	context
@param[in] ep	USB spec endpoint address field
@warning must be protected from interrupts
*/

void  usbd_ep_stall(usbd_p t, uint8_t ep, int stall);
/**<
stall ep
@param[in] t		context
@param[in] ep		USB spec endpoint address field
@param[in] stall	0 = unstall, else stall
@remark				for ep0, starts protocol stall and the client is notified 
					with USBD_EV_STALL to queue for the next setup
*/

void  usbd_ep_pid(usbd_p t, uint8_t ep, int pid);
/**<
set data pid value
@param[in] t	context
@param[in] ep	USB spec endpoint address
@param[in] pid	0,1,2,3 for DATA0,1,2,M
@remark			this is typically used only to initialize control transfers:
					ep0_OUT with 0 for setup & ep0_IN with 1 for data/status
				hardware toggles the pid automatically
*/


#endif

