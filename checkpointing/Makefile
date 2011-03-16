toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: barrelfish_mp_checkpointing ulm_checkpointing

C:=g++
CFLAGS:=-Wall -Werror -g -ltcmalloc
DEPS:=src/time.h src/checkpointing.cc src/Checkpointer.cc src/Message.cc src/Checkpoint_request.cc src/Checkpoint_response.cc

posix_msg_queue_checkpointing: $(DEPS) src/comm_mech/posix_msg_queue.c
	$(shell if [ ! -e POSIX_MSG_QUEUE_PROPERTIES ]; then echo "" > POSIX_MSG_QUEUE_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat POSIX_MSG_QUEUE_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^ -lrt
	
barrelfish_mp_checkpointing: $(DEPS) src/comm_mech/urpc.h src/comm_mech/urpc_transport.c src/comm_mech/barrelfish_mp.c
	$(shell if [ ! -e BARRELFISH_MP_PROPERTIES ]; then echo "-DNB_MESSAGES=10 -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DURPC_MSG_WORDS=16 -DURPC_MSG_WORDS_CHKPT=16" > BARRELFISH_MP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat BARRELFISH_MP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
ulm_checkpointing: $(DEPS) src/comm_mech/mpsoc.c src/comm_mech/ulm.c
	$(shell if [ ! -e ULM_PROPERTIES ]; then echo "-DULM -DMESSAGE_MAX_SIZE=64 -DMESSAGE_MAX_SIZE_CHKPT_REQ=64 -DNB_MESSAGES=10" > ULM_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat ULM_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^
	
clean:
	-rm *.o
	-rm *~
	-rm BARRELFISH_MP_PROPERTIES
	-rm ULM_PROPERTIES

clobber:
	-rm *.o
	-rm *~
	-rm bin/barrelfish_mp_checkpointing
	-rm bin/ulm_checkpointing
