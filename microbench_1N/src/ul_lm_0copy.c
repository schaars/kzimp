/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: User-level Local Multicast with 0-copy
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

#include "ipc_interface.h"
#include "mpsoc.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by UDP sockets **********/

// port used by the producer
// the ports used by the consumers are PRODUCER_PORT + core_id

#define MIN_MSG_SIZE (sizeof(char))

int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

static struct mpsoc_ctrl ulm_ctrl;

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

  int i;
  unsigned int multicast_bitmap_mask = 0;
  for (i = 0; i < nb_receivers; i++)
  {
    multicast_bitmap_mask = multicast_bitmap_mask | (1 << i);
  }

  mpsoc_init(&ulm_ctrl, "/tmp/ul_lm_0copy_microbenchmark", nb_receivers,
      NB_MESSAGES, multicast_bitmap_mask);
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  // in order to ensure that the consumers are listening on the structure,
  // otherwise a very fast producer can fill the buffer and this is bad at the beginning
  //sleep(1);
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  mpsoc_destroy(&ulm_ctrl);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  mpsoc_destroy(&ulm_ctrl);
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
  int msg_pos_in_ring_buffer;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = mpsoc_alloc(&ulm_ctrl, msg_size, &msg_pos_in_ring_buffer);
  if (!msg)
  {
    perror("mpsoc_alloc error! ");
    exit(errno);
  }

  bzero(msg, msg_size);

  msg[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %i of size %i to %i recipients\n",
      core_id, msg_long[0], msg_size, nb_receivers);
#endif

#ifdef COMPUTE_CYCLES
  rdtsc(cycle_start);
#endif

  mpsoc_sendto(&ulm_ctrl, msg, msg_size, msg_pos_in_ring_buffer, -1);

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

  recv_size = mpsoc_recvfrom(&ulm_ctrl, msg, msg_size, core_id - 1);

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
