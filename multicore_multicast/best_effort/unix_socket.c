/* using UNIX sockets */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <sys/un.h>

#include "multicore_multicast.h"

#define UDP_SEND_MAX_SIZE 65507

#define MIN(a, b) ((a < b) ? a : b)

int sock;
struct sockaddr_un core_addr[MAX_NB_CORES];


/* Initialize client sockets */
void initialize_client(void) {
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);

  bzero((char *)&core_addr[core_id], sizeof(core_addr[core_id]));
  core_addr[core_id].sun_family = AF_UNIX;
  snprintf(core_addr[core_id].sun_path, sizeof(char)*108, "/tmp/core%i", core_id);
  unlink(core_addr[core_id].sun_path);
  bind(sock, (struct sockaddr *)&core_addr[core_id], sizeof(core_addr[core_id]));
  
  bzero((char *)&core_addr[0], sizeof(core_addr[0]));
  core_addr[0].sun_family = AF_UNIX;
  snprintf(core_addr[0].sun_path, sizeof(char)*108, "/tmp/core%i", 0);
}


/* Initialize server sockets */
void initialize_server(void) {
  int i;
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);

  for (i=0; i<nb_cores; i++) {
    bzero((char *)&core_addr[i], sizeof(core_addr[i]));
    core_addr[i].sun_family = AF_UNIX;
    snprintf(core_addr[i].sun_path, sizeof(char)*108, "/tmp/core%i", i);
  }

  unlink(core_addr[core_id].sun_path);
  bind(sock, (struct sockaddr *)&core_addr[core_id], sizeof(core_addr[core_id]));
}


/* receive a message and return its size */
int mmcast_receive(void *buf, int s) {
  int recv_size;
  
  recv_size = 0;
  while (recv_size < s) {
    recv_size += recvfrom(sock, buf+recv_size, s-recv_size, 0, 0, 0);
  }

  return recv_size;
}


/* send buffer buf of size s to core c */
void mmcast_send(void *buf, int s, int c) {
  int sent, to_send;

  sent = 0;
  while (sent < s) {
     to_send = MIN(s-sent, UDP_SEND_MAX_SIZE);
     sent += sendto(sock, buf+sent, to_send, 0, (struct sockaddr*)&core_addr[c], sizeof(core_addr[c]));
  }
}
