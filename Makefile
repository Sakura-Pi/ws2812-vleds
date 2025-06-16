CONFIG_MODULE_NAME=ws2812-vleds
# CONFIG_WS2812_VLEDS=m
# =========== CONFIG END ============

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
