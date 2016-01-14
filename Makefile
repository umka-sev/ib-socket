ifneq ($(KERNELRELEASE),)
    obj-m	:= netsrv.o netcli.o
    obj_ib-y	:= ib-sock.o 
    netsrv-objs	:= srv.o $(obj_ib-y)
    netcli-objs	:= cli.o $(obj_ib-y)
else
    KDIR        := /lib/modules/$(shell uname -r)/build
    PWD         := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -f Module.markers Module.symvers modules.order

