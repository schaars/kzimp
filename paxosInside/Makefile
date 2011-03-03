toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: inet_tcp_paxosInside

C:=g++
CFLAGS:=-Wall -Werror -g -ltcmalloc
DEPS:=src/paxosInside.cc src/Message.cc src/Proposer.cc src/Acceptor.cc src/Learner.cc src/PaxosNode.cc

	
inet_tcp_paxosInside: $(DEPS) 
	$(shell if [ ! -e INET_TCP_PROPERTIES ]; then echo "" > INET_TCP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_TCP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

clean:
	-rm *.o
	-rm *~
	-rm INET_TCP_PROPERTIES

clobber:
	-rm *.o
	-rm *~
	-rm bin/inet_tcp_paxosInside