toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: inet_tcp_microbench inet_udp_microbench inet_udp_multicast_microbench unix_microbench \
	 pipe_microbench pipe_vmsplice_microbench ipc_msg_queue_microbench posix_msg_queue_microbench \
	 barrelfish_message_passing local_multicast_microbench ul_lm_0copy_microbench

C:=gcc
CFLAGS:=-Wall -Werror -g -lm
DEPS:=src/microbench.c

inet_tcp_microbench: $(DEPS) src/tcp_net.c src/inet_tcp_socket.c 
	$(shell if [ ! -e INET_TCP_PROPERTIES ]; then echo "" > INET_TCP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_TCP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

inet_udp_microbench: $(DEPS) src/inet_udp_socket.c
	$(shell if [ ! -e INET_UDP_PROPERTIES ]; then echo "" > INET_UDP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_UDP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

inet_udp_multicast_microbench: $(DEPS) src/inet_udp_socket.c
	$(shell if [ ! -e INET_UDP_PROPERTIES ]; then echo "-DIP_MULTICAST" > INET_UDP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_UDP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

unix_microbench: $(DEPS) src/unix_socket.c
	$(shell if [ ! -e UNIX_SOCKETS_PROPERTIES ]; then echo "" > UNIX_SOCKETS_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat UNIX_SOCKETS_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

pipe_microbench: $(DEPS) src/pipe.c
	$(shell if [ ! -e PIPE_PROPERTIES ]; then echo "" > PIPE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat PIPE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

pipe_vmsplice_microbench: $(DEPS) src/pipe.c
	$(shell if [ ! -e PIPE_PROPERTIES ]; then echo "-DVMSPLICE" > PIPE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat PIPE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

ipc_msg_queue_microbench: $(DEPS) src/ipc_msg_queue.c
	$(shell if [ ! -e IPC_MSG_QUEUE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=1000000" > IPC_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat IPC_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

posix_msg_queue_microbench: $(DEPS) src/posix_msg_queue.c
	$(shell if [ ! -e POSIX_MSG_QUEUE_PROPERTIES ]; then echo "" > POSIX_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat POSIX_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt

barrelfish_message_passing: $(DEPS) src/urpc.h src/urpc_transport.c src/barrelfish_mp.c
	$(shell if [ ! -e BARRELFISH_MESSAGE_PASSING_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DURPC_MSG_WORDS=8" > BARRELFISH_MESSAGE_PASSING_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell grep -v '#' BARRELFISH_MESSAGE_PASSING_PROPERTIES 2>/dev/null) -o bin/$@ $^

local_multicast_microbench: $(DEPS) src/local_multicast.c
	$(shell if [ ! -e LOCAL_MULTICAST_PROPERTIES ]; then echo "" > LOCAL_MULTICAST_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat LOCAL_MULTICAST_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ul_lm_0copy_microbench: $(DEPS) src/mpsoc.c src/ul_lm_0copy.c
	$(shell if [ ! -e UL_LM_0COPY_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DMESSAGE_MAX_SIZE=1000" > UL_LM_0COPY_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell grep -v '#' UL_LM_0COPY_PROPERTIES 2>/dev/null) -o bin/$@ $^ -lrt
	
clean:
	-rm *.o
	-rm *~
	-rm INET_TCP_PROPERTIES
	-rm INET_UDP_PROPERTIES
	-rm UNIX_SOCKETS_PROPERTIES
	-rm PIPE_PROPERTIES
	-rm IPC_MSG_QUEUE_PROPERTIES
	-rm POSIX_MSG_QUEUE_PROPERTIES
	-rm BARRELFISH_MESSAGE_PASSING_PROPERTIES
	-rm LOCAL_MULTICAST_PROPERTIES
	-rm UL_LM_0COPY_PROPERTIES

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
	-rm bin/barrelfish_message_passing
	-rm bin/local_multicast_microbench
	-rm bin/ul_lm_0copy_microbench