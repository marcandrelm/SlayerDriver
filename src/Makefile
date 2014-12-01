TARGET_MODULE:=CamDriver

# If we are running by kernel building system
ifneq ($(KERNELRELEASE),)
	obj-m := $(TARGET_MODULE).o

# If we running without kernel build system
else
	BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

all : 
# run kernel build system to make module
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
# run kernel build system to cleanup in current directory
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean
	
clean_soft:
	-rm -f *.o .*.cmd .*.flags *.mod.c Module.symvers modules.order
	-rm -rf .tmp_versions

install : unload all clean_soft load

load:
	sudo insmod ./$(TARGET_MODULE).ko

unload:
	sudo rmmod ./$(TARGET_MODULE).ko


UVC:
# remove concurent camera driver
	sudo rmmod uvcvideo
endif
