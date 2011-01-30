toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: inet_tcp_microbench inet_udp_microbench inet_udp_multicast_microbench unix_microbench pipe_microbench pipe_vmsplice_microbench

C:=gcc
CFLAGS:=-Wall -Werror -g -lm
DEPS:=src/time_for_latency.c src/list_of_double.c src/microbench.c

inet_tcp_microbench: $(DEPS) src/tcp_net.c src/inet_tcp_socket.c 
	$(C) $(CFLAGS) -o bin/$@ $^

inet_udp_microbench: $(DEPS) src/inet_udp_socket.c
	$(C) $(CFLAGS) -o bin/$@ $^

inet_udp_multicast_microbench: $(DEPS) src/inet_udp_socket.c
	$(C) $(CFLAGS) -DIP_MULTICAST -o bin/$@ $^

unix_microbench: $(DEPS) src/unix_socket.c
	$(C) $(CFLAGS) -o bin/$@ $^

pipe_microbench: $(DEPS) src/pipe.c
	$(C) $(CFLAGS) -o bin/$@ $^

pipe_vmsplice_microbench: $(DEPS) src/pipe.c
	$(C) $(CFLAGS) -DVMSPLICE -o bin/$@ $^

clean:
	-rm *.o
	-rm *~

clobber:
	-rm *.o
	-rm *~
	-rm bin/inet_tcp_microbench
	-rm bin/inet_udp_microbench
	-rm bin/inet_udp_multicast_microbench
	-rm bin/unix_microbench
	-rm bin/pipe_microbench
	-rm bin/pipe_vmsplice_microbench