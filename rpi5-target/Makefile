# Kernel module Makefile for RPi 5 (ARM64)
# Build on target: make (normal) or make debug

# Normal version (production-like)
obj-m += event_driver.o

# Debug version (with printk logging)
obj-m += event_driver_debug.o

# Kernel build directory - auto-detected on RPi
KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

debug:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
