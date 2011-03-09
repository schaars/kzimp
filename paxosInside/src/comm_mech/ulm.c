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

static struct mpsoc_ctrl acceptor_multicast; // multicast
static struct mpsoc_ctrl leader_to_acceptor; // unicast
static struct mpsoc_ctrl clients_to_leader; // unicast
static struct mpsoc_ctrl *leader_to_client; // unicast, 1 for each client

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
    // the acceptor does not send messages to himself :)
    if (i == 1)
    {
      continue;
    }

    multicast_bitmap_mask = multicast_bitmap_mask | (1 << i);
  }

  printf("[IPC_initialize] The multicast mask is %x\n", multicast_bitmap_mask);

  mpsoc_init(&acceptor_multicast,
      (char*) "/tmp/ulm_paxosInside_acceptor_multicast", nb_paxos_nodes,
      NB_MESSAGES, multicast_bitmap_mask);

  mpsoc_init(&leader_to_acceptor,
      (char*) "/tmp/ulm_paxosInside_leader_to_acceptor", 1, 2, 0x1);

  mpsoc_init(&clients_to_leader,
      (char*) "/tmp/ulm_paxosInside_clients_to_leader", 1, nb_clients + 1, 0x1);

  leader_to_client = (struct mpsoc_ctrl*) malloc(sizeof(struct mpsoc_ctrl)
      * nb_clients);
  if (!leader_to_client)
  {
    perror("[IPC_initialize] allocation error");
    exit(errno);
  }

  char filename[256];
  for (int i = 0; i < nb_clients; i++)
  {
    sprintf(filename, "%s_%i", "/tmp/ulm_paxosInside_leader_to_client", i);
    mpsoc_init(&leader_to_client[i], filename, 1, 2, 0x1);
  }
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
  mpsoc_destroy(&acceptor_multicast);
  mpsoc_destroy(&leader_to_acceptor);
  mpsoc_destroy(&clients_to_leader);

  for (int i = 0; i < nb_clients; i++)
  {
    mpsoc_destroy(&leader_to_client[i]);
  }

  free(leader_to_client);
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
//  node 0                  -> leader_to_acceptor OR leader_to_client[cid]
//  node 1                  -> acceptor_multicast
//  node i < nb_paxos_node  ->      -
//  node i >= nb_paxos_node -> clients_to_leader
void* IPC_ulm_alloc(size_t len, int *msg_pos_in_ring_buffer, int dest)
{
  if (node_id == 0)
  {
    if (dest == 1)
    {
      return mpsoc_alloc(&leader_to_acceptor, len, msg_pos_in_ring_buffer);
    }
    else
    {
      return mpsoc_alloc(&leader_to_client[dest - nb_paxos_nodes], len,
          msg_pos_in_ring_buffer);
    }
  }
  else if (node_id == 1)
  {
    return mpsoc_alloc(&acceptor_multicast, len, msg_pos_in_ring_buffer);
  }
  else if (node_id >= nb_paxos_nodes)
  {
    return mpsoc_alloc(&clients_to_leader, len, msg_pos_in_ring_buffer);
  }
  else
  {
    printf(
        "Node %i is attempting to send a message to %i whereas it should not.\n",
        node_id, dest);
    return NULL;
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
  printf("Acceptor going to multicast a messag\n");
  mpsoc_sendto(&acceptor_multicast, msg, length, msg_pos_in_ring_buffer, -1);
  printf("Acceptor has sent a multicast message\n");
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length,
    int msg_pos_in_ring_buffer)
{
  mpsoc_sendto(&clients_to_leader, msg, length, msg_pos_in_ring_buffer, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid,
    int msg_pos_in_ring_buffer)
{
  printf("Leader is going to send a message to client %i\n", cid);
  mpsoc_sendto(&leader_to_client[cid - nb_paxos_nodes], msg, length,
      msg_pos_in_ring_buffer, 0);
}

size_t recv_for_node0(void *msg, size_t length)
{
  size_t ret = -1;

  while (1)
  {
    // recv from the clients
    ret = mpsoc_recvfrom_nonblocking(&clients_to_leader, msg, length, node_id);
    if (ret > 0)
    {
      break;
    }

    // recv from the acceptor
    ret = mpsoc_recvfrom_nonblocking(&acceptor_multicast, msg, length, node_id);
    if (ret > 0)
    {
      break;
    }

#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

  return ret;
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  if (node_id == 0)
  {
    return recv_for_node0(msg, length);
  }
  else if (node_id == 1)
  {
    printf("Acceptor (node %i) waiting for a response\n", node_id);
    return (size_t) mpsoc_recvfrom(&leader_to_acceptor, msg, length, 0);
  }
  else if (node_id > nb_paxos_nodes)
  {
    printf("Client %i waiting for a response\n", node_id);
    return (size_t) mpsoc_recvfrom(&leader_to_client[node_id - nb_paxos_nodes],
        msg, length, 0);
  }
  else
  {
    printf("Node %i waiting for a Learn\n", node_id);
    return (size_t) mpsoc_recvfrom(&acceptor_multicast, msg, length, node_id);
  }
}
