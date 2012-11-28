#include "os.h"

#include "nfcr.h"
#include "nfc.h"



#define NFC_TOUT	5000 

static nfc_err nfc_reset(nfc_p t)
{
	int tout = NFC_TOUT; 
	nfcr_p	r; 

	r = t->r;
	io_wr32(r->CFG, NFC_CFG_RST | NFC_CFG_EN);
	while (tout--)
	{
		if ((io_rd32(r->CFG) & (NFC_CFG_RST | NFC_CFG_EN)) == NFC_CFG_EN)
		{
			//io_wr32(r->CFG, 0);
			return NFC_ERR_NONE;
		}
	}
	dbg("nfc: reset err - timeout\n");
	return NFC_ERR_TOUT;
}


static void nfc_set_addr(nfc_p t, int col, int pg, int nraddr)
{
	nfcr_p		r;
	uint32_t	l, h = 0;

	r = t->r;

	l = (pg << t->pgshift) | col;
	h = (pg >> (32 - t->pgshift)) & 0xff;
	if (nraddr == 1)
	{
		io_wr32(r->ADDR1, l);
		io_wr32(r->HADDR1, h);
	}
	else
	{
		io_wr32(r->ADDR2, l);
		io_wr32(r->HADDR2, h);
	}
}


nfc_err nfc_setecc(nfc_p t)
{
	uint32_t	d = 0;
	nfcr_p		r;

	if (t->eccmode && t->eccmode != 2 && t->eccmode != 3)
	{
		dbg("nfc: setecc - invalid ecc mode%d\n",t->eccmode);
		return NFC_ERR_PARM;
	}

	if (t->eccmode && t->eccofs)
	{
		dbg("nfc: setecc err - invalid ofs%d mode%d\n", t->eccofs, t->eccmode);
		return NFC_ERR_PARM;
	}

	if (t->buswidth == 1 && (t->eccofs & 1))
	{
		dbg("nfc: setecc err - odd offset%d for 16 bit\n",t->eccofs);
		return NFC_ERR_PARM;
	}

	r = t->r;
	/* config ecc mode */
	d = (t->eccmode << 16) | (t->eccofs << 4);
	io_wr32(r->ECTRL, d);
	d = 0;
	d = io_rd32(r->ECTRL);
	if (t->eccmode == 2)
	{
		io_wr32(r->TECC4, (t->eccctrl << 3) | NFC_TECC4_RST);
	}
	else if (t->eccmode == 3)
	{
		if (t->eccctrl > 0x20)
		{
			dbg("nfc: setecc err - tecc8 REDN value %d\n",t->eccctrl);
			return NFC_ERR_PARM;
		}
		io_wr32(r->TECC8, NFC_TECC8_RST | (t->eccctrl << 8));
	}
	return NFC_ERR_NONE;
}


nfc_err nfc_init(nfc_p t)
{
	uint32_t	d;
	nfcr_p		r;
	int			rt;

	r = t->r;

	/* check id register */
	d = io_rd32(r->ID);
	if (NFC_ID_CLID(d) != NFC_PCI_CLASS)
	{
		dbg("nfc: init err - id mismatch\n");
		return NFC_ERR_HW;
	}
	t->ver = (NFC_ID_MAJ(d) << 8) | NFC_ID_MIN(d);

	/* check cap register */
	d = io_rd32(r->CAP);
	dbg("nfc: init info - ver=%X buf=%d\n", t->ver,  
		(1 << ((d & 3) + 10)) + (1 << ((d & 3) + 5)));

	rt = nfc_reset(t);
	if (rt)
	{
		return rt;
	}

	
	//io_wr32(r->IER, NFC_IER_ERR | NFC_IER_CMP);	
	io_wr32(r->IER, NFC_IER_CMP);	
	
	//system run on 240M
	//APB clk = 60M, i.e 16.7 ns	
	//io_wr32(r->CFGR2, 0x33031101);
	//case many ecc correction !!
	//io_wr32(r->CFGR2, 0x11111066);	
	
	
	//Samsung, Toshiba MLC ok
	io_wr32(r->CFGR2, 0x11031066);	
	return 0;
}


void nfc_exit(nfc_p t)
{
	nfc_reset(t);
}


