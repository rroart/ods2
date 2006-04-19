CONFIG_ODS2_FS=m

CFLAGS  := $(CFLAGS) -Werror-implicit-function-declaration

ifeq ($(PATCHLEVEL),4)
O_TARGET := ods2.o

obj-y    := super.o inode.o file.o dir.o util.o tparse.o bitmap.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
else
obj-$(CONFIG_ODS2_FS) += ods2.o

ods2-y    := super.o inode.o file.o dir.o util.o tparse.o bitmap.o

CFLAGS  := $(CFLAGS) -DTWOSIX
endif

ifneq ($(KERNELRELEASE),)
obj-y    := super.o inode.o file.o dir.o util.o tparse.o bitmap.o
ods2-objs := super.o inode.o file.o dir.o util.o tparse.o bitmap.o
obj-m    := ods2.o

else
KSRC        := /lib/modules/$(shell uname -r)/build
PWD         := $(shell pwd)

all:
	$(MAKE) -C $(KSRC) SUBDIRS=$(PWD) modules

install:
#	$(MAKE) -C $(KSRC) SUBDIRS=$(pwd) modules_install

clean:
	-rm *.o *.ko .*.cmd *.mod.c *~

endif

