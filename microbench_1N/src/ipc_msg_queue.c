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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

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

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_bzero;

static int *consumers;
static int consumer_queue; // pointer to this consumer's queue for reading

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
  nb_cycles_bzero = 0;

  key_t key;
  int i;

#ifdef ONE_QUEUE
  consumers = (int*) malloc(sizeof(int) * 1);
#else
  consumers = (int*) malloc(sizeof(int) * nb_receivers);
#endif
  if (!consumers)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (i = 0; i < nb_receivers; i++)
  {
    if ((key = ftok("/tmp/ipc_msg_queue_microbench", 'A' + i)) == -1)
    {
      perror("ftok");
      exit(1);
    }

    if ((consumers[i] = msgget(key, 0600 | IPC_CREAT)) == -1)
    {
      perror("msgget");
      exit(1);
    }

#ifdef DEBUG
    printf("Core %i key is %x\n", i, key);
#endif

#ifdef ONE_QUEUE
    break;
#endif
  }

}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

#ifdef ONE_QUEUE
  consumer_queue = consumers[0];
#else
  consumer_queue = consumers[core_id - 1];
#endif
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  int i;

  sleep(1);

  for (i = 0; i < nb_receivers; i++)
  {
    msgctl(consumers[i], IPC_RMID, NULL);

#ifdef ONE_QUEUE
    break;
#endif
  }
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
}

// Return the number of cycles spent in the send() operation
uint64_t get_cycles_send()
{
  return nb_cycles_send;
}

// Return the number of cycles spent in the recv() operation
uint64_t get_cycles_recv()
{
  return nb_cycles_recv;
}

// Return the number of cycles spent in the bzero() operation
uint64_t get_cycles_bzero()
{
  return nb_cycles_bzero;
}

// Send a message to all the cores
// The message id will be msg_id
// Return the total sent payload (i.e. size of the messages times number of consumers)
int IPC_sendToAll(int msg_size, long msg_id)
{
  uint64_t cycle_start, cycle_stop;
  struct ipc_message ipc_msg;
  int i;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

  ipc_msg.mtype = 1;

  rdtsc(cycle_start);
  bzero(ipc_msg.mtext, msg_size);
  rdtsc(cycle_stop);

  nb_cycles_bzero += cycle_stop - cycle_start;

  int *msg_as_int = (int*) ipc_msg.mtext;
  msg_as_int[0] = msg_size;
  long *msg_as_long = (long*) (msg_as_int + 1);
  msg_as_long[0] = msg_id;

#ifdef DEBUG
  printf(
      "[producer %i] going to send message %li of size %i to %i recipients\n",
      core_id, msg_as_long[0], msg_size, nb_receivers);
#endif


  for (i = 0; i < nb_receivers; i++)
  {
    // writing the content
#ifdef ONE_QUEUE
    ipc_msg.mtype = i+1;

    rdtsc(cycle_start);
    msgsnd(consumers[0], &ipc_msg, msg_size, 0);
#else
    rdtsc(cycle_start);
    msgsnd(consumers[i], &ipc_msg, msg_size, 0);
#endif
    rdtsc(cycle_stop);

    nb_cycles_send += cycle_stop - cycle_start;
  }

  return msg_size * nb_receivers;
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, long *msg_id)
{
  uint64_t cycle_start, cycle_stop;
  struct ipc_message ipc_msg;

  if (msg_size < MIN_MSG_SIZE)
  {
    msg_size = MIN_MSG_SIZE;
  }

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  rdtsc(cycle_start);
#ifdef ONE_QUEUE
  int recv_size = msgrcv(consumer_queue, &ipc_msg, sizeof(ipc_msg.mtext), core_id, 0);
#else
  int recv_size = msgrcv(consumer_queue, &ipc_msg, sizeof(ipc_msg.mtext), 0, 0);
#endif
  rdtsc(cycle_stop);

  nb_cycles_recv += cycle_stop - cycle_start;

#ifdef ONE_QUEUE
  if (ipc_msg.mtype != core_id)
  {
    printf("[consumer %i] Ooops, wrong message: %li instead of %i\n", core_id, ipc_msg.mtype, core_id);
  }
#endif

  int msg_size_in_msg = *((int*) ipc_msg.mtext);

  // get the id of the message
  *msg_id = *((long*) ((int*) ipc_msg.mtext + 1));

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
