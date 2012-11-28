#ifndef _I2CR_SASI_
#define _I2CR_SASI_


#pragma pack(4)

/*I2C registers */
typedef struct
{
	uint32_t	IDR;	/**< Device Identification Register */
	uint32_t	CAPR;	/**< Capabilities Register */
	uint32_t	CFGR;	/**< Configuration Register */
	uint32_t	SADDR;	/**< Slave Address Register */
	uint32_t	OPR;	/**< Operational Register */
	uint32_t	BRR;	/**< Baud Rate Register */
	uint32_t	IER;	/**< Interrupt Enable Register */
	uint32_t	ISR;	/**< Interrupt Status Register */
	uint32_t	DATA;	/**< Data Register */
}
volatile i2cr_t, *i2cr_p;

#pragma pack()

/* device identification registers */
#define I2C_IDR_CLID		0xffff0000
#define I2C_IDR_DSGNR		0x0000FC00
#define	I2C_IDR_MAJ			0x000003C0
#define	I2C_IDR_MIN			0x0000003F
#define I2C_PCI_CLASS		0x0C04

/* Configuration Register */
#define I2C_CFGR_CTLDIS		(1 << 2)
#define I2C_CFGR_RST		(1 << 1)
#define I2C_CFGR_EN			(1 << 0)

/* Operational Register */
#define I2C_OPR_SCL			(1 << 31)	/* float clk or pull low */
#define I2C_OPR_SDA			(1 << 30)	/* float dat or pull low */
#define I2C_OPR_ACKHI		(1 << 5)	/* ack hi or lo */
#define I2C_OPR_ACK			(1 << 4)	/* detect ack */
#define I2C_OPR_RX			(1 << 3)	/* rx or tx */
#define I2C_OPR_ADDR		(1 << 2)	/* send addr */
#define I2C_OPR_CMD_RESET	0
#define I2C_OPR_CMD_START	1
#define I2C_OPR_CMD_CONT	2
#define I2C_OPR_CMD_END		3

/* Interrupt Enable/Status Register */
#define I2C_INT_CMDERR		(1 << 7)
#define I2C_INT_LOST		(1 << 2)
#define I2C_INT_RDY			(1 << 0)

/* Interrupt Status Register only */
#define I2C_STA_CLK			(1 << 31)
#define I2C_STA_DAT			(1 << 30)
#define I2C_STA_BUSY		(1 << 3)
#define I2C_STA_NAK			(1 << 1)

#endif

