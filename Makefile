ifeq ($(PATCHLEVEL),4)
O_TARGET := ods2.o

obj-y    := super.o inode.o file.o dir.o util.o tparse.o bitmap.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
else
obj-$(CONFIG_ODS2_FS) += ods2.o

ods2-y    := super.o inode.o file.o dir.o util.o tparse.o bitmap.o

CFLAGS	:= $(CFLAGS) -DTWOSIX
endif