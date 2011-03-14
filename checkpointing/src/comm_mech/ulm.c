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

static struct mpsoc_ctrl *multicast_from_node; // node i -> all nodes but i
static struct mpsoc_ctrl *all_nodes_to_node; // all nodes -> node i, for all i

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

  multicast_from_node = (struct mpsoc_ctrl*) malloc(sizeof(struct mpsoc_ctrl)
      * nb_nodes);
  if (!multicast_from_node)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  all_nodes_to_node = (struct mpsoc_ctrl*) malloc(sizeof(struct mpsoc_ctrl)
      * nb_nodes);
  if (!all_nodes_to_node)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  char filename[256];

  for (int i = 0; i < nb_nodes; i++)
  {
    unsigned int multicast_mask = 0;
    for (int j = 0; j < nb_nodes; j++)
    {
      if (j != i)
      {
        multicast_mask |= (1 << j);
      }
    }

    sprintf(filename, "%s_%i", "/tmp/ulm_checkpointing_multicast_from_node", i);

    mpsoc_init(&multicast_from_node[i], filename, nb_nodes, NB_MESSAGES,
        multicast_mask);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    sprintf(filename, "%s_%i", "/tmp/ulm_checkpointing_all_nodes_to_node", i);

    mpsoc_init(&all_nodes_to_node[i], filename, 1, NB_MESSAGES, 0x1);
  }
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
  for (int i = 0; i < nb_nodes; i++)
  {
    mpsoc_destroy(&multicast_from_node[i]);
    mpsoc_destroy(&all_nodes_to_node[i]);
  }
}

// allocate a message in shared memory.
// If dest is -1, then this message is going to be multicast.
// Otherwise it is sent to node dest.
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer, int dest)
{
  if (dest == -1)
  {
    return mpsoc_alloc(&multicast_from_node[node_id], len,
        msg_pos_in_ring_buffer);
  }
  else
  {
    return mpsoc_alloc(&all_nodes_to_node[dest], len, msg_pos_in_ring_buffer);
  }
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length, int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&multicast_from_node[node_id], msg, length,
      msg_pos_in_ring_buffer, -1);
}

// send the message msg of size length to the node nid
void IPC_send_unicast(void *msg, size_t length, int nid,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&all_nodes_to_node[nid], msg, length, msg_pos_in_ring_buffer, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// Non-blocking
size_t IPC_receive(void *msg, size_t length)
{
  ssize_t recv_size = 0;

  for (int i = 0; i < nb_nodes; i++)
  {
    recv_size = mpsoc_recvfrom_nonblocking(&multicast_from_node[i], msg,
        length, node_id);

    if (recv_size > 0)
    {
      return (size_t) recv_size;
    }
  }

  recv_size = mpsoc_recvfrom_nonblocking(&all_nodes_to_node[node_id], msg,
      length, 0);

  // if there is nothing to read then mpsoc_recvfrom_nonblocking returns 0,
  // thus we can directly return recv_size :)

  return (size_t) recv_size;
}
