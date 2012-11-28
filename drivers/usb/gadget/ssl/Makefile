#
# linux/drivers/usb/gadget/ssl/Makefile
#
obj-$(CONFIG_USB_SSL) := ssludc.o
ssludc-objs := ssl_udc.o usbd.o
EXTRA_CFLAGS += -DIO_MAP=1 -DINTC=0 -DUSBD_DMA=1
