all: inet_tcp_microbench

C:=gcc
CFLAGS:=-Wall -Werror -g

%.o: %.c
	$(C) $(CFLAGS) -o $@ -c $<

inet_tcp_microbench: time.h tcp_net.o inet_tcp_socket.o microbench.o
	$(C) $(CFLAGS) -o $@ $^

clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o *~ inet_tcp_microbench
