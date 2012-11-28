/*
 * (C) Copyright sasin@solomon-systech.com
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <linux/usb.h>
#include <asm/arch/gpio.h>

static int	sslehci_init(struct usb_hcd *hcd);

static const struct hc_driver hc_ssl_drv = 
{
	.description = "ssl ehci",
	.product_desc = "ssl ehci",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/* generic hardware linkage */
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_USB2,

	/* basic lifecycle operations */
	.reset = sslehci_init,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/* managing i/o requests and associated device resources */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,

	/* scheduling support */
	.get_frame_number = ehci_get_frame,

	/* root hub support */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
#ifdef  CONFIG_PM
	//.suspend = ehci_hub_suspend,  // changed by spiritwen DATE: 080905
	//.resume = ehci_hub_resume,
#endif
};


static int	sslehci_init(struct usb_hcd *hcd)
{
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	int				rt;
	uint32_t		d, r;

	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs + HC_LENGTH(readl(&ehci->caps->hc_capbase));
	ehci->hcs_params = readl(&ehci->caps->hcs_params);
	ehci->is_tdi_rh_tt = 1;

	rt = ehci_init(hcd);

	/* identify version and apply fixes */
	r = (uint32_t)((char *)ehci->caps - 0x100);	
	d = readl(r + 0x10) & 0xFFFF;
	if (d == 0x608)
	{
		/* Magus TO1: set burst to 15 - seems to fixes data integity */
		writel(0xF0F, r + 0x160);
	}
	else if (d == 0x808)
	{
		/* Magus TO2: disable stream mode */
		writel(0x13, r + 0x1A8);
	}

	/* prevent unexpected IRQ after previous OTG switch */
	ehci_writel(ehci, 0, &ehci->regs->intr_enable);
	return rt;
}


static int usb_hcd_probe (struct platform_device *dev)
{
	struct usb_hcd	*hcd;
	struct resource	*rc;
	int				rt;

	if (usb_disabled())
	{
		return -ENODEV;
	}
#if 1
	/*Max Yin: Force OTG bypass. 
	  If need OTG function, this part of code need to be removed
	*/
	if(request_mem_region(MAGUS_IO_OTG, 0x1000, "SSL OTG")!=0){
		volatile unsigned int *otgr = ioremap_nocache(MAGUS_IO_OTG, 0x1000);
		unsigned int d;
		if(otgr != NULL){
			int i, count;
			unsigned int status;
			otgr[0x08>>2] = 1; // enable OTG
			otgr[0x0C>>2] = 1; // reset OTG
			mdelay(10);
			otgr[0x0C>>2] = 0; // resume OTG
			
			mdelay(100);
			
			status = otgr[0x28>>2];
			//printk("EHCI-SSL: OTG STS2: 0x%08x\n", status);
			
			d= otgr[(0x10>>2)];
			//d &= 0x000000FF;
#if 0
			d = 0x1C0; // by pass, force host
#else
			d = (
				1 << 10 | /* SW DP pulldown */
				1 <<  9 | /* SWHNP */
				1 <<  8 | /* OTG bypass */
				1 <<  7 | /* Host device select->host */
				1 <<  6 | /* EN 16-bit interface, 30 M*/
				1 <<  5   /* VBUS enable */
				);
#endif
			otgr[(0x10>>2)]=d;
			#if 0 /* for debug, print out all OTG's registers */
			for(i=0 ; i<13 ; i++){
				printk("OTG[0x%08x]: 0x%08x\n", otgr+ i, otgr[i]);
			}
			#endif
			iounmap(otgr);
		}
		release_mem_region(MAGUS_IO_OTG, 0x1000);
		/* turn off overcurrent detection */
		gpio_direction_output(MAGUS_GPIO_EHCIOCD, 0);
	}
	else{
		printk("SSL OTG: failed to request OTG memory region\n");
	}
#endif

	rt = -ENOMEM;
	hcd = usb_create_hcd (&hc_ssl_drv, &dev->dev, "ssl_ehci");
	if (!hcd)
	{
		goto err1;
	}

	/* assumes 1st resource is IO address */
	rc = platform_get_resource(dev, IORESOURCE_MEM, 0);
	hcd->rsrc_start = rc->start;
	hcd->rsrc_len = rc->end - rc->start + 1;
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
			hc_ssl_drv.description)) 
	{
	    rt = -EBUSY;
	    goto err2;
	}
	hcd->regs = ioremap_nocache (hcd->rsrc_start, hcd->rsrc_len) + 0x100;
	if (!hcd->regs)
	{
		goto err3;
	}

	/* assumes 2nd resource is IRQ */
	rt = platform_get_irq(dev, 0);
	rt = usb_add_hcd (hcd, rt, IRQF_DISABLED);
	if (rt)
	{
		goto err4;
	}

	platform_set_drvdata(dev, hcd);
	return 0;

err4:
	iounmap (hcd->regs);
err3:
	release_mem_region (hcd->rsrc_start, hcd->rsrc_len);
err2:
	usb_put_hcd (hcd);
err1:
	dev_err(&dev->dev, "probe err %d\n", rt);
	return rt;
} 
EXPORT_SYMBOL (usb_hcd_probe);


static int usb_hcd_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd;

	hcd = platform_get_drvdata(dev);
	if (!hcd)
	{
		return -ENODEV;
	}

	disable_irq(hcd->irq);
	usb_remove_hcd (hcd);
	iounmap (hcd->regs);
	release_mem_region (hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd (hcd);
	return 0;
}
EXPORT_SYMBOL (usb_hcd_remove);


static struct platform_driver ehci_ssl_driver =
{
	.driver =
	{
		.name	= "ehci",
		.bus	= &platform_bus_type,
	},
	.probe		= usb_hcd_probe,
	.remove		= usb_hcd_remove,
	.shutdown	= usb_hcd_platform_shutdown,
};

