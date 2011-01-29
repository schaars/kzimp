all: inet_tcp_microbench inet_udp_microbench inet_udp_multicast_microbench unix_microbench

C:=gcc
CFLAGS:=-Wall -Werror -g -lm

inet_tcp_microbench: list.c tcp_net.c inet_tcp_socket.c microbench.c
	$(C) $(CFLAGS) -o $@ $^

inet_udp_microbench: list.c inet_udp_socket.c microbench.c
	$(C) $(CFLAGS) -o $@ $^

inet_udp_multicast_microbench: list.c inet_udp_socket.c microbench.c
	$(C) $(CFLAGS) -DIP_MULTICAST -o $@ $^

unix_microbench: list.c unix_socket.c microbench.c
	$(C) $(CFLAGS) -o $@ $^

clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o
	-rm *~
	-rm inet_tcp_microbench
	-rm inet_udp_microbench
	-rm inet_udp_multicast_microbench
	-rm unix_microbench
