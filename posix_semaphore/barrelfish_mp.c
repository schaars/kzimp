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

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by Barrelfish message-passing **********/

//#define NB_MESSAGES 10
#define BUFFER_SIZE (URPC_MSG_WORDS*8*NB_MESSAGES)
#define CONNECTION_SIZE (BUFFER_SIZE * 2 + 2 * URPC_CHANNEL_SIZE)

#define MIN_MSG_SIZE (URPC_MSG_WORDS*sizeof(uint64_t))

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes
static int nb_messages_in_transit; // Barrelfish requires an acknowledgement of sent messages. We send one peridically

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
    return ret;
  }

  printf("Size of the shared memory segment to create: %d\n", (int) s);

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
        "/tmp/barrelfish_message_passing_microbench", CONNECTION_SIZE, 'a' + i);

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
    urpc_transport_create(0, shared_areas[i], CONNECTION_SIZE, BUFFER_SIZE,
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

  urpc_transport_create(core_id, shared_areas[core_id - 1], CONNECTION_SIZE,
      BUFFER_SIZE, consumer_connection, false);
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

// Send a message to all the cores
// The message id will be msg_id
// Return the total sent payload (i.e. size of the messages times number of consumers)
int IPC_sendToAll(int msg_size, long msg_id)
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

  // malloc is lazy: the pages may not be really allocated yet.
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

  for (i = 0; i < nb_receivers; i++)
  {
    // writing the content
    urpc_transport_send(&conn[i], msg, URPC_MSG_WORDS);
  }

  nb_messages_in_transit++;
  if (nb_messages_in_transit == NB_MESSAGES)
  {
    // receive a message
    for (i = 0; i < nb_receivers; i++)
    {
      urpc_transport_recv(&conn[i], (void*) msg, URPC_MSG_WORDS);
    }
    nb_messages_in_transit = 0;
  }

  free(msg);

  return msg_size * nb_receivers;
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

  msg = (char*) malloc(sizeof(char) * msg_size);
  if (!msg)
  {
    perror("IPC_receive allocation error! ");
    exit(errno);
  }

#ifdef DEBUG
  printf("Waiting for a new message\n");
#endif

  int recv_size = urpc_transport_recv(consumer_connection, (void*) msg,
      URPC_MSG_WORDS);

  // urpc_transport_recv returns URPC_MSG_WORDS. We want the size of the message in bytes
  recv_size *= sizeof(uint64_t);

  int msg_size_in_msg = *((int*) msg);

  // get the id of the message
  *msg_id = *((long*) ((int*) msg + 1));

#ifdef DEBUG
  printf(
      "[consumer %i] received message %li of size %i, should be %i (%i in the message)\n",
      core_id, *msg_id, recv_size, msg_size, msg_size_in_msg);
#endif

  nb_messages_in_transit++;
  if (nb_messages_in_transit == NB_MESSAGES)
  {
    // send a message
    urpc_transport_send(consumer_connection, msg, URPC_MSG_WORDS);
    nb_messages_in_transit = 0;
  }

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
