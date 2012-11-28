#include "os.h"

#include "i2cr.h"
#include "i2c.h"

#define I2C_TOUT 20000
#define I2C_HAS_START(cmd)	(!((cmd) & 2))
#define I2C_HAS_END(cmd)	(!((cmd) & 1))


/* internal functions */

static i2c_err i2c_reset(i2c_p t)
{
	int		tout = I2C_TOUT;
	i2cr_p	r;

	r = t->r;
	io_wr32(r->CFGR, I2C_CFGR_RST | I2C_CFGR_EN);
	while (io_rd32(r->CFGR) & I2C_CFGR_RST)
	{
		if (!tout--)
		{
			return I2C_ERR_TOUT;
		}
	}
	io_wr32(r->CFGR, 0);
	return 0;
}


/* external functions */

i2c_err i2c_init(i2c_p t)
{
	i2c_err err;
	uint32_t d;
	i2cr_p reg;

	reg = t->r;
	d = io_rd32(reg->IDR);
	if (((d & I2C_IDR_CLID)>>16) != I2C_PCI_CLASS)
	{
		dbg("i2c: init err - class id mismatch\n");
		return I2C_ERR_HW;
	}
	dbg("i2c: init info - ver=%d,%d\n",
		(d & I2C_IDR_MAJ) >> 6, (d & I2C_IDR_MIN));

	/* reset & check reset status, disable module */
	err = i2c_reset(t);
	if (err)
	{
		dbg("i2c: init err - rst\n");
		return err;
	}

	/* enable interrupts */
	io_wr32(reg->IER, I2C_INT_CMDERR | I2C_INT_LOST | I2C_INT_RDY);

	return 0;
}


void i2c_exit(i2c_p t)
{
	i2c_reset(t);
}


i2c_err i2c_cfg(i2c_p t, i2c_cfg_p c)
{
	i2cr_p	r;

	r = (i2cr_p)t->r;

	t->flag = c->ack ?
		(c->ackhi ? (I2C_OPR_ACK | I2C_OPR_ACKHI) : I2C_OPR_ACK) : 0;
//printk("i2c: cfg info - flag = %X\n", t->flag);

	io_wr32(r->SADDR, c->addr);

	if (c->i2c_clk_hz);
	{
		int		baud;

//		baud = 64 - (c->i2c_clk_hz / (32 * c->per_clk_hz));

		/* the baud equation is the spec. refers to the time period not freq, so inversing the I2C clk & per clk */
		baud = 64 - (c->per_clk_hz / (32 * c->i2c_clk_hz));
		if (baud < 0)
		{
			dbg("i2c: cfg err - baud rate\n");
			return I2C_ERR_PARM;
		}
		io_wr32(r->BRR, baud);
//printk("i2c: cfg info - baud = %X\n", baud);
	}

	return I2C_ERR_NONE;
}


int i2c_isr(i2c_p t)
{
	register int	sta;
	i2cr_p		reg;
	i2c_cmd_p	cmd;

	reg = t->r;
	sta = io_rd32(reg->ISR);
	if (!(sta & io_rd32(reg->IER)))
	{
		dbg("i2c: isr err - no interrupt status\n");
		return -1;
	}
	io_wr32(reg->ISR, sta);

	cmd = t->cmd;

	if (sta & (I2C_INT_CMDERR | I2C_INT_LOST))
	{
		i2c_reset(t);
		dbg("i2c: isr err - sta=%02X\n", sta);
		goto l_done;
	}
	if (sta & I2C_STA_NAK)
	{
		goto l_done;
	}
	if (sta & I2C_INT_RDY)
	{
		register int	r;
		register int	len, actual;

		r = t->flag;
		len = cmd->wr.len;
		actual = cmd->wr.actual;
		if (len && actual < len)
		{
			io_wr32(reg->DATA, cmd->wr.buf[actual]);
			if (len - actual == 1)
			{
				if (I2C_HAS_END(cmd->cmd) && !cmd->rd.len)
				{
					r |= I2C_OPR_CMD_END;
				}
				else
				{
					r |= I2C_OPR_CMD_CONT;
					cmd->rd.actual = -1;
				}
			}
			else
			{
				r |= I2C_OPR_CMD_CONT;
			}
			io_wr32(reg->OPR, r);
			cmd->wr.actual = actual + 1;
		}
		else
		{
			len = cmd->rd.len;
			actual = cmd->rd.actual;
			if (len && actual < len)
			{
				if (actual >= 0)
				{
					cmd->rd.buf[actual] = io_rd32(reg->DATA);
				}
				cmd->rd.actual = actual + 1;
				if (len - actual <= 1)
				{
					/* finish */
					sta = 0;
					goto l_done;
				}
				if (I2C_HAS_END(cmd->cmd) && len - actual == 2)
				{
					r |= I2C_OPR_RX | I2C_OPR_CMD_END;
				}
				else
				{
					r |= I2C_OPR_RX | I2C_OPR_CMD_CONT;
				}
				io_wr32(reg->OPR, r);
			}
			else
			{
				/* finish */
				sta = 0;
				goto l_done;
			}
		}
	}

	return 0;

l_done:
	io_wr32(reg->CFGR, 0);
	cmd->rt = sta;
	t->evt(t->ctx);
	return 0;
}


i2c_err i2c_rw(i2c_p t, i2c_cmd_p cmd)
{
	register int	r;
	register int	len;
	i2cr_p	reg;

	len = cmd->wr.len;
	cmd->wr.actual = 0;
	cmd->rd.actual = -1;
	t->cmd = cmd;

	reg = t->r;
	io_wr32(reg->CFGR, I2C_CFGR_EN);

	r = t->flag;
	if (I2C_HAS_START(cmd->cmd))
	{
		r |= len ? (I2C_OPR_ADDR | I2C_OPR_CMD_START) :
					(I2C_OPR_ADDR | I2C_OPR_CMD_START | I2C_OPR_RX);
	}
	else if (len)
	{
		io_wr32(reg->DATA, *cmd->wr.buf);
		if (I2C_HAS_END(cmd->cmd) && (len == 1))
		{
			r |= I2C_OPR_CMD_END;
		}
		else
		{
			r |= I2C_OPR_CMD_CONT;
		}
		cmd->wr.actual = 1;
	}
	else
	{
		if (I2C_HAS_END(cmd->cmd) && (cmd->rd.len == 1))
		{
			r |= I2C_OPR_RX | I2C_OPR_CMD_END;
		}
		else
		{
			r |= I2C_OPR_RX | I2C_OPR_CMD_CONT;
		}
		cmd->rd.actual = 0;
	}

	io_wr32(reg->OPR, r);

	return I2C_ERR_NONE;
}

