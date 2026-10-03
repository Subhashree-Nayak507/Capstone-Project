# Out-of-tree build:  make            (needs linux-headers-$(uname -r))
obj-m += envmon_drv.o
ccflags-y += -I$(src)/../include

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
