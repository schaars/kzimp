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
//#undef DEBUG

// Define NB_MESSAGES as the max number of messages in the channel
// Define URPC_MSG_WORDS as the size of the messages in uint64_t
// You can define USLEEP if you want to add a usleep(1) when busy waiting
// You can define NOP if you want to add a nop when busy waiting

//#define NB_MSG_MAX_IN_TRANSIT NB_MESSAGES
//#define NB_MSG_MAX_IN_TRANSIT 1

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Barrelfish message passing **********/

static size_t buffer_size;
static size_t connection_size;

static int node_id;
static int nb_nodes;

static void* **shared_areas; // a matrice of shared areas between the nodes
static struct urpc_connection **connections; // a matrix of urpc_connections between the nodes

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

  shared_areas = (void***) malloc(sizeof(void**) * nb_nodes);
  if (!shared_areas)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    shared_areas[i] = (void**) malloc(sizeof(void*) * nb_nodes);
    if (!shared_areas[i])
    {
      perror("Allocation error: ");
      exit(-1);
    }
  }

  connections = (struct urpc_connection**) malloc(
      sizeof(struct urpc_connection*) * nb_nodes);
  if (!connections)
  {
    perror("Allocation error: ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    connections[i] = (struct urpc_connection*) malloc(
        sizeof(struct urpc_connection) * nb_nodes);
    if (!connections[i])
    {
      perror("Allocation error: ");
      exit(-1);
    }
  }

  // ***** now that we have allocated the necessary, fill-in the matrix of shared areas
  for (int i = 0; i < nb_nodes; i++)
  {
    for (int j = 0; j < i; j++)
    {
      shared_areas[i][j] = init_shared_memory_segment(
          (char*) "/tmp/checkpointing_barrelfish_mp_shmem", connection_size,
          'a' + nb_nodes * i + j);
    }
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    shared_areas[i][i] = NULL;

    for (int j = i + 1; j < nb_nodes; j++)
    {
      shared_areas[i][j] = shared_areas[j][i];
    }
  }

#ifdef DEBUG
  printf("Shared areas addresses:\n");

  for (int i = 0; i < nb_nodes; i++)
  {
    for (int j = 0; j < nb_nodes; j++)
    {
      printf("%p\t", shared_areas[i][j]);
    }

    printf("\n");
  }
#endif
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  for (int j = 0; j < nb_nodes; j++)
  {
    if (node_id < j)
    {
#ifdef DEBUG
      printf("Node %i: [%i][%i] = true\n", node_id, node_id, j);
#endif

      urpc_transport_create(node_id, shared_areas[node_id][j], connection_size,
          buffer_size, &connections[node_id][j], true);
    }
    else if (node_id == j)
    {
      continue;
    }
    else
    {
#ifdef DEBUG
      printf("Node %i: [%i][%i] = false\n", node_id, node_id, j);
#endif

      urpc_transport_create(node_id, shared_areas[node_id][j], connection_size,
          buffer_size, &connections[node_id][j], false);
    }
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
  for (int i = 0; i < nb_nodes; i++)
  {
    for (int j = 0; j < i; j++)
    {
      shmdt(shared_areas[i][j]);
    }
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    free(connections[i]);
  }
  free(connections);

  for (int i = 0; i < nb_nodes; i++)
  {
    free(shared_areas[i]);
  }
  free(shared_areas);
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int j = 0; j < nb_nodes; j++)
  {
    if (node_id == j)
    {
      continue;
    }

#ifdef DEBUG
    printf("Node %i is sending on [%i][%i]\n", node_id, node_id, j);
#endif

    urpc_transport_send(&connections[node_id][j], msg, URPC_MSG_WORDS);
  }
}

// send the message msg of size length to the node nid
void IPC_send_unicast(void *msg, size_t length, int nid)
{
#ifdef DEBUG
  printf("Node %i is sending on [%i][%i]\n", node_id, node_id, nid);
#endif

  urpc_transport_send(&connections[node_id][nid], msg, URPC_MSG_WORDS);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// Non-blocking
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;

  for (int j = 0; j < nb_nodes; j++)
  {
    if (j == node_id)
    {
      continue;
    }

#ifdef DEBUG
    // too verbose
    //printf("Node %i is receiving on [%i][%i]\n", node_id, node_id, j);
#endif

    recv_size = urpc_transport_recv_nonblocking(&connections[node_id][j],
        (void*) msg, URPC_MSG_WORDS);

    if (recv_size > 0)
    {
      return recv_size * sizeof(uint64_t);
    }
  }

  return 0;
}
