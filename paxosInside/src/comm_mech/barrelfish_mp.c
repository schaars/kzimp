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
#include "../MessageTag.h"
#include "../Message.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define NB_MESSAGES as the max number of messages in the channel
// Define URPC_MSG_WORDS as the size of the messages in uint64_t
// You can define USLEEP if you want to add a usleep(1) when busy waiting
// You can define NOP if you want to add a nop when busy waiting

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

static struct urpc_connection *client_leader; // connection between client i and the leader
static struct urpc_connection *acceptor_node; // connection between the acceptor and node i

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
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  size_t urpc_msg_word = URPC_MSG_WORDS;
  size_t nb_messages = NB_MESSAGES;
  buffer_size = urpc_msg_word * 8 * nb_messages;
  connection_size = buffer_size * 2 + 2 * URPC_CHANNEL_SIZE;
  nb_messages_in_transit = 0;

  shared_areas = (void**) malloc(sizeof(void*) * total_nb_nodes);
  if (!shared_areas)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < total_nb_nodes; i++)
  {
    // the acceptor uses the shared area of the leader to communicate with him
    if (i == 1)
    {
      shared_areas[i] = NULL;
      continue;
    }

    shared_areas[i] = init_shared_memory_segment(
        (char*) "/tmp/barrelfish_message_passing_microbench", connection_size,
        'a' + i);
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  if (node_id == 0)
  {
    client_leader = (struct urpc_connection*) malloc(
        sizeof(struct urpc_connection) * nb_clients);
    if (!client_leader)
    {
      perror("Clients connections allocation error");
      exit(errno);
    }

    for (int i = 0; i < nb_clients; i++)
    {
      urpc_transport_create(0, shared_areas[nb_paxos_nodes + i],
          connection_size, buffer_size, &client_leader[i], true);
    }
  }
  else
  {
    client_leader = NULL;
  }

  if (node_id == 1)
  {
    acceptor_node = (struct urpc_connection*) malloc(
        sizeof(struct urpc_connection) * nb_paxos_nodes);
    if (!acceptor_node)
    {
      perror("Acceptor connections allocation error");
      exit(errno);
    }

    for (int i = 0; i < nb_paxos_nodes; i++)
    {
      if (i == 1)
      {
        continue;
      }

      urpc_transport_create(1, shared_areas[i], connection_size, buffer_size,
          &acceptor_node[i], true);
    }
  }
  else
  {
    acceptor_node = (struct urpc_connection*) malloc(
        sizeof(struct urpc_connection) * 1);
    if (!acceptor_node)
    {
      perror("Acceptor connection allocation error");
      exit(errno);
    }

    urpc_transport_create(node_id, shared_areas[node_id], connection_size,
        buffer_size, &acceptor_node[0], false);
  }
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  // clients use client_leader[0] only
  acceptor_node = NULL;

  client_leader = (struct urpc_connection*) malloc(
      sizeof(struct urpc_connection) * 1);
  if (!client_leader)
  {
    perror("Clients connection allocation error");
    exit(errno);
  }

  urpc_transport_create(node_id, shared_areas[node_id], connection_size,
      buffer_size, &client_leader[0], false);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  if (node_id == 0)
  {
    free(client_leader);
    free(acceptor_node);

    for (int i = 0; i < total_nb_nodes; i++)
    {
      shmdt(shared_areas[i]);
    }
  }
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  free(client_leader);

  for (int i = 0; i < total_nb_nodes; i++)
  {
    shmdt(shared_areas[i]);
  }
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  urpc_transport_send(&acceptor_node[0], msg, URPC_MSG_WORDS);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  Message m;

    
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    if (i == 1)
    {
      continue;
    }

#ifdef DEBUG
    printf("Node %i is going to send to %i\n", node_id, i);
#endif

    urpc_transport_send(&acceptor_node[i], msg, URPC_MSG_WORDS);
  }

#ifdef DEBUG
  printf("Node %i is going to receive acks from the learners\n", node_id);
#endif

  // the leader (node 0) does not send acks
  // the acceptor (this node, node 1) does not send messages to himself
  for (int i = 2; i < nb_paxos_nodes; i++)
  {
    urpc_transport_recv(&acceptor_node[i], (void*) m.content(),
		    URPC_MSG_WORDS);

#ifdef DEBUG
    printf("Node %i has received an ack from %i\n", node_id, i); 
#endif
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  urpc_transport_send(&client_leader[0], msg, URPC_MSG_WORDS);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  urpc_transport_send(&client_leader[cid - nb_paxos_nodes], msg, URPC_MSG_WORDS);
}

// return the size in uint64_t
size_t recv_for_node0(void *msg, size_t length)
{
  size_t recv_size;
  while (1)
  {
    // recv from the acceptor
    recv_size = urpc_transport_recv_nonblocking(&acceptor_node[0], (void*) msg,
        URPC_MSG_WORDS);

    // there is a message from this client
    if (recv_size > 0)
    {
      return recv_size;
    }

    // recv from the clients
    for (int i = 0; i < nb_clients; i++)
    {
      recv_size = urpc_transport_recv_nonblocking(&client_leader[i],
          (void*) msg, URPC_MSG_WORDS);

      // there is a message from this client
      if (recv_size > 0)
      {
        return recv_size;
      }
    }

    // sleep
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size;

  if (node_id == 0)
  {
    recv_size = recv_for_node0(msg, length);
  }
  else if (node_id == 1)
  {
    recv_size = urpc_transport_recv(&acceptor_node[0], (void*) msg,
        URPC_MSG_WORDS);
  }
  else if (node_id < nb_paxos_nodes)
  {
    recv_size = urpc_transport_recv(&acceptor_node[0], (void*) msg,
        URPC_MSG_WORDS);

    // send a message
    Message m( BARRELFISH_ACK);

#ifdef DEBUG
    printf("Node %i is sending an ack to the acceptor\n", node_id);
#endif

    urpc_transport_send(&acceptor_node[0], m.content(), URPC_MSG_WORDS);
  }
  else
  {
    recv_size = urpc_transport_recv(&client_leader[0], (void*) msg,
        URPC_MSG_WORDS);
  }

  return recv_size * sizeof(uint64_t);
}
