obj-m := kbfish.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS += $(shell if [ ! -e $(M)/KBFISH_PROPERTIES ]; then echo "-DMESSAGE_BYTES=64" > $(M)/KBFISH_PROPERTIES; fi;\
				  cat $(M)/KBFISH_PROPERTIES | tr '\n' ' ' 2>/dev/null)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
