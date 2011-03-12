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
static int nb_paxos_nodes;
static int nb_clients;
static int total_nb_nodes;

static struct mpsoc_ctrl client_to_leader; // client 1 -> leader
static struct mpsoc_ctrl leader_to_acceptor; // leader -> acceptor
static struct mpsoc_ctrl acceptor_multicast; // acceptor -> learners
static struct mpsoc_ctrl learners_to_client; // learners -> client 0

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  mpsoc_init(&client_to_leader,
      (char*) "/tmp/ulm_paxosInside_client_to_leader", 1, NB_MESSAGES, 0x1);

  mpsoc_init(&leader_to_acceptor,
      (char*) "/tmp/ulm_paxosInside_leader_to_acceptor", 1, NB_MESSAGES, 0x1);

  mpsoc_init(&learners_to_client,
      (char*) "/tmp/ulm_paxosInside_learners_to_client", 1, NB_MESSAGES, 0x1);

  // multicast mask is all the learners
  unsigned int multicast_bitmap_mask = 0;
  for (int i = 0; i < nb_paxos_nodes - 2; i++)
  {
    multicast_bitmap_mask = multicast_bitmap_mask | (1 << i);
  }

  mpsoc_init(&acceptor_multicast,
      (char*) "/tmp/ulm_paxosInside_acceptor_multicast", 1, NB_MESSAGES,
      multicast_bitmap_mask);
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

static void clean_node(void)
{
  mpsoc_destroy(&client_to_leader);
  mpsoc_destroy(&leader_to_acceptor);
  mpsoc_destroy(&learners_to_client);
  mpsoc_destroy(&acceptor_multicast);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  clean_node();
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  clean_node();
}

// allocate a new message in the shared memory
// we use node_id to differentiate which instance has to be used:
//  node 0                  -> leader_to_acceptor
//  node 1                  -> acceptor_multicast
//  node i < nb_paxos_node  -> learners_to_client
//  node i >= nb_paxos_node -> client_to_leader
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer, int dest)
{
  if (node_id == 0)
  {
    return mpsoc_alloc(&leader_to_acceptor, len, msg_pos_in_ring_buffer);
  }
  else if (node_id == 1)
  {
    return mpsoc_alloc(&acceptor_multicast, len, msg_pos_in_ring_buffer);
  }
  else if (node_id >= nb_paxos_nodes)
  {
    return mpsoc_alloc(&client_to_leader, len, msg_pos_in_ring_buffer);
  }
  else
  {
    return mpsoc_alloc(&learners_to_client, len, msg_pos_in_ring_buffer);
  }
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length, int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&leader_to_acceptor, msg, length, msg_pos_in_ring_buffer, 0);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&acceptor_multicast, msg, length, msg_pos_in_ring_buffer, -1);
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&client_to_leader, msg, length, msg_pos_in_ring_buffer, 0);
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&learners_to_client, msg, length, msg_pos_in_ring_buffer, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  if (node_id == 0)
  {
    return (size_t) mpsoc_recvfrom(&client_to_leader, msg, length, 0);
  }
  else if (node_id == 1)
  {
    return (size_t) mpsoc_recvfrom(&leader_to_acceptor, msg, length, 0);
  }
  else if (node_id >= nb_paxos_nodes)
  {
    return (size_t) mpsoc_recvfrom(&learners_to_client, msg, length, 0);
  }
  else
  {
    return (size_t) mpsoc_recvfrom(&acceptor_multicast, msg, length, node_id);
  }
}
