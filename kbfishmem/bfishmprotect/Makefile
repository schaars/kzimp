C:=gcc
CFLAGS:=-Wall -Werror -g -lm -ltcmalloc

all: bfishmprotect_test

bfishmprotect_test: bfishmprotect.c bfishmprotect_test.c
	$(C) $(CFLAGS) -o $@ $^

clean:
	-rm *.o
	-rm *~

clobber: clean
	-rm bfishmprotect_test
