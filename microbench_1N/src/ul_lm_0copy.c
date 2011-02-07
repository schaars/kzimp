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

#define MIN_MSG_SIZE (sizeof(int) + sizeof(long))

int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

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

  mpsoc_init("/tmp/ul_lm_0copy_microbenchmark", nb_receivers, NB_MESSAGES);
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
  mpsoc_destroy();
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  mpsoc_destroy();
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
void IPC_sendToAll(int msg_size, long msg_id)
{
  uint64_t cycle_start, cycle_stop;
  char *msg;
  int msg_pos_in_ring_buffer;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  msg = mpsoc_alloc(msg_size, &msg_pos_in_ring_buffer);
  if (!msg)
  {
    perror("mpsoc_alloc error! ");
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

  rdtsc(cycle_start);
  mpsoc_sendto(msg, msg_size, msg_pos_in_ring_buffer, -1);
  rdtsc(cycle_stop);

  nb_cycles_send += cycle_stop - cycle_start;
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, long *msg_id)
{
  char *msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  // let's say that the first packet contains the header
  // we assume messages are not delivered out of order

  uint64_t cycle_start, cycle_stop;

  int recv_size = -1;
  while (recv_size == -1)
  {
    rdtsc(cycle_start);
    recv_size = mpsoc_recvfrom((void**) &msg, msg_size, core_id-1);
    rdtsc(cycle_stop);

    nb_cycles_recv += cycle_stop - cycle_start;
  }

  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }

  int msg_size_in_msg = *((int*) msg);

  // get the id of the message
  *msg_id = *((long*) ((int*) msg + 1));

#ifdef DEBUG
  printf(
      "[consumer %i] received message %li of size %i, should be %i (%i in the message)\n",
      core_id, *msg_id, recv_size, msg_size, msg_size_in_msg);
#endif

  if (recv_size == msg_size && msg_size == msg_size_in_msg)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
