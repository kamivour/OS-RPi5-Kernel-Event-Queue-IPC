# Kernel module Makefile for RPi5 (ARM64)
# Build on target: make
# Clean: make clean

obj-m += event_driver.o

# Kernel build directory - auto-detected on RPi
KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
