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

// You can define ONE_QUEUE if you want to have only 1 queue shared by all the nodes (clients + paxos nodes).

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Barrelfish message passing **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int total_nb_nodes;

static int nb_messages_in_transit; // Barrelfish requires an acknowledgement of sent messages. We send one peridically
static size_t buffer_size;
static size_t connection_size;

static void* *shared_areas; // all the pieces of shared area

static struct urpc_connection **client_leader; // connection between client i and the leader
static struct urpc_connection **acceptor_node; // connection between the acceptor and node i

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
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  size_t urpc_msg_word = URPC_MSG_WORDS;
  size_t nb_messages = NB_MESSAGES;
  buffer_size = urpc_msg_word * 8 * nb_messages;
  nb_messages_in_transit = 0;

  shared_areas = (void**) malloc(sizeof(void*) * total_nb_nodes);
  if (!shared_areas)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < total_nb_nodes; i++)
  {
    // do not create a shared area for the acceptor
    if (i == 1)
    {
      shared_areas[i] = NULL;
      continue;
    }

    shared_areas[i] = init_shared_memory_segment(
        "/tmp/barrelfish_message_passing_microbench", connection_size, 'a' + i);
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  //todo
  static struct urpc_connection **client_leader; // connection between client i and the leader
  static struct urpc_connection **acceptor_node; // connection between the acceptor and node i

}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  //todo
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  for (int i = 0; i < total_nb_nodes; i++)
  {
    msgctl(ipc_queues[i], IPC_RMID, NULL);

#ifdef ONE_QUEUE
    break;
#endif
  }
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(struct ipc_message *msg, size_t length)
{
#ifdef ONE_QUEUE
  msg->mtype = 1 + 1;
  msgsnd(ipc_queues[0], msg, length, 0);
#else
  msg->mtype = 1;
  msgsnd(ipc_queues[1], msg, length, 0);
#endif
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(struct ipc_message *msg, size_t length)
{
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    if (i == 1)
    {
      continue;
    }

#ifdef ONE_QUEUE
    msg->mtype = i + 1;
    msgsnd(ipc_queues[0], msg, length, 0);
#else
    msg->mtype = 1;
    msgsnd(ipc_queues[i], msg, length, 0);
#endif
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(struct ipc_message *msg, size_t length)
{
#ifdef ONE_QUEUE
  msg->mtype = 0 + 1;
  msgsnd(ipc_queues[0], msg, length, 0);
#else
  msg->mtype = 1;
  msgsnd(ipc_queues[0], msg, length, 0);
#endif
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(struct ipc_message *msg, size_t length, int cid)
{
#ifdef ONE_QUEUE
  msg->mtype = cid + 1;
  msgsnd(ipc_queues[0], msg, length, 0);
#else
  msg->mtype = 1;
  msgsnd(ipc_queues[cid], msg, length, 0);
#endif
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(struct ipc_message *msg, size_t length)
{
#ifdef ONE_QUEUE
  return msgrcv(ipc_queues[0], msg, length, node_id + 1, 0);
#else
  return msgrcv(ipc_queues[node_id], msg, length, 0, 0);
#endif
}
