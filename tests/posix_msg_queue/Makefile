all: posix_msg_queue_test posix_msg_queue_timeout_test posix_msg_queue_handler_test

GCC:=gcc
FLAGS:=-g3 -ggdb -Wall -Werror
LIBS=-lrt

%.o: %.c
	$(GCC) $(FLAGS) -o $@ -c $<

posix_msg_queue_test: posix_msg_queue_test.o
	$(GCC) $(FLAGS) -o $@ $^ $(LIBS)

posix_msg_queue_timeout_test: posix_msg_queue_timeout_test.o
	$(GCC) $(FLAGS) -o $@ $^ $(LIBS)

posix_msg_queue_handler_test: posix_msg_queue_handler_test.o
	$(GCC) $(FLAGS) -o $@ $^ $(LIBS)

clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o
	-rm *~
	-rm posix_msg_queue_test posix_msg_queue_timeout_test posix_msg_queue_handler_test