/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: ULM (User-level Local Multicast)
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
#include "mpsoc.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define NB_MESSAGES as the max number of messages in the channel
// Define MESSAGE_MAX_SIZE as the max size of a message in the channel
// Define ULM because the interface is a little different than the usual
// You can define USLEEP if you want to add a usleep(1) when busy waiting
// You can define NOP if you want to add a nop when busy waiting

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by ULM **********/

static int node_id;
static int nb_nodes;

static struct mpsoc_ctrl multicast_0_to_all; // node 0          -> all but 0
static struct mpsoc_ctrl all_to_0; // all nodes but 0 -> node 0

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

  unsigned int multicast_mask = 0;
  for (int i = 1; i < nb_nodes; i++)
  {
    multicast_mask |= (1 << i);
  }

  mpsoc_init(&multicast_0_to_all,
      (char*) "/tmp/ulm_checkpointing_multicast_0_to_all", nb_nodes,
      NB_MESSAGES, multicast_mask);

  mpsoc_init(&all_to_0, (char*) "/tmp/ulm_checkpointing_all_to_0", 1,
      NB_MESSAGES, 0x1);
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  mpsoc_destroy(&multicast_0_to_all);
  mpsoc_destroy(&all_to_0);
}

// allocate a message in shared memory.
// If dest is -1, then this message is going to be multicast.
// Otherwise it is sent to node 0.
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer, int dest)
{
  if (dest == -1)
  {
    return mpsoc_alloc(&multicast_0_to_all, len, msg_pos_in_ring_buffer);
  }
  else
  {
    return mpsoc_alloc(&all_to_0, len, msg_pos_in_ring_buffer);
  }
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length, int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&multicast_0_to_all, msg, length, msg_pos_in_ring_buffer, -1);
}

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&all_to_0, msg, length, msg_pos_in_ring_buffer, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  ssize_t recv_size;

  if (node_id == 0)
  {
    recv_size = mpsoc_recvfrom(&all_to_0, msg, length, node_id);
  }
  else
  {
    recv_size = mpsoc_recvfrom(&multicast_0_to_all, msg, length, node_id);
  }

  return (size_t) recv_size;
}
