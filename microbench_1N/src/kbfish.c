/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing, kernel implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by Barrelfish message-passing **********/

#define MIN_MSG_SIZE (sizeof(char))

#define KBFISH_CHAR_DEV_FILE "/dev/kbfish"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static int request_size; // requests size in bytes

static uint64_t nb_cycles_send;
static uint64_t nb_cycles_recv;
static uint64_t nb_cycles_first_recv;

static int *conn; // conn[i] = the connection the producer uses to communicate with core i+1
static int consumer_connection; // this consumer's connection

// open wrapper which handles the errors
int Open(const char* pathname, int flags)
{
  int r = open(pathname, flags);
  if (r == -1)
  {
    perror(">>> Error while opening channel\n");
    printf(
        "Node %i experiences an error at %i when opening file %s with flags %x\n",
        core_id, __LINE__, pathname, flags);
    exit(-1);
  }
  return r;
}

// write wrapper which handles the errors
ssize_t Write(int fd, const void *buf, size_t count)
{
  int r = write(fd, buf, count);
  if (r == -1)
  {
    switch (errno)
    {
    case ENOMEM:
      printf(
          "Node %i: memory allocation failed while calling write @ %s:%i. Aborting.\n",
          core_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EFAULT:
      printf("Node %i: write buffer is invalid @ %s:%i. Aborting.\n", core_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EINTR:
      printf("Node %i: write call has been interrupted @ %s:%i. Aborting.\n",
          core_id, __FILE__, __LINE__);
      exit(-1);
      break;

    default:
      perror("Error in write");
      printf("Node %i: write error @ %s:%i. Aborting.\n", core_id, __FILE__,
          __LINE__);
      exit(-1);
      break;
    }
  }
  return r;
}

// read wrapper which handles the errors
ssize_t Read(int fd, void *buf, size_t count)
{
  int r = read(fd, buf, count);
  if (r == -1)
  {
    switch (errno)
    {
    case EAGAIN:
      printf(
          "Node %i: non-blocking ops and call would block while calling read @ %s:%i.\n",
          core_id, __FILE__, __LINE__);
      break;

    case EFAULT:
      printf("Node %i: read buffer is invalid @ %s:%i.\n", core_id, __FILE__,
          __LINE__);
      return 0;
      break;

    case EINTR:
      printf("Node %i: read call has been interrupted @ %s:%i. Aborting.\n",
          core_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EBADF:
      printf("Node %i: reader no longer online @ %s:%i. Aborting.\n", core_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EIO:
      printf("Node %i: checksum is incorrect @ %s:%i.\n", core_id, __FILE__,
          __LINE__);
      return 0;
      break;

    default:
      perror("Error in read");
      printf("Node %i: read error @ %s:%i. Aborting.\n", core_id, __FILE__,
          __LINE__);
      exit(-1);
      break;
    }
  }
  return r;
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

  nb_cycles_send = 0;
  nb_cycles_recv = 0;
  nb_cycles_first_recv = 0;
}

// Initialize resources for the producer
void IPC_initialize_producer(int _core_id)
{
  char chaname[256];

  core_id = _core_id;

  // ensure that the receivers have opened the file
  sleep(2);

  conn = (int*) malloc(sizeof(*conn) * nb_receivers);
  if (!conn)
  {
    perror("Connections allocation error");
    exit(errno);
  }

  int i;
  for (i = 0; i < nb_receivers; i++)
  {
    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, i);
    conn[i] = Open(chaname, O_RDWR);
  }
}

// Initialize resources for the consumers
void IPC_initialize_consumer(int _core_id)
{
  char chaname[256];

  core_id = _core_id;

  conn = NULL;
  snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, core_id - 1);
  consumer_connection = Open(chaname, O_RDWR | O_CREAT);
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

  for (i = 0; i < nb_receivers; i++)
  {
    close(conn[i]);
  }

  free(conn);
}

// Clean ressources created for the consumer.
void IPC_clean_consumer(void)
{
  close(consumer_connection);
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
    Write(conn[i], msg, msg_size);
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
  recv_size = Read(consumer_connection, msg, msg_size);
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
