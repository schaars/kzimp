toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: inet_tcp_microbench inet_udp_microbench inet_udp_multicast_microbench unix_microbench \
	 pipe_microbench pipe_vmsplice_microbench ipc_msg_queue_microbench posix_msg_queue_microbench \
	 barrelfish_message_passing_microbench

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

ipc_msg_queue_microbench: $(DEPS) src/ipc_msg_queue.c
	$(shell if [ ! -e IPC_MSG_QUEUE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=1000000" > IPC_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell grep -v '#' IPC_MSG_QUEUE_PROPERTIES 2>/dev/null) -o bin/$@ $^

posix_msg_queue_microbench: $(DEPS) src/posix_msg_queue.c
	$(C) $(CFLAGS) -lrt -o bin/$@ $^

barrelfish_message_passing_microbench: $(DEPS) src/urpc.h src/urpc_transport.c src/barrelfish.c
	$(shell if [ ! -e BARRELFISH_MESSAGE_PASSING_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DURPC_MSG_WORDS=8" > BARRELFISH_MESSAGE_PASSING_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell grep -v '#' BARRELFISH_MESSAGE_PASSING_PROPERTIES 2>/dev/null) -o bin/$@ $^

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
	-rm bin/ipc_msg_queue_microbench
	-rm bin/barrelfish_message_passing_microbench
