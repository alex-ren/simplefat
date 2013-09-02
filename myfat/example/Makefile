#
# Makefile for the Linux fat filesystem support.
#
######

######

KVERS = $(shell uname -r)

######
# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y

######
# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DATS_FAT_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

KERNELDIR = $(shell if test -d ~/workspace/linux-stable/include; \
then echo "~/workspace/linux-stable"; fi)

# ifeq ($(KERNELDIR),)
KERNELDIR = /lib/modules/$(KVERS)/build
# endif
# KERNELDIR ?= ~/workspace/linux-stable

######
#
# Specify flags for the module compilation.
#
# EXTRA_CFLAGS += $(DEBFLAGS)
# EXTRA_CFLAGS += -I$(LDDINC)
# EXTRA_CFLAGS += -I$(ATSHOME)
# EXTRA_CFLAGS += -D_ATS_HEADER_NONE -D_ATS_PRELUDE_NONE

######

ifneq ($(KERNELRELEASE),)
# call from kernel build system
# Kernel modules
obj-m += fat.o
obj-m += vfat.o
obj-m += msdos.o

fat-objs := cache.o dir.o fatent.o file.o inode.o misc.o 
vfat-objs := namei_vfat.o
msdos-objs := namei_msdos.o
else

PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
#	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINC=$(PWD)/../include modules

endif

######


.PHONY: clean
clean:
	rm -f *~
#	rm -f *_?ats.c *_?ats.o
	make -C $(KERNELDIR) M=$(PWD) clean


# depend .depend dep:
# 	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


# ifeq (.depend,$(wildcard .depend))
# include .depend
# endif
###### end of [Makefile] ######


