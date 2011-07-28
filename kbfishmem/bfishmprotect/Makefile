C:=gcc
CFLAGS:=-Wall -Werror -g -lm -ltcmalloc -DMESSAGE_BYTES=64
DEPS:= futex.c bfishmprotect.c bfishmprotect_test.c
TARGETS:= bfishmprotect_simple_test bfishmprotect_fork_test bfishmprotect_get_struct_ump_message_size

all: $(TARGETS)

bfishmprotect_get_struct_ump_message_size: bfishmprotect_get_struct_ump_message_size.c
	$(C) $(CFLAGS) -o $@ $^
	
bfishmprotect_simple_test: $(DEPS)
	$(C) $(CFLAGS) -DSIMPLE_TEST -o $@ $^
	
bfishmprotect_fork_test: $(DEPS)
	$(C) $(CFLAGS) -o $@ $^
	
clean:
	-rm *.o
	-rm *~

clobber: clean
	-rm $(TARGETS)
