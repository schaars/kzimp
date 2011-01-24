all: inet_server inet_client lm_server lm_client lmunicast_server lmunicast_client unix_server unix_client ipc_server ipc_client ulm_servent ulm2_servent ipc0_server ipc0_client

C:=gcc
CFLAGS:=-Wall -Werror -g

%.o: %.c
	$(C) $(CFLAGS) -o $@ -c $<

inet_server: inet_socket.c timer.h server.c
	$(C) -o $@ $(CFLAGS) $^ -lm

inet_client: inet_socket.c client.c
	$(C) -o $@ $(CFLAGS) $^

lm_server: inet_socket.c timer.h server.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST $^

lm_client: inet_socket.c client.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST $^

lmunicast_server: inet_socket.c timer.h server.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST -DUNICAST_ONLY $^

lmunicast_client: inet_socket.c client.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST -DUNICAST_ONLY $^

unix_server: unix_socket.c timer.h server.c
	$(C) -o $@ $(CFLAGS) $^

unix_client: unix_socket.c client.c
	$(C) -o $@ $(CFLAGS) $^

ipc_server: ipcmsgqueue.c timer.h server.c
	$(C) -o $@ $(CFLAGS) $^

ipc_client: ipcmsgqueue.c client.c
	$(C) -o $@ $(CFLAGS) $^

ipc0_server: ipc0copymsgqueue.c timer.h ipc0copy_server.c
	$(C) -o $@ $(CFLAGS) $^

ipc0_client: ipc0copymsgqueue.c ipc0copy_client.c
	$(C) -o $@ $(CFLAGS) $^


ulm_servent: mpsoc.c timer.h ulm_servent.c atomic.h
	$(C) -o $@ $(CFLAGS) $^

ulm2_servent: mpsoc2.c timer.h ulm2_servent.c atomic.h
	$(C) -o $@ $(CFLAGS) $^

clean:
	-rm *.o
	-rm *~

clobber:
	-rm inet_server inet_client lm_server lm_client unix_server unix_client ipc_server ipc_client ulm_servent ulm2_servent ipc0_server ipc0_client
