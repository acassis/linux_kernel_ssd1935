#include "os.h"

#include "spir.h"
#include "spi.h"
#define SPI_TOUT	5000

#define	SPI_DEF_WW	16
#define MIN(a, b)	((a) < (b)) ? (a) : (b)

/* internal functions */
static void spi_rx(volatile uint32_t *r, char *buf, int count, int inc);
static void spi_tx(volatile uint32_t *r, const char *buf, int count, int inc);


static spi_err spi_reset(spi_p t)
{
	int tout = SPI_TOUT;
	spir_p r;
	
	r = t->r;
	io_wr32(r->ctl, SPI_CTL_ENA | SPI_CTL_RST);
	while (io_rd32(r->ctl) & SPI_CTL_RST)
	{
		if (!tout--)
		{
			dbg("spi: rst err - time out\n");
			io_wr32(r->ctl, 0);
			return SPI_ERR_TOUT;
		}
	}
	io_wr32(r->ctl, 0);
	return SPI_ERR_NONE;
}


/* external functions */

spi_err spi_init(spi_p t)
{
	uint32_t id, cap;
	spir_p	r;
	r = t->r;
	/* check id rister */
	id = io_rd32(r->id);
	if (SPI_ID_CLASS(id) != SPI_PCI_CLASS)
	{
		dbg("spi: init err - id mismatch\n");
		return SPI_ERR_HW;
	}

	/* check cap rister */
	cap = io_rd32(r->cap);
	t->fifosize = SPI_CAP_BUF(cap);

	/* report findings */
	dbg("spi: init info - ver=%d.%d fifo=%d slaves=%d master=%d\n",
		SPI_ID_MAJ(id), SPI_ID_MIN(id), t->fifosize, 
		SPI_CAP_SLAVE(cap) ? SPI_CAP_NSLAVE(cap) + 1 : 0, 
		!!SPI_CAP_MASTER(cap));

	/* reset */
	if (spi_reset(t))
	{
		return SPI_ERR_HW;
	}

#if 0
	if (((SPI_DEF_WW - 1) | SPI_OP_RXEN) != io_rd32(r->op))
	{
		dbg("spi: init err - op reset value %02x\n", io_rd32(r->op));
		return SPI_ERR_HW;
	}
#endif

	return SPI_ERR_NONE;
}


void spi_exit(spi_p t)
{
	spi_reset(t);
}


int  spi_cs(spi_p t, int ena)
{
	spir_p		r;
	uint32_t	v;

	r = t->r;
	v = io_rd32(r->master);
	if (!ena != !(v & SPI_MST_FSS))
	{
//dbg("spi: cs info - %d\n", ena);
		io_wr32(r->master, v ^ SPI_MST_FSS);
	}
	return 0;
}


spi_err spi_cfg(spi_p t, spi_cfg_p c)
{
	uint32_t	op;
	spir_p		r;

	r = t->r;

	/* write op rister */
	op = SPI_OP_CS(c->cs) | SPI_OP_WORD(c->word);
#if 1
	if (c->burst)
	{
		op |= SPI_OP_BURST(c->burst);
	}
#endif
	if (c->lsb_first)
	{
		op |= SPI_OP_LSB;
	}
	if (c->cs_high)
	{
		op |= SPI_OP_CSHIGH;
	}
	if (c->clk_fall)
	{
		op |= SPI_OP_CLKFALL;
	}
	if (c->clk_phase)
	{
		op |= SPI_OP_CLKPHASE;
	}
	if (!t->master)
	{
		io_wr32(r->op, op);
		/* 4 word time */
		io_wr32(r->slave_tout, c->word << 2);
	}
	else
	{
		int	clkdiv, frqdiv;

		io_wr32(r->op, op | SPI_OP_MASTER);
//dbg("spi: cfg info - op = %X, %X\n", r, io_rd32(r->op));

		frqdiv = c->baud;
		if (!frqdiv)
		{
			dbg("spi: cfg err - baud is 0\n");
			return SPI_ERR_PARM;
		}
		frqdiv <<= 2;
		clkdiv = c->per_clk;
		if (frqdiv >= clkdiv)
		{
			clkdiv = 0;
		}
		else
		{
			int	k;

			k = 0;
			do
			{
				clkdiv >>= 1;
				k++;
			}
			while (clkdiv > frqdiv);
			clkdiv = k;
		}

		if (!c->datdly)
		{
			c->datdly = 1;
		}
		if (!c->csdly)
		{
			c->csdly = 1;
		}
		/* preserve chip select */
		io_wr32(r->master, (io_rd32(r->master) & SPI_MST_FSS) | 
			SPI_MST_DATDLY(c->datdly) | SPI_MST_CSDLY(c->csdly) | clkdiv);
	}

	t->inc = (c->word - 1) >> 3;
	if (t->inc > 2)
	{
		t->inc  = 2;
	}

	return SPI_ERR_NONE;
}


