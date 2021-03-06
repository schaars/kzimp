/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Unix domain sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define FAULTY_RECEIVER if you want the receiver to call recv() infinitely
#define FAULTY_RECEIVER
#undef FAULTY_RECEIVER

/********** All the variables needed by Unix domain sockets **********/

#define UNIX_SOCKET_FILE_NAME "/tmp/multicore_replication_microbench_producer"

#define MIN_MSG_SIZE (sizeof(char))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int sock; // the socket

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

#ifdef SYSCALLS_MEASUREMENT
uint64_t nb_syscalls_send;
uint64_t nb_syscalls_recv;
uint64_t nb_syscalls_first_recv;
#endif

struct sockaddr_un *addresses; // for each consumer, its address

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

#ifdef SYSCALLS_MEASUREMENT
  nb_syscalls_send = 0;
  nb_syscalls_recv = 0;
  nb_syscalls_first_recv = 0;
#endif
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  int i;

  core_id = _core_id;

  // create socket
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);

  // fill the addresses
  addresses = (struct sockaddr_un*) malloc(
      sizeof(struct sockaddr_un) * nb_receivers);
  if (!addresses)
  {
    perror("IPC_initialize_producer malloc error ");
    exit(-1);
  }

  for (i = 0; i < nb_receivers; i++)
  {
    bzero((char *) &addresses[i], sizeof(addresses[i]));
    addresses[i].sun_family = AF_UNIX;
    snprintf(addresses[i].sun_path, sizeof(char) * 108, "%s_%i",
        UNIX_SOCKET_FILE_NAME, i + 1); // core_id starts at 1 for the consumers
  }

  // wait a few seconds for the consumers to be bound to their ports
  sleep(1);
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  // create socket
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);

  // fill the addresses
  addresses = (struct sockaddr_un*) malloc(sizeof(struct sockaddr_un) * 1);
  if (!addresses)
  {
    perror("IPC_initialize_producer malloc error ");
    exit(-1);
  }

  bzero((char *) &addresses[0], sizeof(addresses[0]));
  addresses[0].sun_family = AF_UNIX;
  snprintf(addresses[0].sun_path, sizeof(char) * 108, "%s_%i",
      UNIX_SOCKET_FILE_NAME, core_id);
  unlink(addresses[0].sun_path);

  // bind socket
  bind(sock, (struct sockaddr *) &addresses[0], sizeof(addresses[0]));

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
  char filename[108];
  int i;

  for (i = 0; i < nb_receivers; i++)
  {
    snprintf(filename, sizeof(char) * 108, "%s_%i", UNIX_SOCKET_FILE_NAME,
        i + 1); // core_id starts at 1 for the consumers
    unlink(filename);
  }
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  // close socket
  close(sock);

  free(addresses);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  // close socket
  close(sock);

  free(addresses);
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
    int sent;

    sent = 0;
    while (sent < msg_size)
    {
      rdtsc(cycle_start);
      sent += sendto(sock, msg + sent, msg_size - sent, 0,
          (struct sockaddr*) &addresses[i], sizeof(addresses[i]));
      rdtsc(cycle_stop);

#ifdef SYSCALLS_MEASUREMENT
      nb_syscalls_send++;
#endif

      nb_cycles_send += cycle_stop - cycle_start;
    }
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

  uint64_t cycle_start, cycle_stop;

  int recv_size = 0;
  while (recv_size < msg_size)
  {
    rdtsc(cycle_start);
    recv_size += recvfrom(sock, msg + recv_size, msg_size - recv_size, 0, 0, 0);
    rdtsc(cycle_stop);

#ifdef SYSCALLS_MEASUREMENT
    nb_syscalls_recv++;
#endif

    nb_cycles_recv += cycle_stop - cycle_start;
  }

#ifdef SYSCALLS_MEASUREMENT
  if (nb_syscalls_first_recv == 0)
  {
    nb_syscalls_first_recv = nb_syscalls_recv;
  }
#endif

  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }

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
