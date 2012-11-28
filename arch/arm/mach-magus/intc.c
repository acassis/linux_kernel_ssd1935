#include "os.h"
#include "intc.h"
#include "intcr.h"


int  intc_init(void *reg)
{
	intc_reg_t	*r = reg;
	uint32_t	d;
	int			tout;

	/* check ID register */
	d = io_rd32(r->id);
	if (INTC_ID_CLSDEV(d) != _INTC_CLSDEV)
	{
		dbg("intc: init err - class/dev id mismatch "
			"exp=%08X got=%08X d=%08X\n", 
			_INTC_CLSDEV, INTC_ID_CLSDEV(d), d);
		return -1;
	}
	dbg("intc: init info - ver=%d,%d\n", INTC_ID_MAJ(d), INTC_ID_MIN(d));

	/* enable & reset */
	io_wr32(r->ctl, INTC_CTL_ENA | INTC_CTL_RST);
	tout = 0x1000;
	while (tout--) 
	{
		d = io_rd32(r->ctl);
		if (d == INTC_CTL_ENA)
		{
			break;
		}
		if (d != (INTC_CTL_ENA | INTC_CTL_RST))
		{
dbg("intc: init err - ctl reset value %08X\n", d);
			return -1;
		}
	}
	if (!tout)
	{
dbg("intc: init err - reset timeout\n");
			return -1;
	}
	return 0;
}


void  intc_exit(void *reg)
{
	intc_reg_t	*r = reg;

	io_wr32(r->ctl, INTC_CTL_ENA | INTC_CTL_RST);
}


void  intc_unmask(void *reg, int intr)
{
	intc_reg_t	*r = reg;

	io_wr32(r->unmask, intr);
}


void  intc_mask(void *reg, int intr)
{
	intc_reg_t	*r = reg;

	io_wr32(r->mask, intr);
}


int  intc_pend(void *reg)
{
	intc_reg_t	*r = reg;

	return io_rd32(r->pend);
}


void  intc_ack(void *reg, int intr)
{
	intc_reg_t			*r = reg;
	volatile uint32_t	*sta;

	if (intr > 63)
	{
		return;
	}
	if (intr > 31)
	{
		intr -= 32;
		sta = &r->sta[1];
	}
	else
	{
		sta = r->sta;
	}
	io_wr32(*sta, 1 << intr);
}

