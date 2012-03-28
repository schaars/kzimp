toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: barrelfish_mp_checkpointing 	ulm_checkpointing 		kzimp_checkpointing 				\
	 bfish_mprotect_checkpointing 	unix_checkpointing 		inet_tcp_checkpointing 			\
	 inet_udp_checkpointing 		   pipe_checkpointing		ipc_msg_queue_checkpointing	\
	 posix_msg_queue_checkpointing   openmpi_checkpointing	mpich2_checkpointing
	 #kbfish_checkpointing

C:=g++
OPENMPIC:=mpic++
MPICH2C:=/home/bft/mpich2-install/bin/mpic++
CFLAGS:=-Wall -Werror -g -ltcmalloc
DEPS:=src/checkpointing.cc src/Checkpointer.cc src/Message.cc src/Checkpoint_request.cc src/Checkpoint_response.cc
	
barrelfish_mp_checkpointing: $(DEPS) src/comm_mech/urpc.h src/comm_mech/urpc_transport.c src/comm_mech/barrelfish_mp.c
	$(shell if [ ! -e BARRELFISH_MP_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DURPC_MSG_WORDS=16 -DURPC_MSG_WORDS_CHKPT=16" > BARRELFISH_MP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat BARRELFISH_MP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ulm_checkpointing: $(DEPS) src/comm_mech/mpsoc.c src/comm_mech/ulm.c
	$(shell if [ ! -e ULM_PROPERTIES ]; then echo "-DULM -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DNB_MESSAGES=10" > ULM_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat ULM_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
kzimp_checkpointing: $(DEPS) src/comm_mech/kzimp.c
	$(shell if [ ! -e KZIMP_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > KZIMP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat KZIMP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

bfish_mprotect_checkpointing: $(DEPS) src/comm_mech/bfish_mprotect.c ../kbfishmem/bfishmprotect/futex.c ../kbfishmem/bfishmprotect/bfishmprotect.c
	$(shell if [ ! -e BFISH_MPROTECT_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DMESSAGE_BYTES=64" > BFISH_MPROTECT_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat BFISH_MPROTECT_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	$(C) $(CFLAGS) $(shell cat BFISH_MPROTECT_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/bfishmprotect_get_struct_ump_message_size ../kbfishmem/bfishmprotect/futex.c ../kbfishmem/bfishmprotect/bfishmprotect.c ../kbfishmem/bfishmprotect/bfishmprotect_get_struct_ump_message_size.c

kbfish_checkpoiting: $(DEPS) src/comm_mech/kbfish.c
	$(shell if [ ! -e KBFISH_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > KBFISH_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat KBFISH_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
inet_tcp_checkpointing: $(DEPS) src/comm_mech/tcp_net.c src/comm_mech/inet_tcp_socket.c
	$(shell if [ ! -e INET_TCP_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DTCP_NAGLE" > INET_TCP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_TCP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
inet_udp_checkpointing: $(DEPS) src/comm_mech/inet_udp_socket.c
	$(shell if [ ! -e INET_UDP_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > INET_UDP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_UDP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
unix_checkpointing: $(DEPS) src/comm_mech/unix_socket.c
	$(shell if [ ! -e UNIX_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > UNIX_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat UNIX_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
pipe_checkpointing: $(DEPS) src/comm_mech/pipe.c
	$(shell if [ ! -e PIPE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > PIPE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat PIPE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ipc_msg_queue_checkpointing: $(DEPS) src/comm_mech/ipc_msg_queue.c
	$(shell if [ ! -e IPC_MSG_QUEUE_PROPERTIES ]; then echo "-DIPC_MSG_QUEUE -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > IPC_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat IPC_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
posix_msg_queue_checkpointing: $(DEPS) src/comm_mech/posix_msg_queue.c
	$(shell if [ ! -e POSIX_MSG_QUEUE_PROPERTIES ]; then echo "-DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > POSIX_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat POSIX_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt
	
openmpi_checkpointing: $(DEPS) src/comm_mech/openmpi.c
	$(shell if [ ! -e MPI_PROPERTIES ]; then echo "-DUSE_MPI -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > MPI_PROPERTIES; sleep 1; fi)
	$(OPENMPIC) $(CFLAGS) $(shell cat MPI_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt

mpich2_checkpointing: $(DEPS) src/comm_mech/openmpi.c
	$(shell if [ ! -e MPI_PROPERTIES ]; then echo "-DUSE_MPI -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64" > MPI_PROPERTIES; sleep 1; fi)
	$(MPICH2C) $(CFLAGS) $(shell cat MPI_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt
	
clean:
	-rm *.o
	-rm *~
	-rm BARRELFISH_MP_PROPERTIES
	-rm ULM_PROPERTIES
	-rm KZIMP_PROPERTIES
	-rm BFISH_MPROTECT_PROPERTIES
	-rm KBFISH_PROPERTIES
	-rm INET_TCP_PROPERTIES
	-rm INET_UDP_PROPERTIES
	-rm UNIX_PROPERTIES
	-rm PIPE_PROPERTIES
	-rm IPC_MSG_QUEUE_PROPERTIES
	-rm POSIX_MSG_QUEUE_PROPERTIES
	-rm MPI_PROPERTIES

clobber:
	-rm *.o
	-rm *~
	-rm bin/barrelfish_mp_checkpointing
	-rm bin/ulm_checkpointing
	-rm bim/kzimp_checkpointing
	-rm bin/bfish_mprotect_checkpointing
	-rm bin/kbfish_checkpointing
	-rm bin/inet_tcp_checkpointing
	-rm bin/inet_udp_checkpointing
	-rm bin/unix_checkpointing
	-rm bin/pipe_checkpointing
	-rm bin/ipc_msg_queue_checkpointing
	-rm bin/posix_msg_queue_checkpointing
	-rm bin/openmpi_checkpointing
	-rm bin/mpich2_checkpointing