void nfc_cfg(nfc_p t, nfc_cfg_p c)
{
	nfcr_p		r;

	r = t->r;
	t->buswidth = c->buswidth;
	t->pgsz = c->pgsz;
	t->pgshift = c->colsz + 1;
	io_wr32(r->CFG, (c->rowsz << 24) | (c->colsz << 16) | (c->blksz << 12) |
		(c->buswidth << 11) | (c->pgsz << 8) | NFC_CFG_SEQ | NFC_CFG_EN);
}


#if 0
void nfc_cfg2(nfc_p t, nfc_cfg_p c)
{
	uint32_t	d = 0;
	nfcr_p	r;

	r = t->r;
	d = (c->tweh << 28) | (c->twel << 24) | (c->treh << 20) |
		(c->trel << 16) | (c->taclh << 12) | (c->tacls << 8) |
		(c->trwsp << 4) | c->twebusy;
#if 0
// for toshiba chip
//d = 0x445521ee;
d = 0xffffffff;
//d = 0x111233ee;
#else
d = 0x211313ee;	/* toshiba 4k */
#endif
	io_wr32(r->CFGR2, d);
}
#endif


int nfc_isr(nfc_p t)
{
	register uint32_t	st;
	nfcr_p				r;

	r = t->r;
	st = io_rd32(r->ISR);
		
	//only handle Command sequence error interrup &
	//Command completed interrupt
	if (!(st & (NFC_ISR_ERR | NFC_ISR_CMP)))		
	{
		NAND_DBG_ERR("nfc: isr err - no interrupt %X\n", st);
		return 0;
	}
	
	io_wr32(r->ISR, st & 0xFF);	/* clear ECC status bits as well */
	//dbg("nfc: isr info - %X", st);

	if (st & NFC_ISR_ERR)
	{
		//Command sequence error
		NAND_DBG_ERR("nfc: isr err - %X", st);
		st = NFC_EVT_ERR;
	}
	else if (st & NFC_ISR_ECCERR)		
	{		
		st = NFC_EVT_ECC;	
		//printk(KERN_ERR "nfc: ecc seems error \n");
	}	
	else if (st & NFC_ISR_CMP)			
	{				
		//command is completed
		uint32_t	fix, err, ecc;			
		
		ecc = st >> 8;
		err = fix = 0;
		
		//check bit 8 to bit 31
		while (ecc)
		{
			switch (ecc & 3)
			{
				case 1: fix++; break;
				case 2: case 3: err++; break;
			}
			ecc >>= 2;
		}
			
				
		//fix = (err << 16) | (fix << 8);		
			
		//check max ECC corection count
		if(fix)
		{		
			uint32_t errcnt,i,max_errcnt;
			int loop;
						
			fix = (fix << 8);	//no. of fix per page, to return out
			fix |= NFC_EVT_ECC_CORRECTED;				
			
			//for checking no. of ecc used
			ecc = st >> 8;			
			
			max_errcnt = 0;		
			if(t->eccmode == 3)			//8 bit ecc
			{	
				errcnt = io_rd32(r->TECC8R);
				for(i=0;i<8;i++)			
				{
					if ((errcnt & 0x07) > max_errcnt)
						max_errcnt	= (errcnt & 0x07);
					errcnt >>= 4;
				}							
			}	
			else						//4 bit ecc
			{	
				errcnt = io_rd32(r->TECC4R);
				
				//4Kpage
				if(t->pgsz==3)	
					loop=8;
				else
					loop=4;
				
				
				for(i=0;i<loop;i++)			
				{					
					if(ecc&0x3)	//check which 512 has ecc fix, only necessary for case of 4bit ecc
					{
						if ((errcnt & 0x03) > max_errcnt)
							max_errcnt	= (errcnt & 0x03);					
					}					
					ecc >>= 2;
					errcnt >>= 4;
				}							
				max_errcnt+=1;	//0 means 1bit error!
			}		
			
			//store max ecc bit per 518 bytes
			fix |= (max_errcnt << 16);
		}
		st  = NFC_EVT_CMP | fix;
	}
	else
	{
		NAND_DBG_ERR("nfc: isr warn - %X", st);
		//printk(KERN_DEBUG "nfc_isr status 0x%08x\n",st);
	}

	//	io_wr32(r->CFG, io_rd32(r->CFG) & ~NFC_CFG_EN);
	t->evt(t->ctx, st);
	return 1;
}


