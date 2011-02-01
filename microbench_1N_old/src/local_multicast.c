/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Local Multicast
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by UDP sockets **********/

// port used by the producer
// the ports used by the consumers are PRODUCER_PORT + core_id
#define PRODUCER_PORT 6000

#define MIN_MSG_SIZE (sizeof(int) + sizeof(long))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int sock; // the socket

struct sockaddr_in addresses;
struct sockaddr_in multicast_addr;

#define MIN(a, b) ((a < b) ? a : b)

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;
  request_size = _request_size;

  multicast_addr.sin_addr.s_addr = 0x7f7f7f7f;
  multicast_addr.sin_family = AF_INET;
  multicast_addr.sin_port = 0;
}

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

  addresses.sin_addr.s_addr = 0x7f7f7f7f;
  addresses.sin_family = AF_INET;
  addresses.sin_port = htons(PRODUCER_PORT + core_id);

  // bind socket
  bind(sock, (struct sockaddr *) &addresses, sizeof(addresses));
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
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  // close socket
  close(sock);
}

// Send a message to all the cores
// The message id will be msg_id
// Return the total sent payload (i.e. size of the messages times number of consumers)
// if spent_cycles is not NULL, then add the number of spent cycles in *spent_cycles
int IPC_sendToAll(int msg_size, long msg_id, uint64_t *spent_cycles)
{
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(sizeof(char) * msg_size);
  if (!msg)
  {
    perror("IPC_sendToAll allocation error! ");
    exit(errno);
  }

  // malloc is lazy: the pages may not be really allocated yet.
  // We force the allocation and the fetch of the pages with bzero
  bzero(msg, msg_size);

  int *msg_int = (int*) msg;
  msg_int[0] = msg_size;
  long *msg_long = (long*) (msg_int + 1);
  msg_long[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %li of size %i to %i recipients\n",
      core_id, msg_long[0], msg_size, nb_receivers);
#endif

  uint64_t cycle_start, cycle_stop;

  int sent = 0;

  while (sent < msg_size)
  {
    rdtsc(cycle_start);
    sent += sendto(sock, msg, msg_size, 0, (struct sockaddr*) &multicast_addr,
        sizeof(multicast_addr));
    rdtsc(cycle_stop);

    if (spent_cycles != NULL)
    {
      *spent_cycles += (cycle_stop - cycle_start);
    }
  }

  free(msg);

  return msg_size;
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
// if spent_cycles is not NULL, then add the number of spent cycles in *spent_cycles
int IPC_receive(int msg_size, long *msg_id, uint64_t *spent_cycles)
{
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = (char*) malloc(sizeof(char) * msg_size);
  if (!msg)
  {
    perror("IPC_receive allocation error! ");
    exit(errno);
  }

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
    recv_size += recvfrom(sock, msg + recv_size, msg_size - recv_size,
        MSG_DONTWAIT, 0, 0);
    rdtsc(cycle_stop);

    if (spent_cycles != NULL)
    {
      *spent_cycles += (cycle_stop - cycle_start);
    }
  }

  int msg_size_in_msg = *((int*) msg);

  // get the id of the message
  *msg_id = *((long*) ((int*) msg + 1));

#ifdef DEBUG
  printf(
      "[consumer %i] received message %li of size %i, should be %i (%i in the message)\n",
      core_id, *msg_id, recv_size, msg_size, msg_size_in_msg);
#endif

  free(msg);

  if (recv_size == msg_size && msg_size == msg_size_in_msg)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
