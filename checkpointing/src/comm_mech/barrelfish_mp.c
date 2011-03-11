/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message passing
 */

#include <stdio.h>
#include <stdlib.h>
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

// Define NB_MESSAGES as the max number of messages in the channel
// Define URPC_MSG_WORDS as the size of the messages in uint64_t
// You can define USLEEP if you want to add a usleep(1) when busy waiting
// You can define NOP if you want to add a nop when busy waiting

#define NB_MSG_MAX_IN_TRANSIT NB_MESSAGES
//#define NB_MSG_MAX_IN_TRANSIT 1

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Barrelfish message passing **********/

static int node_id;
static int nb_nodes;

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
    printf("Did you touch %s?\n", p);
    perror("ftok error: ");
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

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;

  //todo
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  //todo
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  //todo
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  //todo
}

// send the message msg of size length to all the nodes
void IPC_send_all_nodes(void *msg, size_t length)
{
  //todo
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  //todo
  return 0;
}
