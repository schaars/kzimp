toplevel:
	-mkdir -p bin
	-$(MAKE) $(MLAGS) all

all: inet_tcp_paxosInside

C:=g++
CFLAGS:=-Wall -Werror -g -ltcmalloc
DEPS:=src/paxosInside.cc src/Message.cc src/Request.cc src/Accept_req.cc src/Learn.cc src/PaxosNode.cc src/Client.cc

	
inet_tcp_paxosInside: $(DEPS) src/tcp_net.c src/inet_tcp_socket.c
	$(shell if [ ! -e INET_TCP_PROPERTIES ]; then echo "-DTCP_NAGLE" > INET_TCP_PROPERTIES; fi)
	$(C) $(CFLAGS) $(shell cat INET_TCP_PROPERTIES | tr '\n' ' ' 2>/dev/null) -o bin/$@ $^

clean:
	-rm *.o
	-rm *~
	-rm INET_TCP_PROPERTIES

clobber:
	-rm *.o
	-rm *~
	-rm bin/inet_tcp_paxosInside
