#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/arch/dma.h>
#include "map.h"


dma_t	ssldma = { .r = (void *)MAGUS_VIO_DMAC };


static irqreturn_t ssldma_isr(int irq, void *dev_id)
{
	return dma_isr(&ssldma);
}


static int __init ssldma_init(void)
{
	int	rt;

	rt = dma_init(&ssldma);
	if (rt)
	{
		return rt;
	}
	rt = request_irq(MAGUS_IRQ_DMAC, ssldma_isr, IRQF_DISABLED, 0, 0);
	if (rt)
	{
		printk("ssldma: init err - request_irq %d\n", rt);
		return rt;
	}
	return 0;
}


arch_initcall(ssldma_init);

EXPORT_SYMBOL(dma_request);
EXPORT_SYMBOL(dma_free);
EXPORT_SYMBOL(dma_cfg);
EXPORT_SYMBOL(dma_enable);
EXPORT_SYMBOL(dma_abort);
EXPORT_SYMBOL(ssldma);
