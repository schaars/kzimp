#Uncomment to get memory wrapping
#EXTRA_CFLAGS += -DMEMORY_WRAPPING

obj-m := kzimp.o 
kzimp-objs := kzimp_main.o mem_wrapper.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