static void ddump(char *s, uint8_t *b, int len)
{
	if (!len || !b) return;
	dbg(s);
	while (len--)
	{
		dbg("%02X ", *b); b++;
	}
	dbg("\n");
}

#if 0
void spi_quiet(spi_p t)
{
	spir_p		r;

	r = t->r;
	io_wr32(r->ier, 0);
	io_wr32(r->sta, io_rd32(r->sta));
	io_wr32(r->op, io_rd32(r->op) & ~SPI_OP_EN);
	io_wr32(r->ctl, 0);
}
#endif
int spi_isr(spi_p t)
{
	int			st;
	spir_p		r;
	spi_xfr_p	bp;
	register int	actual, count, rem, inc;

	r = t->r;
	st = io_rd32(r->sta);
	if (!st)
	{
		dbg("spi: isr err - no source\n");
		return 0;
	}
//dbg("spi: isr info - %X %X\n", st, io_rd32(r->ier));
	io_wr32(r->sta, st);

	/* errors */
	if (st & (SPI_INT_WOVR | SPI_INT_WUNR | SPI_INT_FOVR))
	{
		dbg("spi: isr err - %X ard=%d\n", st, r->readcount);
	//	spi_quiet(t);
		io_wr32(r->ier, 0);
		io_wr32(r->op, io_rd32(r->op) & ~SPI_OP_EN);
		io_wr32(r->sta, st);
		io_wr32(r->ctl, 0);
		t->ev(t->ctx, SPI_EV_ERR);
		return 1;
	}

	/* data xfer */
	bp = t->bp;
	if (!bp)
	{
		printk(KERN_ERR"spi: isr err - no buf, %X\n", st);
	//	spi_quiet(t);
//		t->ev(t->ctx, SPI_EV_DONE);
		return 1;
	}
	inc = t->inc;
	actual = bp->actual;
	rem = bp->len - actual;
//	printk("before SPI_INT_TEMT 0x%x 0x%x %d %d %d\n",bp->tx,bp->rx,bp->len,bp->flag,bp->actual);
	if (st & SPI_INT_TEMT)
	{
		/*	use transmitter empty to minimize io & irq for all cases
			except slave rx-only */
		int	size;

		size = t->fifosize << inc;
		count = MIN(size, rem);
		rem -= count;
//	printk("in SPI_INT_TEMT 0x%x 0x%x %d %d %d\n",bp->tx,bp->rx,bp->len,bp->flag,bp->actual);
//		spi_dump_register(t);
		if (bp->rx)
		{
			spi_rx(&r->rx, bp->rx + actual, count, inc);
		}

		actual += count;
		bp->actual = actual;
		if (!rem)
		{
		//	spi_quiet(t);
			io_wr32(r->ier, 0);
			io_wr32(r->op, io_rd32(r->op) & ~SPI_OP_EN);
			io_wr32(r->ctl, 0);
			if (bp->flag & SPI_XFR_NCS)
			{
				io_wr32(r->master, io_rd32(r->master) & ~SPI_MST_FSS);
			}
			t->bp = 0;
//ddump("spi: rx - ", bp->rx, bp->len);
//dbg("spi: isr info - done, %X %X\n", st, io_rd32(r->sta));
			t->ev(t->ctx, SPI_EV_DONE);
		}
		else
		{
			count = MIN(size, rem);
			rem = count >>= t->inc;
			if (bp->tx)
			{
				spi_tx(&r->tx, bp->tx + actual, count, inc);
			}
			else
			{
				while (rem--)
				{
					io_wr32(r->tx, 0);
				}
			}
		}
	}
	else if (st & (SPI_INT_RDR | SPI_INT_TOUT))
	{
		/* slave rx-only */
		union
		{
			uint8_t		*buf;
			uint16_t	*buf2;
			uint32_t	*buf4;
		}
		u;

		u.buf = bp->rx + actual;
		do
		{
			uint32_t	d;

			d = io_rd32(r->rx);
			switch (inc)
			{
				case 0: *u.buf++ = d; break;
				case 1: *u.buf2++ = d; break;
				case 2: *u.buf4++ = d; break;
			}
			rem -= inc;
			actual += inc;
		}
		while (rem && (io_rd32(r->sta) & SPI_INT_RDR));
		bp->actual = actual;
		if (!rem)
		{
			//spi_quiet(t);
			io_wr32(r->ier, 0);
			io_wr32(r->op, io_rd32(r->op) & ~SPI_OP_EN);
			io_wr32(r->ctl, 0);
			t->bp = 0;
//dbg("spi: isr info - done, %X\n", st);
			t->ev(t->ctx, SPI_EV_DONE);
		}
	}

	return 1;
}

