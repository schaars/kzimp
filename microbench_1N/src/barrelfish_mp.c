/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "ipc_interface.h"
#include "urpc.h"
#include "urpc_transport.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by Barrelfish message-passing **********/

//#define BUFFER_SIZE (URPC_MSG_WORDS*8*NB_MESSAGES)
//#define CONNECTION_SIZE (BUFFER_SIZE * 2 + 2 * URPC_CHANNEL_SIZE)

// size of an ack in uint64_t
#define ACK_SIZE 1

#define MIN_MSG_SIZE (URPC_MSG_WORDS*sizeof(uint64_t))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int nb_messages_in_transit; // Barrelfish requires an acknowledgement of sent messages. We send one peridically

static size_t buffer_size;
static size_t connection_size;

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

static void* *shared_areas; // shared_areas[i] = (void*) shared are between producer and core i+1

static struct urpc_connection *conn; // conn[i] = the connection the producer uses to communicate with core i+1
static struct urpc_connection *consumer_connection; // this consumer's connection


/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
void* init_shared_memory_segment(char *p, size_t s, int i)
{
  key_t key;
  int shmid;
  void* ret;

  ret = NULL;

  key = ftok(p, i);
  if (key == -1)
  {
    perror("ftok error: ");
    printf("Did you touch %s?\n", p);
    return ret;
  }

  printf("Size of the shared memory segment to create: %qu\n",
      (unsigned long long) s);

  shmid = shmget(key, s, IPC_CREAT | 0666);
  if (shmid == -1)
  {
    int errsv = errno;

    perror("shmget error: ");

    switch (errsv)
    {
    case EACCES:
      printf("errno = EACCES\n");
      break;
    case EINVAL:
      printf("errno = EINVAL\n");
      break;
    case ENFILE:
      printf("errno = ENFILE\n");
      break;
    case ENOENT:
      printf("errno = ENOENT\n");
      break;
    case ENOMEM:
      printf("errno = ENOMEM\n");
      break;
    case ENOSPC:
      printf("errno = ENOSPC\n");
      break;
    case EPERM:
      printf("errno = EPERM\n");
      break;
    default:
      printf("errno = %i\n", errsv);
      break;
    }

    return ret;
  }

  ret = shmat(shmid, NULL, 0);

  return ret;
}

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

  size_t urpc_msg_word = URPC_MSG_WORDS;
  size_t nb_messages = NB_MESSAGES;
  buffer_size = urpc_msg_word * 8 * nb_messages;
  connection_size = buffer_size * 2 + 2 * URPC_CHANNEL_SIZE;

  nb_cycles_send = 0;
  nb_cycles_recv = 0;
  nb_cycles_first_recv = 0;

  nb_messages_in_transit = 0;

  shared_areas = (void**) malloc(sizeof(void*) * nb_receivers);
  if (!shared_areas)
  {
    perror("Allocation error");
    exit(errno);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    shared_areas[i] = init_shared_memory_segment(
        "/tmp/barrelfish_message_passing_microbench", connection_size, 'a' + i);

#ifdef DEBUG
    printf("New shared area @ %p, len = %li\n", shared_areas[i],
        CONNECTION_SIZE);
#endif
  }
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  core_id = _core_id;

  conn = (struct urpc_connection*) malloc(sizeof(struct urpc_connection)
      * nb_receivers);
  if (!conn)
  {
    perror("Connections allocation error");
    exit(errno);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    urpc_transport_create(0, shared_areas[i], connection_size, buffer_size,
        &conn[i], true);
  }

  sleep(2);
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  core_id = _core_id;

  sleep(1);

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    if (i != core_id - 1)
      shmdt(shared_areas[i]);
  }

  consumer_connection = (struct urpc_connection*) malloc(
      sizeof(struct urpc_connection));
  if (!consumer_connection)
  {
    perror("Connection allocation error");
    exit(errno);
  }

  urpc_transport_create(core_id, shared_areas[core_id - 1], connection_size,
      buffer_size, consumer_connection, false);
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  int i;

  free(conn);
  for (i = 0; i < nb_receivers; i++)
  {
    shmdt(shared_areas[i]);
  }
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  free(consumer_connection);
  shmdt(shared_areas[core_id - 1]);
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
      core_id, msg[0], msg_size, nb_receivers);
#endif

  for (i = 0; i < nb_receivers; i++)
  {
    // writing the content
    rdtsc(cycle_start);
    urpc_transport_send(&conn[i], msg, URPC_MSG_WORDS);
    rdtsc(cycle_stop);

    nb_cycles_send += cycle_stop - cycle_start;
  }

  nb_messages_in_transit++;
  if (nb_messages_in_transit == NB_MESSAGES)
  {
    // receive a message
    for (i = 0; i < nb_receivers; i++)
    {
      urpc_transport_recv(&conn[i], (void*) msg, ACK_SIZE);
    }
    nb_messages_in_transit = 0;
  }

  free(msg);
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
  char *msg;
  int old_msg_size;

  old_msg_size = msg_size;
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

  uint64_t cycle_start, cycle_stop;

  rdtsc(cycle_start);
  int recv_size = urpc_transport_recv(consumer_connection, (void*) msg,
      URPC_MSG_WORDS);
  rdtsc(cycle_stop);

  nb_cycles_recv += cycle_stop - cycle_start;
  if (nb_cycles_first_recv == 0)
  {
    nb_cycles_first_recv = nb_cycles_recv;
  }

  // urpc_transport_recv returns URPC_MSG_WORDS. We want the size of the message in bytes
  recv_size *= sizeof(uint64_t);

  // get the id of the message
  *msg_id = msg[0];

#ifdef DEBUG
  printf(
      "[consumer %i] received message %i of size %i, should be %i\n",
      core_id, *msg_id, recv_size, msg_size);
#endif

  nb_messages_in_transit++;
  if (nb_messages_in_transit == NB_MESSAGES)
  {
    // send a message
    urpc_transport_send(consumer_connection, msg, ACK_SIZE);
    nb_messages_in_transit = 0;
  }

  free(msg);

  if (recv_size == msg_size)
  {
    return msg_size;
  }
  else if (old_msg_size == 1)
  {
    return old_msg_size;
  }
  else
  {
    return 0;
  }
}
