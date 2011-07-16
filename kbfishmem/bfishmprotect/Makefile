C:=gcc
CFLAGS:=-Wall -Werror -g -lm -ltcmalloc -DMESSAGE_BYTES=64 -DWAIT_TYPE=USLEEP
DEPS:= bfishmprotect.c bfishmprotect_test.c
TARGETS:= bfishmprotect_simple_test bfishmprotect_writer_test bfishmprotect_reader_test

all: $(TARGETS)

bfishmprotect_simple_test: $(DEPS)
	$(C) $(CFLAGS) -DSIMPLE_TEST -o $@ $^
	
bfishmprotect_writer_test: $(DEPS)
	$(C) $(CFLAGS) -DWRITER -o $@ $^
	
bfishmprotect_reader_test: $(DEPS)
	$(C) $(CFLAGS) -DREADER -o $@ $^

clean:
	-rm *.o
	-rm *~

clobber: clean
	-rm $(TARGETS)
