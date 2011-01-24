all: inet_server inet_client lm_server lm_client unix_server unix_client ipc_server ipc_client #smmp_replica smmp_replica_pthread

C:=gcc
CFLAGS:=-Wall -Werror -g

%.o: %.c
	$(C) $(CFLAGS) -o $@ -c $<

inet_server: inet_socket.c list.c timer.h server.c
	$(C) -o $@ $(CFLAGS) $^ -lm

inet_client: inet_socket.c client.c
	$(C) -o $@ $(CFLAGS) $^

lm_server: inet_socket.c list.c timer.h server.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST $^ -lm

lm_client: inet_socket.c client.c
	$(C) -o $@ $(CFLAGS) -DLOCAL_MULTICAST $^

unix_server: unix_socket.c list.c timer.h server.c
	$(C) -o $@ $(CFLAGS) $^ -lm

unix_client: unix_socket.c client.c
	$(C) -o $@ $(CFLAGS) $^

ipc_server: ipcmsgqueue.c list.c timer.h ipc_server.c
	$(C) -o $@ $(CFLAGS) $^ -lm

ipc_client: ipcmsgqueue.c ipc_client.c
	$(C) -o $@ $(CFLAGS) $^

#smmp_replica: smmp.c list.c smmp_replica.c
#	$(C) -o $@ $(CFLAGS) -DSMMP_MEMCPY $^ -lm
#
#smmp_replica_pthread: smmp.c list.c smmp_replica.c
#	$(C) -o $@ $(CFLAGS) -DPTHREAD_LOCK -DSMMP_MEMCPY $^ -lm
	
clean:
	-rm *.o
	-rm *~

clobber:
	-rm inet_server inet_client lm_server lm_client unix_server unix_client ipc_server ipc_client #smmp_replica smmp_replica_pthread
