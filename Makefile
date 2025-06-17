CONFIG_MODULE_NAME=ws2812-vleds
# CONFIG_WS2812_VLEDS=m
# CONFIG_WS2812_VLEDS_CHANNEL_CONTROL=y
# =========== CONFIG END ============

ifeq ($(CONFIG_WS2812_VLEDS_CHANNEL_CONTROL),y)
ccflags-y += -DCONFIG_WS2812_VLEDS_CHANNEL_CONTROL
endif

obj-$(CONFIG_WS2812_VLEDS) := $(CONFIG_MODULE_NAME).o
ws2812-vleds-objs := \
	src/ws2812.o \
	src/utils.o \
	src/main.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