void nfc_write(nfc_p t, const char *buf, int ofs, int len)
/*
NFC buffer can only be accessed in 32/64 bit
*/
{
	uint32_t	d;
//	uint32_t	cfg;
	uint32_t	*ibuf;
	nfcr_p		r = t->r;

//	cfg = io_rd32(r->CFG);
//	io_wr32(r->CFG, cfg | NFC_CFG_EN);

	ibuf = ((uint32_t *)r->BUF) + (ofs >> 2);
	ofs &= 3;

	/* write initial non-32 bit aligned data - read modify */
	if (ofs)
	{
		int	ilen;

		ilen = 4 - ofs;
		if (ilen > len)
		{
			ilen = len;
		}
		d = io_rd32(*ibuf);
		memcpy(((char *)&d) + ofs, buf, ilen);
		io_wr32(*ibuf, d);
		ibuf++;
		buf += ilen;
		len -= ilen;
	}

	/* handle 32-bit transfers */
	if (len >= 4)
	{
		ofs = (int)buf & 3;
		if (!ofs)	/* source & destination aligned */
		{
#if 1
			memcpy(ibuf, buf, len & ~3);	/* use Linux ARM bursting */
			buf += len & ~3;
			ibuf += len >> 2;
			len &= 3;
#else
			do
			{
				io_wr32(*ibuf, *((uint32_t *)buf));
				ibuf++;
				buf += 4;
				len -= 4;
			}
			while (len >= 4)
#endif
		}
		else
		{
			/* write middle data 32-bit destination aligned */
			do
			{
				memcpy(&d, buf, 4);
				io_wr32(*ibuf, d);
				ibuf++;
				buf += 4;
				len -= 4;
			}
			while (len >= 4);
		}
	}

	/* write final non 32-bit data - read-modify */
	if (len)
	{
		d = io_rd32(*ibuf);
		memcpy(&d, buf, len);
		io_wr32(*ibuf, d);
	}

//	io_wr32(r->CFG, cfg);
}



//because mlc always allign page
void nfc_write_mlc(nfc_p t, const char *buf, int ofs, int len)
{	
	uint32_t	*ibuf;
	nfcr_p		r = t->r;
	
	ibuf = ((uint32_t *)r->BUF) + (ofs >> 2);
	memcpy(ibuf, buf, len);		
}

void nfc_read(nfc_p t, char *buf, int ofs, int len)
/*
NFC buffer can only be accessed in 32/64 bit
*/
{
	uint32_t	d;
//	uint32_t	cfg;
	uint32_t	*ibuf;
	nfcr_p		r = t->r;

//	cfg = io_rd32(r->CFG);
//	io_wr32(r->CFG, cfg | NFC_CFG_EN);

//dbg("nfc: read info - buf=x%X ofs=%d len=%d\n", buf, ofs, len);
	ibuf = ((uint32_t *)r->BUF) + (ofs >> 2);
	ofs &= 3;

	/* read initial non-32 bit NFC aligned data */
	if (ofs)
	{
		int	ilen;

		ilen = 4 - ofs;
		if (ilen > len)
		{
			ilen = len;
		}
		d = io_rd32(*ibuf);
		memcpy(buf, ((char *)&d) + ofs, ilen);
		ibuf++;
		buf += ilen;
		len -= ilen;
	}

	/* handle 32-bit transfers */
	if (len >= 4)
	{
//dbg("nfc: read info - mid buf=x%X ofs=%d len=%d\n", buf, ofs, len);
		ofs = (int)buf & 3;
		if (!ofs)
		{
#if 1
			memcpy(buf, ibuf, len & ~3);	/* use Linux ARM bursting */
			buf += len & ~3;
			ibuf += len >> 2;
			len &= 3;
#else
			do
			{
				*((uint32_t *)buf) = io_rd32(*ibuf);
				ibuf++;
				buf += 4;
				len -= 4;
			}
			while (len >= 4);
#endif
		}
		else
		{
			do
			{
				d = io_rd32(*ibuf);
				memcpy(buf, &d, 4);
				ibuf++;
				buf += 4;
				len -= 4;
			}
			while (len >= 4);
		}
	}

	/* read final non 32-bit data */
	if (len)
	{
//dbg("nfc: read info - final buf=x%X ofs=%d len=%d\n", buf, ofs, len);
		d = io_rd32(*ibuf);
		memcpy(buf, &d, len);
	}

//dbg("nfc: read info - done buf=x%X ofs=%d len=%d\n", buf, ofs, len);
//	io_wr32(r->CFG, cfg);
}


