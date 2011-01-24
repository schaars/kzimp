/* using INET sockets */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>

#ifdef LOCAL_MULTICAST
#include "timer.h"
#endif

#include "multicore_multicast.h"

/* port numbers range from PORT_CORE_0 to PORT_CORE_0 + MAX_NB_CORES - 1 */
#define PORT_CORE_0 6001
#define IP_ADDRESS "127.0.0.1"
#define UDP_SEND_MAX_SIZE 65507

#define MIN(a, b) ((a < b) ? a : b)

int sock;
struct sockaddr_in core_addr[MAX_NB_CORES];
#ifdef LOCAL_MULTICAST
  struct sockaddr_in multicast_addr;
#endif

/* Initialize client sockets */
void initialize_client(void) {
  sock = socket(AF_INET, SOCK_DGRAM, 0);

  bzero((char *)&core_addr[core_id], sizeof(core_addr[core_id]));
#ifdef LOCAL_MULTICAST
  core_addr[core_id].sin_addr.s_addr = 0x7f7f7f7f;
#else
  core_addr[core_id].sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  core_addr[core_id].sin_family = AF_INET;
  core_addr[core_id].sin_port = htons(PORT_CORE_0 + core_id);
  bind(sock, (struct sockaddr *)&core_addr[core_id], sizeof(core_addr[core_id]));
  
  bzero((char *)&core_addr[0], sizeof(core_addr[0]));
#ifdef LOCAL_MULTICAST
  core_addr[0].sin_addr.s_addr = 0x7f7f7f7f;
#else
  core_addr[0].sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  core_addr[0].sin_family = AF_INET;
  core_addr[0].sin_port = htons(PORT_CORE_0);

#ifdef LOCAL_MULTICAST
  bzero((char *)&multicast_addr, sizeof(multicast_addr));
  multicast_addr.sin_addr.s_addr = 0x7f7f7f7f;
  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_port = 0;
#endif

}


/* Initialize server sockets */
void initialize_server(void) {
  int i;
  sock = socket(AF_INET, SOCK_DGRAM, 0);

  for (i=0; i<nb_cores; i++) {
    bzero((char *)&core_addr[i], sizeof(core_addr[i]));
#ifdef LOCAL_MULTICAST
    core_addr[i].sin_addr.s_addr = 0x7f7f7f7f;
#else
    core_addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    core_addr[i].sin_family = AF_INET;
    core_addr[i].sin_port = htons(PORT_CORE_0 + i);
  }

#ifdef LOCAL_MULTICAST
  bzero((char *)&multicast_addr, sizeof(multicast_addr));
  multicast_addr.sin_addr.s_addr = 0x7f7f7f7f;
  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_port = 0;
#endif

  bind(sock, (struct sockaddr *)&core_addr[core_id], sizeof(core_addr[core_id]));
}


/* receive a message and return its size.
 * s is the size the message should have.
 */
int mmcast_receive(void *buf, int s) {
  int recv_size;
  recv_size = 0;

#ifdef LOCAL_MULTICAST
  while (recv_size != s) {
    recv_size = recvfrom(sock, buf, MESSAGE_MAX_SIZE, MSG_DONTWAIT, 0, 0);
  }
#else
  while (recv_size < s) {
    recv_size += recvfrom(sock, buf+recv_size, s-recv_size, 0, 0, 0);
  }
#endif

  return recv_size;
}


/* send buffer buf of size s to core c */
void mmcast_send(void *buf, int s, int c) {
#ifdef LOCAL_MULTICAST
#ifndef UNICAST_ONLY
  if (core_id == 0 && c == -1) 
    sendto(sock, buf, s, 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
  else
    sendto(sock, buf, s, 0, (struct sockaddr*)&core_addr[c], sizeof(core_addr[c]));
#else
  int i;

  for (i=0; i<nb_cores; i++)
    sendto(sock, buf, s, 0, (struct sockaddr*)&core_addr[i], sizeof(core_addr[i]));
#endif

#else
  int sent, to_send;

  sent = 0;
  while (sent < s) {
    to_send = MIN(s-sent, UDP_SEND_MAX_SIZE);
    sent += sendto(sock, buf+sent, to_send, 0, (struct sockaddr*)&core_addr[c], sizeof(core_addr[c]));
  }
#endif
}
