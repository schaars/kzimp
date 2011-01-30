/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: pipes
 */

#define _GNU_SOURCE         /* vmsplice, See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by TCP sockets **********/

#define MIN_MSG_SIZE (sizeof(int) + sizeof(long))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

// pipes[i] = the pipe[2] between the producer and the consumer i+1
static int **pipes;
static int consumer_reading_pipe; // fd used by the consumer for reading messages coming from the producer

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;
  request_size = _request_size;

  // create the pipes
  pipes = (int**) malloc(sizeof(int*) * nb_receivers);
  if (!pipes)
  {
    perror("[IPC_clean] Allocation error! ");
    exit(errno);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    pipes[i] = (int*) malloc(sizeof(int) * 2);
    pipe(pipes[i]);
  }
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    // the producer never reads from the pipe
    close(pipes[i][0]);
  }
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  consumer_reading_pipe = pipes[core_id - 1][0];

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    if (i + 1 != core_id)
    {
      close(pipes[i][0]);
    }

    // the consumers never write to the pipe
    close(pipes[i][1]);
  }
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    free(pipes[i]);
  }

  free(pipes);
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    close(pipes[i][1]);
  }
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close(consumer_reading_pipe);
}

// Send a message to all the cores
// The message id will be msg_id
// Return the total sent payload (i.e. size of the messages times number of consumers)
// if spent_cycles is not NULL, then add the number of spent cycles in *spent_cycles
int IPC_sendToAll(int msg_size, long msg_id, uint64_t *spent_cycles)
{
  int i;
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

  // malloc is lazy: the pages may not be yet really allocated.
  // We force the allocation and the fetch of the pages with bzero
  bzero(msg, msg_size);

  int *msg_as_int = (int*) msg;
  msg_as_int[0] = msg_size;
  long *msg_as_long = (long*) (msg_as_int + 1);
  msg_as_long[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %li of size %i to %i recipients\n",
      core_id, msg_as_long[0], msg_size, nb_receivers);
#endif

  uint64_t cycle_start, cycle_stop;

  for (i = 0; i < nb_receivers; i++)
  {
    // writing the content
    rdtsc(cycle_start);
#ifdef VMSPLICE
    struct iovec iov;

    iov.iov_base = &msg;
    iov.iov_len = msg_size;
    vmsplice(pipes[i][1], &iov, 1, 0);
#else
    write(pipes[i][1], msg, msg_size);
#endif
    rdtsc(cycle_stop);

    if (spent_cycles != NULL)
    {
      *spent_cycles += (cycle_stop - cycle_start);
    }
  }

  free(msg);

  return msg_size * nb_receivers;
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

  uint64_t cycle_start, cycle_stop;

  rdtsc(cycle_start);
  int header_size = read(consumer_reading_pipe, msg, MIN_MSG_SIZE);
  rdtsc(cycle_stop);

  if (spent_cycles != NULL)
  {
    *spent_cycles += (cycle_stop - cycle_start);
  }

  // get the message
  int s = 0;
  int msg_size_in_msg = *((int*) msg);
  int left = msg_size_in_msg - header_size;
  if (left > 0)
  {
    rdtsc(cycle_start);
    s = read(consumer_reading_pipe, (void*) (msg + header_size), left);
    rdtsc(cycle_stop);

    if (spent_cycles != NULL)
    {
      *spent_cycles += (cycle_stop - cycle_start);
    }
  }

  // get the id of the message
  *msg_id = *((long*) ((int*) msg + 1));

#ifdef DEBUG
  printf("[consumer %i] received message %li of size %i, should be %i (%i in the message)\n",
      core_id, *msg_id, s + header_size, msg_size, msg_size_in_msg);
#endif

  free(msg);

  if (s + header_size == msg_size && msg_size == msg_size_in_msg)
  {
    return msg_size;
  }
  else
  {
    return 0;
  }
}
