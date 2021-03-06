/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Inet sockets using UDP
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define FAULTY_RECEIVER if you want the receiver to call recv() infinitely
#define FAULTY_RECEIVER
#undef FAULTY_RECEIVER

/********** All the variables needed by UDP sockets **********/

// port used by the producer
// the ports used by the consumers are PRODUCER_PORT + core_id
#define PRODUCER_PORT 6000

#define UDP_SEND_MAX_SIZE 65507

#ifdef IP_MULTICAST
#define MULTICAST_ADDR "228.5.6.7"
#endif

#define MIN_MSG_SIZE (sizeof(char))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

#ifdef INET_SYSCALLS_MEASUREMENT
uint64_t nb_syscalls_send;
uint64_t nb_syscalls_recv;
uint64_t nb_syscalls_first_recv;
#endif

static int sock; // the socket

#ifdef IP_MULTICAST
struct sockaddr_in multicast_addr;
#else
struct sockaddr_in *addresses; // for each consumer, its address
#endif

#define MIN(a, b) ((a < b) ? a : b)

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;

  request_size = _request_size;
  if (request_size < MIN_MSG_SIZE)
  {
    request_size = MIN_MSG_SIZE;
  }

  nb_cycles_send = 0;
  nb_cycles_recv = 0;
  nb_cycles_first_recv = 0;

#ifdef INET_SYSCALLS_MEASUREMENT
  nb_syscalls_send = 0;
  nb_syscalls_recv = 0;
  nb_syscalls_first_recv = 0;
#endif
}

#ifdef IP_MULTICAST
// join the multicast group
void join_mcast_group(void)
{
  // construct an IGMP join request structure
  struct ip_mreq mc_req;
  mc_req.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
  mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

  // send an ADD MEMBERSHIP message via setsockopt
  if ((setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
              (void*) &mc_req, sizeof(mc_req))) < 0)
  {
    perror("setsockopt() failed");
    exit(1);
  }
}

// leave the multicast group
void leave_mcast_group(void)
{
  struct ip_mreq req;
  req.imr_multiaddr.s_addr = multicast_addr.sin_addr.s_addr;
  req.imr_interface.s_addr = htonl(INADDR_ANY);

  setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &req,
      sizeof(req));
  // we do not care if it fails :)
}
#endif

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  // create socket
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("[IPC_initialize_producer] Error while creating the socket! ");
    exit(errno);
  }

  // the producer does not need to call bind

  // fill the addresses
#ifdef IP_MULTICAST
  /* set the TTL (time to live/hop count) for the send */
  int mc_ttl = 1;
  if ((setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
              (void*) &mc_ttl, sizeof(mc_ttl))) < 0)
  {
    perror("setsockopt() failed");
    exit(1);
  }

  /* construct a multicast address structure */
  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
  multicast_addr.sin_port = htons(PRODUCER_PORT);

#else
  addresses = (struct sockaddr_in*) malloc(
      sizeof(struct sockaddr_in) * nb_receivers);
  if (!addresses)
  {
    perror("IPC_initialize_producer malloc error ");
    exit(-1);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    addresses[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    addresses[i].sin_family = AF_INET;
    addresses[i].sin_port = htons(PRODUCER_PORT + i + 1); // i starts at 0, the port starts at PRODUCER_PORT+1
  }
#endif

  // wait a few seconds for the consumers to be bound to their ports
  sleep(1);
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  // create socket
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("[IPC_initialize_producer] Error while creating the socket! ");
    exit(errno);
  }

#ifdef IP_MULTICAST
  // set reuse port to on to allow multiple binds per host
  int flag_on = 1;
  if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag_on,
              sizeof(flag_on))) < 0)
  {
    perror("setsockopt() failed");
    exit(1);
  }

  // construct a multicast address structure
  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  multicast_addr.sin_port = htons(PRODUCER_PORT);

  // bind to multicast address to socket
  if ((bind(sock, (struct sockaddr *) &multicast_addr,
              sizeof(multicast_addr))) < 0)
  {
    perror("bind() failed");
    exit(1);
  }

  join_mcast_group();

