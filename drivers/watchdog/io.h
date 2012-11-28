#ifndef __IO_H___
#define __IO_H___
#define io_rd32(a)      (*(volatile uint32_t *)(a))
#define io_wr32(a, d)   (*(volatile uint32_t *)(a) =  (d))
#define io_rd16(a)      (*(volatile uint16_t *)(a))
#define io_wr16(a, d)   (*(volatile uint16_t *)(a) = (d))
#define io_rd8(a)       (*(volatile uint8_t *)(a))
#define io_wr8(a, d)    (*(volatile uint8_t *)(a) = (d))
#define IO_RD32		io_rd32
#define IO_WR32		io_wr32
#define IO_RD16		io_rd16
#define IO_WR16		io_wr16
#define IO_RD8		io_rd8

#define __BIT(a)		(1<<a)

#define BIT_CLEAN32(a, b)	IO_WR32(a, IO_RD32(a)	& (~__BIT(b))	)
#define BIT_CLEAN16(a, b)	IO_WR32(a, IO_RD16(a)	& (~__BIT(b))	)
#define BIT_CLEAN8(a, b)	IO_WR32(a, IO_RD8(a)	& (~__BIT(b))	)
#define BIT_SET32(a, b)		IO_WR32(a, IO_RD32(a)	| __BIT(b)	)
#define BIT_SET16(a, b)		IO_WR32(a, IO_RD16(a)	| __BIT(b)	)
#define BIT_SET8(a, b)		IO_WR32(a, IO_RD8(a)	| __BIT(b)	)

#endif

