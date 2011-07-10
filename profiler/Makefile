################################################################################
# Automatically-generated file. Do not edit!
################################################################################

CC=gcc
INCLUDES= -I/usr/include/ -I/usr/src/linux-source-2.6.36/include/ -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/src/linux-source-2.6.32-custom/include/
CFLAGS=-Wall -O2 -g -Werror ${INCLUDES}
LDFLAGS=-lpthread -lelf -lglib-2.0 -lbfd -lnuma

TARGETS=makefile.dep profiler profiler-sampling parser-sampling


all: $(TARGETS)

makefile.dep : *.[Cch]
	    for i in *.[Cc]; do gcc -MM "$${i}"; done > $@
-include makefile.dep

parser-sampling: parser-sampling.o symbols.o events.o

clean:
	rm -f *.o $(TARGETS)

.PHONY: all clean 
