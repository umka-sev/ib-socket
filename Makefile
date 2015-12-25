ifneq ($(KERNELRELEASE),)
    obj-m	:= netsrv.o netcli.o
    netsrv-objs	:= srv.o ib-sock.o ib-sock-mem.o ib-sock-util.o
    netcli-objs	:= cli.o ib-sock.o ib-sock-mem.o ib-sock-util.o
else
    KDIR        := /lib/modules/$(shell uname -r)/build
    PWD         := /Users/shadow/work/lustre/work/WorkQ/LNet/test2

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -f Module.markers Module.symvers modules.order

