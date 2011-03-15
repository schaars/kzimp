toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

# This version of PaxosInside works with Barrelfish MP and ULM only. However, we keep the other compilation rules.
#all: inet_tcp_paxosInside inet_udp_paxosInside unix_paxosInside \
	 pipe_paxosInside ipc_msg_queue_paxosInside posix_msg_queue_paxosInside \
	 barrelfish_mp_paxosInside ulm_paxosInside
all: barrelfish_mp_paxosInside ulm_paxosInside

C:=g++
CFLAGS:=-Wall -Werror -g -ltcmalloc
DEPS:=src/time.h src/paxosInside.cc src/Message.cc src/Request.cc src/Accept_req.cc src/Learn.cc src/Response.cc src/PaxosNode.cc src/Client.cc

	
inet_tcp_paxosInside: $(DEPS) src/comm_mech/tcp_net.c src/comm_mech/inet_tcp_socket.c
	$(shell if [ ! -e INET_TCP_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=128 -DTCP_NAGLE" > INET_TCP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_TCP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
inet_udp_paxosInside: $(DEPS) src/comm_mech/inet_udp_socket.c
	$(shell if [ ! -e INET_UDP_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=128" > INET_UDP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_UDP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
unix_paxosInside: $(DEPS) src/comm_mech/unix_socket.c
	$(shell if [ ! -e UNIX_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=128" > UNIX_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_UDP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
pipe_paxosInside: $(DEPS) src/comm_mech/pipe.c
	$(shell if [ ! -e PIPE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=128" > PIPE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat PIPE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ipc_msg_queue_paxosInside: $(DEPS) src/comm_mech/ipc_msg_queue.c
	$(shell if [ ! -e IPC_MSG_QUEUE_PROPERTIES ]; then echo "-DIPC_MSG_QUEUE -DMESSAGE_MAX_SIZE=128" > IPC_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat IPC_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
posix_msg_queue_paxosInside: $(DEPS) src/comm_mech/posix_msg_queue.c
	$(shell if [ ! -e POSIX_MSG_QUEUE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=128" > POSIX_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat POSIX_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt
	
barrelfish_mp_paxosInside: $(DEPS) src/comm_mech/urpc.h src/comm_mech/urpc_transport.c src/comm_mech/barrelfish_mp.c
	$(shell if [ ! -e BARRELFISH_MP_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DMESSAGE_MAX_SIZE=128 -DURPC_MSG_WORDS=16" > BARRELFISH_MP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat BARRELFISH_MP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ulm_paxosInside: $(DEPS) src/comm_mech/mpsoc.c src/comm_mech/ulm.c
	$(shell if [ ! -e ULM_PROPERTIES ]; then echo "-DULM -DMESSAGE_MAX_SIZE=128 -DNB_MESSAGES=10" > ULM_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat ULM_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
clean:
	-rm *.o
	-rm *~
	-rm INET_TCP_PROPERTIES
	-rm INET_UDP_PROPERTIES
	-rm UNIX_PROPERTIES
	-rm PIPE_PROPERTIES
	-rm IPC_MSG_QUEUE_PROPERTIES
	-rm POSIX_MSG_QUEUE_PROPERTIES
	-rm BARRELFISH_MP_PROPERTIES
	-rm ULM_PROPERTIES

clobber:
	-rm *.o
	-rm *~
	-rm bin/inet_tcp_paxosInside
	-rm bin/inet_udp_paxosInside
	-rm bin/unix_paxosInside
	-rm bin/pipe_paxosInside
	-rm bin/ipc_msg_queue_paxosInside
	-rm bin/posix_msg_queue_paxosInside
	-rm bin/barrelfish_mp_paxosInside
	-rm bin/ulm_paxosInside