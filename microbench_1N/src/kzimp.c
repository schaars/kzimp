/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: kzimp
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ipc_interface.h"
#include "time.h"

#define KZIMP_CHAR_DEV_FILE "/dev/kzimp0"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by kzimp **********/

// port used by the producer
// the ports used by the consumers are PRODUCER_PORT + core_id

#define MIN_MSG_SIZE (sizeof(char))

int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int fd; // kzimp file descriptor

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

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
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  // sleep to be sure that the multicast mask is set by the different receivers
  sleep(1);

  fd = open(KZIMP_CHAR_DEV_FILE, O_WRONLY);
  if (fd < 0)
  {
    printf(">>> Error while opening channel\n");
  }
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  fd = open(KZIMP_CHAR_DEV_FILE, O_RDONLY);
  if (fd < 0)
  {
    printf("<<< Error while opening channel\n");
  }
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  close(fd);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close(fd);
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
#ifdef COMPUTE_CYCLES
  uint64_t cycle_start, cycle_stop;
#endif

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
      core_id, msg[0], msg_size, nb_receivers);
#endif

#ifdef COMPUTE_CYCLES
  rdtsc(cycle_start);
#endif

  write(fd, msg, msg_size);

#ifdef COMPUTE_CYCLES
  rdtsc(cycle_stop);
  nb_cycles_send += cycle_stop - cycle_start;
#endif
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
  //char msg[MESSAGE_MAX_SIZE];
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

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  // let's say that the first packet contains the header
  // we assume messages are not delivered out of order

#ifdef COMPUTE_CYCLES
  uint64_t cycle_start, cycle_stop;
#endif

  int recv_size = -1;

#ifdef COMPUTE_CYCLES
  rdtsc(cycle_start);
#endif

  recv_size = read(fd, msg, msg_size);

#ifdef COMPUTE_CYCLES
  rdtsc(cycle_stop);
  nb_cycles_recv += cycle_stop - cycle_start;
#endif

  *msg_id = msg[0];

#ifdef COMPUTE_CYCLES
  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }
#endif

#ifdef DEBUG
  printf("[consumer %i] received message %i of size %i, should be %i\n",
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
