#
# linux/drivers/video/ssl/Makefile
#
obj-$(CONFIG_FB_SSL) := sslfb.o
sslfb-objs := ssl_fb.o lcdc.o
EXTRA_CFLAGS += -DIO_MAP=1 -DINTC=0 -DCONFIG_TO1 -DLINUX_VERSION=020620
