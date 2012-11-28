
/* SPI registers */
#pragma pack(4)

typedef struct
{
	uint32_t	id;
	uint32_t	cap;
	uint32_t	ctl;
	uint32_t	op;
	uint32_t	master;
	uint32_t	readcount;
	uint32_t	slave_adv_bits;	/* advance slave fifo by bits(1) or cs(0) */
	uint32_t	slave_tout;		/* slave rx fifo timeout in bits */
	uint32_t	fifo;
	uint32_t	rx;
	uint32_t	tx;
	uint32_t	ier;
	uint32_t	sta;
	uint32_t	tst;
}
volatile spir_t, *spir_p;

#pragma pack()

#define SPI_PCI_CLASS	0x0780

#define SPI_ID_CLASS(d)		((d & 0xffff0000) >> 16)
#define SPI_ID_DSGNR(d)		((d & 0xFC00) >> 10)
#define SPI_ID_MAJ(d)		((d & 0x3C0) >> 6)
#define SPI_ID_MIN(d)		(d & 0x3F)

#define SPI_CAP_SLAVE(d)	(d & (1 << 12))
#define SPI_CAP_MASTER(d)	(d & (1 << 11))
#define SPI_CAP_NSLAVE(d)	((d & 0x0700) >> 8)
#define SPI_CAP_BUF(d)		((d & 0xff) + 1)

#define SPI_CTL_RST		2
#define SPI_CTL_ENA		1

#define	SPI_OP_EN		0x80000000			/* enable operation */
#define SPI_OP_CS(n)	((n) << 16)			/* chip select number 0-7 */
#define SPI_OP_BURST(n)	(((n) - 1) << 12)	/* words per burst 1-16 */
#define SPI_OP_RXEN		0x00000400			/* enable rx while tx */
#define SPI_OP_MASTER	0x00000200			/* master */
#define SPI_OP_CSHIGH	0x00000100			/* chip select active high */
#define SPI_OP_LSB		0x00000080			/* LSb serialized first */
#define SPI_OP_CLKPHASE	0x00000040			/* phase1 or 180 off */
#define SPI_OP_CLKFALL	0x00000020			/* clk falling edge capture */
#define SPI_OP_WORD(n)	((n) - 1)			/* word bitlength: 1 - 32 */

#define	SPI_MST_AUTORD	0x080000
#define SPI_MST_FSS		0x040000			/* force CS high */
#define SPI_MST_CKDIV	0x0f
#define SPI_MST_CSDLY(h)	(((h) - 1) << 4)/* CS to data in halfbits 1-16 */
#define SPI_MST_DATDLY(b)	(((b) - 1) << 8)/* inter burst bit time 1-1024 */

#define SPI_FIFO_RWM	8
#define SPI_FIFO_TWM	4
#define SPI_FIFO_RRST	2
#define SPI_FIFO_TRST	1

#define	SPI_INT_TOUT	0x100
#define SPI_INT_TWM		0x80
#define SPI_INT_RWM		0x40
#define SPI_INT_WOVR	0x20
#define SPI_INT_WUNR	0x10
#define SPI_INT_FOVR	0x08
#define SPI_INT_TEMT	0x04
#define SPI_INT_THRE	0x02
#define SPI_INT_RDR		0x01
#define SPI_INT_ENALL	0x1ff

#define SPI_STA_INT		0xff

#define SPI_TST_MODE_NORM	0
#define SPI_TST_MODE_LPBK 	2
#define SPI_TST_MODE_ECHO 	1
#define SPI_TST_MODE_UNDIF 	3
