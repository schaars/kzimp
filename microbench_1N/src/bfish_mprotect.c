/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "ipc_interface.h"
#include "../../kbfishmem/bfishmprotect/bfishmprotect.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by Barrelfish message-passing **********/

// Define NB_MESSAGES as the size of the channels
// Define MESSAGE_BYTES as the max size of the messages in bytes
// Define WAIT_TYPE as USLEEP or BUSY


#define MIN_MSG_SIZE (sizeof(char))

#define KBFISH_MEM_CHAR_DEV_FILE "/dev/kbfishmem"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

static struct ump_channel *conn; // conn[i] = the connection the producer uses to communicate with core i+1
static struct ump_channel consumer_connection; // this consumer's connection


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

  char chaname[256];
  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, i);
    create_channel(chaname, i);
  }
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  char chaname[256];

  core_id = _core_id;

  // ensure that the receivers have opened the file
  sleep(2);

  conn = (struct ump_channel*) malloc(sizeof(*conn) * nb_receivers);
  if (!conn)
  {
    perror("Connections allocation error");
    exit(errno);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, i);
    conn[i] = open_channel(chaname, i, NB_MESSAGES, MESSAGE_BYTES, 0);
  }
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  char chaname[256];

  core_id = _core_id;

  conn = NULL;
  snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, core_id - 1);
  consumer_connection = open_channel(chaname, core_id - 1, NB_MESSAGES,
      MESSAGE_BYTES, 1);
}

// Clean ressources created for both the producer and the consumer.
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    destroy_channel(i);
  }
}

// Clean ressources created for the producer.
void IPC_clean_producer(void)
{
  int i;

  for (i = 0; i < nb_receivers; i++)
  {
    close_channel(&conn[i]);
  }

  free(conn);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close_channel(&consumer_connection);
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
    send_msg(&conn[i], msg, msg_size);
    rdtsc(cycle_stop);

    nb_cycles_send += cycle_stop - cycle_start;
  }

  free(msg);
}

// Get a message for this core
// return the size of the message if it is valid, 0 otherwise
// Place in *msg_id the id of this message
int IPC_receive(int msg_size, char *msg_id)
{
  char *msg;
  int recv_size;

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
  recv_size = recv_msg(&consumer_connection, msg, msg_size);
  rdtsc(cycle_stop);

  nb_cycles_recv += cycle_stop - cycle_start;
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
