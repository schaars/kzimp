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

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Barrelfish message passing **********/

static size_t buffer_size;
static size_t connection_size;

static int node_id;
static int nb_nodes;

static void* *shared_areas; // shared area between nodes 0 and i, for all the nodes
static struct urpc_connection *connections; // urpc_connections between nodes 0 and nodes i

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

  size_t urpc_msg_word = URPC_MSG_WORDS;
  size_t nb_messages = NB_MESSAGES;
  buffer_size = urpc_msg_word * 8 * nb_messages;
  connection_size = buffer_size * 2 + 2 * URPC_CHANNEL_SIZE;

  // ***** allocate the necessary
  shared_areas = (void**) malloc(sizeof(void*) * nb_nodes);
  if (!shared_areas)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  connections = (struct urpc_connection*) malloc(sizeof(struct urpc_connection)
      * nb_nodes);
  if (!connections)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  // ***** now that we have allocated the necessary, fill-in the matrix of shared areas
  // communication from 0 to 0 does not exist
  for (int i = 1; i < nb_nodes; i++)
  {
    shared_areas[i] = init_shared_memory_segment(
        (char*) "/tmp/checkpointing_barrelfish_mp_shmem", connection_size, 'a'
            + i);
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  if (node_id == 0)
  {
    // communication from 0 to 0 does not exist
    for (int i = 1; i < nb_nodes; i++)
    {
      urpc_transport_create(node_id, shared_areas[i], connection_size,
          buffer_size, &connections[i], true);
    }
  }
  else
  {
    urpc_transport_create(node_id, shared_areas[node_id], connection_size,
        buffer_size, &connections[node_id], false);
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    shmdt(shared_areas[i]);
  }

  free(connections);
  free(shared_areas);
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    urpc_transport_send(&connections[i], msg, URPC_MSG_WORDS_CHKPT);
  }
}

// send the message msg of size length to the node nid
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  urpc_transport_send(&connections[node_id], msg, URPC_MSG_WORDS);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size;

  if (node_id == 0)
  {
    while (1)
    {
      for (int i = 1; i < nb_nodes; i++)
      {
        recv_size = urpc_transport_recv_nonblocking(&connections[i],
            (void*) msg, URPC_MSG_WORDS);

        if (recv_size > 0)
        {
          return recv_size * sizeof(uint64_t);
        }
      }
    }
  }
  else
  {
    recv_size = urpc_transport_recv(&connections[node_id], (void*) msg,
        URPC_MSG_WORDS_CHKPT);
    return recv_size * sizeof(uint64_t);
  }

  return 0;
}
