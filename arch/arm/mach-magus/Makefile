#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

# Object file lists.

obj-y				+= irq.o intc.o gpio.o time.o gpt.o dma.o ssl_gpio.o clock.o ssldma.o
led-y := leds.o

# Power Management
obj-$(CONFIG_PM) += pm.o sleep.o

# CPU Frequency Scaling
obj-$(CONFIG_CPU_FREQ_MAGUS)	+= cpufreq.o

obj-$(CONFIG_ARCH_MAGUS_FPGA)	+= magus.o
obj-$(CONFIG_ARCH_MAGUS_ADS)	+= magus.o
obj-$(CONFIG_ACCIO_PF101)	+= accio_pf101.o
obj-$(CONFIG_ACCIO_PF102)	+= accio_pf102.o
obj-$(CONFIG_ACCIO_CM5208)	+= cm5208.o
obj-$(CONFIG_ACCIO_CM5210)  += cm5210.o
obj-$(CONFIG_ACCIO_LITE)	+= accio_lite.o
obj-$(CONFIG_ACCIO_P1)		+= accio_p1.o
obj-$(CONFIG_LUMOS_WE8623_P0)	+= we8623_p0.o
obj-$(CONFIG_ACCIO_A2818T)      += a2818t.o
obj-$(CONFIG_LEDS)			+= $(led-y)