int spi_dmawait(spi_p t)
{
	spir_p		r;
	spi_xfr_p	bp;

	bp = t->bp;
	t->bp = 0;
	if (bp->rx)
	{
		return 0;
	}
	r = t->r;
	io_wr32(r->ier, SPI_INT_TEMT);
	return 1;
}

int spi_io(spi_p t, spi_xfr_p bp)
{
	spir_p		r;
	uint32_t	ier;
	int			count;

	bp->actual = 0;
	t->bp = bp;

//ddump("spi: tx - ", bp->tx, bp->len);
	r = t->r;
	io_wr32(r->ctl, SPI_CTL_ENA);
#if 1
	count = bp->tx ? 
		SPI_FIFO_RRST | SPI_FIFO_TRST | SPI_FIFO_RWM | SPI_FIFO_TWM :
		SPI_FIFO_RRST | SPI_FIFO_TRST | SPI_FIFO_RWM;
	io_wr32(r->fifo, count);
#else
	io_wr32(r->fifo, 
		SPI_FIFO_RRST | SPI_FIFO_TRST | SPI_FIFO_RWM | SPI_FIFO_TWM);
#endif

	if (bp->flag & SPI_XFR_DMA)
	{
		io_wr32(r->ier, SPI_INT_FOVR | SPI_INT_WOVR | SPI_INT_WUNR);
#ifdef SPI_AUTORD
		ier = io_rd32(r->master);
		if (!(ier & SPI_MST_AUTORD) == (!bp->tx && bp->rx))
		{
			ier ^= SPI_MST_AUTORD;
			io_wr32(r->master, ier);
		}
		if (ier & SPI_MST_AUTORD)
		{
			io_wr32(r->readcount, bp->len >> t->inc);
		}
#endif
	}
	else
	{
		count = t->fifosize << t->inc;
#ifdef SPI_AUTORD
		io_wr32(r->master, io_rd32(r->master) & ~SPI_MST_AUTORD);
#endif
		
#if 0		
	register unsigned long sp asm ("sp");
	sp_top = (sp|(THREAD_SIZE-1))&~3;
	io_wr32(*(volatile unsigned long *)sp_top, 0xc0c0c0c0);
		
	printk("in spi_io 0x%x 0x%x %d %d %d\n",bp->tx,bp->rx,bp->len,bp->flag,bp->actual);
		spi_dump_register(t);
#endif
		if (bp->tx)
		{
			/* fill up tx fifo */
			ier = SPI_INT_TEMT;
			count = MIN(count, bp->len);
			spi_tx(&r->tx, bp->tx, count, t->inc);
		}
		else
		{
			if (!bp->rx)
			{
				dbg("spi: io err - no buffers\n");
				io_wr32(r->ctl, 0);
				return -1;
			}
			if (t->master)
			{
				/* autoread causes more interrupts - don't use it */
				count = MIN(t->fifosize, bp->len >> t->inc);
				while (count--)
				{
					io_wr32(r->tx, 0);
				}
				ier = SPI_INT_TEMT | SPI_INT_WOVR | SPI_INT_WUNR | SPI_INT_FOVR;
			}
			else
			{
				/* use WM only if size is sufficient */
				ier = (bp->len < (count >> 1)) ?
					SPI_INT_TOUT | SPI_INT_RDR | SPI_INT_WOVR | SPI_INT_FOVR :
					SPI_INT_TOUT | SPI_INT_RDR | SPI_INT_RWM | 
						SPI_INT_WOVR | SPI_INT_FOVR;
			}
		}
		io_wr32(r->ier, ier);
	}

	/* start clocking */
	ier = io_rd32(r->op);
	if (bp->flag & SPI_XFR_CS)
	{
		io_wr32(r->master, io_rd32(r->master) | SPI_MST_FSS);
	}
	if (bp->rx)
	{
		ier |= SPI_OP_RXEN | SPI_OP_EN;
	}
	else
	{
		ier &= ~SPI_OP_RXEN;
		ier |= SPI_OP_EN;
	}
#if 0
dbg("spi: io info - rx=%X tx=%X len=%d flag=%X\n" 
"op=%X->%X mst=%X ctl=%X sta=%X ier=%X arm=%X\n", 
(uint32_t)bp->rx, (uint32_t)bp->tx, bp->len, bp->flag,
io_rd32(r->op), ier, io_rd32(r->master), io_rd32(r->ctl), io_rd32(r->sta), 
io_rd32(r->ier), io_rd32(r->readcount));
#endif
	io_wr32(r->op, ier);
	return 0;
}