static inline void nfc_cmd(nfcr_p r, uint32_t cmd)
{
//dbg("nfc: cmd info - %X", cmd);
//	io_wr32(r->CFG, io_rd32(r->CFG) | NFC_CFG_EN);
	io_wr32(r->CTL, cmd);
}


uint8_t	nfc_getstatus(nfc_p t)
{
	nfcr_p		r;

	r = t->r;
	return (uint8_t)io_rd32(r->FSR);
}


void nfc_nand_reset(nfc_p t)
{
	nfcr_p		r;

//dbg("nfc: nand_reset info\n");
	r = t->r;
	nfc_cmd(r, (NFC_CTL_OP_RESET << 8) |(t->ce << 5) | NFC_CTL_GO);
}


void nfc_nand_id(nfc_p t, int len)
{
	nfcr_p		r;

//dbg("nfc: nand_id info\n");
	r = t->r;
	io_wr32(r->CMDR, 0x90);
	io_wr32(r->ADDR1, 0);
	nfc_cmd(r, (len << 16) | (NFC_CTL_OP_ID << 8) | (t->ce << 5) | NFC_CTL_GO);
}


void nfc_nand_status(nfc_p t)
{
	nfcr_p		r;

	r = t->r;
	io_wr32(r->CMDR, 0x70);
	nfc_cmd(r, (NFC_CTL_OP_STATUS << 8) | (t->ce << 5) |
		(t->acen << 4) | (t->eccen << 3) | NFC_CTL_GO);
}


void nfc_nand_read(nfc_p t, int pg)
{
	nfcr_p		r;
	uint32_t	d;

	//dbg("nfc: nand_read info\n");

	//printk(KERN_DEBUG "nfc_nand_read page 0x%08x\n",pg);
	
	r = t->r;
	if (t->cached)
	{
		d = 3 << 12;
		io_wr32(r->CMDR, 0x3f313000);
	}
	else
	{
		if (t->pgsz <= 1)
		{
			d = 1 << 12;
		}
		else
		{
			d = 2 << 12;
		}
		io_wr32(r->CMDR, 0x00003000);
	}

	d |= (1 << 16) | (NFC_CTL_OP_READ << 8) | (t->ce << 5) |
		(t->acen << 4) | (t->eccen << 3) | NFC_CTL_GO;
	nfc_set_addr(t, 0, pg, 1);
	nfc_cmd(r, d);
}


void nfc_nand_prog(nfc_p t, int pg)
{
	nfcr_p		r;
	uint32_t	d, cmd;

//dbg("nfc: nand_prog info\n");
	r = t->r;
	if (t->cached)
	{
		d = 3 << 12;
		cmd = 0x00101580;
	}
	else if (t->twoplane)
	{
		d = 4 << 12;
		cmd = 0x00101580;
	}
	else
	{
		d = 2 << 12;
		cmd = 0x00001080;
	}
	io_wr32(r->CMDR, cmd);
	nfc_set_addr(t, 0, pg, 1);
	nfc_cmd(r, d | (1 << 16) | (NFC_CTL_OP_PROG << 8) | (t->ce << 5) |
		(t->acen << 4) | (t->eccen << 3) | NFC_CTL_GO);
}


void nfc_nand_erase(nfc_p t, int pg)
{
	nfcr_p		r;
	uint32_t	d;

	//dbg("nfc: nand_erase info\n");
	r = t->r;
	d = (2 << 12) | (1 << 16) | (NFC_CTL_OP_ERASE << 8) | (t->ce << 5) |
		(t->acen << 4) | (t->eccen << 3) | NFC_CTL_GO;
	io_wr32(r->CMDR, 0x0000d060);
	nfc_set_addr(t, 0, pg, 1);
	nfc_cmd(r, d);
}


