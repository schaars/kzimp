all: inet_tcp_microbench inet_udp_microbench inet_udp_multicast_microbench

C:=gcc
CFLAGS:=-Wall -Werror -g -lm

inet_tcp_microbench: time.h list.c tcp_net.c inet_tcp_socket.c microbench.c
	$(C) $(CFLAGS) -o $@ $^

inet_udp_microbench: time.h list.c inet_udp_socket.c microbench.c
	$(C) $(CFLAGS) -o $@ $^

inet_udp_multicast_microbench: time.h list.c inet_udp_socket.c microbench.c
	$(C) $(CFLAGS) -DIP_MULTICAST -o $@ $^

clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o
	-rm *~
	-rm inet_tcp_microbench
	-rm inet_udp_microbench
	-rm inet_udp_multicast_microbench
