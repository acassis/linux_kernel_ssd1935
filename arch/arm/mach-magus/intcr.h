#ifndef _INTCR_SASI_
#define _INTCR_SASI_

#define BF_GET(r, pos, width)	(((r) >> (pos)) & ((1 << (width)) - 1))

/** register map */
typedef struct
{
	uint32_t	id;				/**< PCI ID & version */
	uint32_t	cap;			/**< Capabilities */
	uint32_t	ctl;			/**< Control register */
	uint32_t	_rsv;
	uint32_t	priority[8];	/**< priority (4 bits per source) */
	uint32_t	fiq[2];			/**< FIQ select bitmap */
	uint32_t	ena[2];			/**< enable bitmap */
	uint32_t	sta[2];			/**< status bitmap */
	uint32_t	mask;			/**< mask by number */
	uint32_t	unmask;			/**< unmask by number */
	uint32_t	pend;			/**< highest priority pending interrupt */
}
intc_reg_t;

#define _INTC_CLSDEV		(0x0800 << 6)

#define INTC_ID_CLS(r)		((r) >> 16)
#define INTC_ID_DEV(r)		BF_GET(r, 10, 6)
#define INTC_ID_CLSDEV(r)	((r) >> 10)
#define INTC_ID_MAJ(r)		BF_GET(r, 6, 4)
#define INTC_ID_MIN(r)		BF_GET(r, 0, 6)

#define INTC_CTL_ENA	1
#define INTC_CTL_RST	2
#define INTC_CTL_MSK	0x80000000

#define INTC_PRI_MAX	7		/* although 4 bits */


#endif