static void spi_rx(volatile uint32_t *r, char *buf, int count, int inc)
{
//dbg("rx %d %d\n", count, inc);
	switch (inc)
	{
		case 0:
			while (count--)
			{
				*buf++ = io_rd32(*r);
			}
			break;

		case 1:
		{
			uint16_t	*buf2 = (uint16_t *)buf;

			count >>= 1;
			while (count--)
			{
				*buf2++ = io_rd32(*r); 
			}
			break;
		}

		case 2:
		{
			uint32_t	*buf4 = (uint32_t *)buf;

			count >>= 2;
			while (count--)
			{
				*buf4++ = io_rd32(*r);
			}
			break;
		}
	}
}


static void spi_tx(volatile uint32_t *r, const char *buf, int count, int inc)
{
//dbg("tx %d %d\n", count, inc);
	switch (inc)
	{
		case 0:
			while (count--)
			{
				io_wr32(*r, *buf++);
			}
			break;

		case 1:
		{
			uint16_t	*buf2 = (uint16_t *)buf;

			count >>= 1;
			while (count--)
			{
				io_wr32(*r, *buf2++);
			}
			break;
		}

		case 2:
		{
			uint32_t	*buf4 = (uint32_t *)buf;

			count >>= 2;
			while (count--)
			{
				io_wr32(*r, *buf4++);
			}
			break;
		}
	}
}


