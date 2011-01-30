/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: IPC message queue
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by IPC message queues **********/

#define MIN_MSG_SIZE (sizeof(int) + sizeof(long))

/* a message for IPC message queue */
struct ipc_message
{
  long mtype;
  char mtext[MESSAGE_MAX_SIZE]; // MESSAGE_MAX_SIZE is defined at compile time, when calling gcc
};

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static int *consumers;
static int consumer_queue; // pointer to this consumer's queue for reading

// Initialize resources for both the producer and the consumers
// First initialization function called
void IPC_initialize(int _nb_receivers, int _request_size)
{
  nb_receivers = _nb_receivers;
  request_size = _request_size;

  key_t key;
  int i;

  consumers = (int*) malloc(sizeof(int) * nb_receivers);
  if (!consumers)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (i = 0; i < nb_receivers; i++)
  {
    if ((key = ftok("../src/ipc_msg_queue.c", 'A' + i)) == -1)
    {
      perror("ftok");
      exit(1);
    }

    if ((consumers[i] = msgget(key, 0600 | IPC_CREAT)) == -1)
    {
      perror("msgget");
      exit(1);
    }
  }
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  //TODO
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  consumer_queue =
  //TODO
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  //TODO
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  //TODO
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  //TODO
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
    //TODO
    write(pipes[i][1], msg, msg_size);
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
  //TODO
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
    //TODO
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