#else
  addresses = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
  if (!addresses)
  {
    perror("IPC_initialize_producer malloc error ");
    exit(-1);
  }

  addresses[0].sin_addr.s_addr = inet_addr("127.0.0.1");
  addresses[0].sin_family = AF_INET;
  addresses[0].sin_port = htons(PRODUCER_PORT + core_id);

  // bind socket
  bind(sock, (struct sockaddr *) &addresses[0], sizeof(addresses[0]));
#endif

#ifdef FAULTY_RECEIVER
  if (core_id == 1)
  {
    int ret = fcntl(sock, F_SETFL, O_NONBLOCK);
    if (ret == -1)
    {
      perror(
          "[IPC_Initialize_consumer] error when setting the socket to non-blocking mode! ");
      exit(errno);
    }
  }
#endif
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  // close socket
  close(sock);

#ifndef IP_MULTICAST
  free(addresses);
#endif
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
#ifdef IP_MULTICAST
  leave_mcast_group();
#endif

  // close socket
  close(sock);

#ifndef IP_MULTICAST
  free(addresses);
#endif
}

// Return the number of cycles spent in the send() operation
uint64_t get_cycles_send()
{
  return nb_cycles_send;
}

// Return the number of cycles spent in the recv() operation
uint64_t get_cycles_recv()
{
  return nb_cycles_recv - nb_cycles_first_recv;
}

// Send a message to all the cores
// The message id will be msg_id
void IPC_sendToAll(int msg_size, char msg_id)
{
  uint64_t cycle_start, cycle_stop;
  int i;
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(GET_MALLOC_SIZE(sizeof(char) * msg_size));
  if (!msg)
  {
    perror("IPC_sendToAll allocation error! ");
    exit(errno);
  }

  // malloc is lazy: the pages may not be really allocated yet.
  // We force the allocation and the fetch of the pages with bzero
  bzero(msg, msg_size);

  msg[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %i of size %i to %i recipients\n",
      core_id, msg_long[0], msg_size, nb_receivers);
#endif

  for (i = 0; i < nb_receivers; i++)
  {
    int sent, to_send;

    sent = 0;
    while (sent < msg_size)
    {
      to_send = MIN(msg_size - sent, UDP_SEND_MAX_SIZE);

      rdtsc(cycle_start);
#ifdef IP_MULTICAST
      sent += sendto(sock, msg + sent, to_send, 0,
          (struct sockaddr*) &multicast_addr, sizeof(multicast_addr));
#else
      sent += sendto(sock, msg + sent, to_send, 0,
          (struct sockaddr*) &addresses[i], sizeof(addresses[i]));
#endif
      rdtsc(cycle_stop);

      nb_cycles_send += cycle_stop - cycle_start;

#ifdef INET_SYSCALLS_MEASUREMENT
      nb_syscalls_send++;
#endif
    }

#ifdef IP_MULTICAST
    // Using IP multicast the message is sent once
    break;
#endif
  }

  free(msg);
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(GET_MALLOC_SIZE(sizeof(char) * msg_size));
  if (!msg)
  {
    perror("IPC_receive allocation error! ");
    exit(errno);
  }

#ifdef FAULTY_RECEIVER
  if (core_id == 1)
  {
    while (1)
    {
      recvfrom(sock, msg, msg_size, 0, 0, 0);
    }
  }
#endif

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  // let's say that the first packet contains the header
  // we assume messages are not delivered out of order

  uint64_t cycle_start, cycle_stop;

  int recv_size = 0;
  while (recv_size < msg_size)
  {
    rdtsc(cycle_start);
    recv_size += recvfrom(sock, msg + recv_size, msg_size - recv_size, 0, 0, 0);
    rdtsc(cycle_stop);

    nb_cycles_recv += cycle_stop - cycle_start;

#ifdef INET_SYSCALLS_MEASUREMENT
    nb_syscalls_recv++;
#endif
  }

  // forget the first message (this message is not counted in the statistics)
  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }

#ifdef INET_SYSCALLS_MEASUREMENT
  if (nb_syscalls_first_recv == 0)
  {
    nb_syscalls_first_recv = nb_syscalls_recv;
  }
#endif

  // get the id of the message
  *msg_id = msg[0];

#ifdef DEBUG
  printf(
      "[consumer %i] received message %i of size %i, should be %i\n",
      core_id, *msg_id, recv_size, msg_size);
#endif

  free(msg);

  if (recv_size == msg_size)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
