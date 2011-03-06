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

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  unsigned int multicast_bitmap_mask = 0;
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    // the acceptor do sends messages to himself :)
    multicast_bitmap_mask = multicast_bitmap_mask | (1 << i);
  }

  printf("[IPC_initialize] The multicast mask is %x\n", multicast_bitmap_mask);

  mpsoc_init((char*) "/tmp/ulm_paxosInside", total_nb_nodes, NB_MESSAGES,
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

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  mpsoc_destroy();
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  mpsoc_destroy();
}

// allocate a new message in the shared memory
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer)
{
  return mpsoc_alloc(len, msg_pos_in_ring_buffer);
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length, int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(msg, length, msg_pos_in_ring_buffer, 1);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(msg, length, msg_pos_in_ring_buffer, -1);
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(msg, length, msg_pos_in_ring_buffer, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(msg, length, msg_pos_in_ring_buffer, cid);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  return (size_t) mpsoc_recvfrom(msg, length, node_id);
}
