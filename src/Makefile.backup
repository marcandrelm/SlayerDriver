ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := CamDriver.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

install:
	cp -p *.ko /tmp/

clean:
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
