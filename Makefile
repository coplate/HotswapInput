export ARCH:=arm
export CROSS_COMPILE:=/home/osboxes/kindle/opt/cross-gcc-linaro/bin/arm-linux-gnueabi-

obj-m := tkbd.o

KDIR := /home/osboxes/kindle/gplrelease/linux-3.0.35

PWD := $(shell pwd)


EXTRA_CFLAGS := -I$(src)/drivers/


default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
