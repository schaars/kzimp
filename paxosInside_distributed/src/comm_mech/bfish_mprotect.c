/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
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
#include "../../../kbfishmem/bfishmprotect/bfishmprotect.h"
#include "../MessageTag.h"
#include "../Message.h"

// debug macro
#define DEBUG
#undef DEBUG


/********** All the variables needed by Barrelfish message passing **********/

// Define NB_MESSAGES as the size of the channels
// Define MESSAGE_BYTES as the max size of the messages in bytes
// Define WAIT_TYPE as USLEEP or BUSY

#define KBFISH_MEM_CHAR_DEV_FILE "/dev/kbfishmem"

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static struct ump_channel *clients_to_leader; // connection between client i and the leader
static struct ump_channel leader_to_acceptor; // connection between leader and acceptor
static struct ump_channel *acceptor_to_learners; // connection between the acceptor and learner i
static struct ump_channel **learners_to_clients; // connection between the learner l and the client c


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  //todo

  /*
  if (node_id == 0)
  {
    nb_messages_in_transit_multi = (int*) malloc(sizeof(int) * nb_clients);
    if (!nb_messages_in_transit_multi)
    {
      perror("Allocation error");
      exit(errno);
    }

    for (int i = 0; i < nb_clients; i++)
    {
      nb_messages_in_transit_multi[i] = 0;
    }
  }
  else
  {
    nb_messages_in_transit_multi = NULL;
  }
*/
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  //todo

  /*
  nb_messages_in_transit_multi = (int*) malloc(sizeof(int) * nb_learners);
  if (!nb_messages_in_transit_multi)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < nb_learners; i++)
  {
    nb_messages_in_transit_multi[i] = 0;
  }
  */
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  //todo

  /*
  if (node_id == 0)
  {
    free(nb_messages_in_transit_multi);
  }
  */
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  //todo

  /*
  free(nb_messages_in_transit_multi);
  */
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  //todo

  //urpc_transport_send(&leader_to_acceptor, msg, length / sizeof(uint64_t));
}

// send the message msg of size length to all the learners
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int l = 0; l < nb_learners; l++)
  {
#ifdef DEBUG
    printf("Node %i is going to send a multicast message to learner %i\n",
        node_id, l + 2);
#endif
    //todo
    //urpc_transport_send(&acceptor_to_learners[l], msg, length
    //    / sizeof(uint64_t));
  }

#ifdef DEBUG
  printf("Node %i has finished to send multicast messages\n", node_id);
#endif
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  //todo

  //urpc_transport_send(&clients_to_leader[node_id - nb_paxos_nodes], msg, length
  //    / sizeof(uint64_t));
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
#ifdef DEBUG
  printf("Node %i is going to send a message to client %i\n", node_id, cid);
#endif

  //todo
  //urpc_transport_send(&learners_to_clients[node_id - 2][0], msg, length
  //    / sizeof(uint64_t));
}

// return a message received by node 0
// This is the leader. It receives messages from the clients
size_t recv_for_node0(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  //todo

  /*
  while (1)
  {
    for (int i = 0; i < nb_clients; i++)
    {
      recv_size = urpc_transport_recv_nonblocking(&clients_to_leader[i],
          (void*) msg, length / sizeof(uint64_t));

      if (recv_size > 0)
      {
        nb_messages_in_transit_multi[i]++;

        if (nb_messages_in_transit_multi[i] == NB_MSG_MAX_IN_TRANSIT)
        {
#ifdef DEBUG
          printf("Node %i is sending an ack to client %i\n", node_id, i);
#endif

          urpc_transport_send(&clients_to_leader[i], m.content(), ACK_SIZE);

          nb_messages_in_transit_multi[i] = 0;
        }

        return recv_size;
      }
    }
  }
  */

  return recv_size;
}

// return a message received by node 0
// This is the leader. It receives messages from the clients
size_t recv_for_client(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  //todo

  /*
  while (1)
  {
    for (int i = 0; i < nb_learners; i++)
    {
      recv_size = urpc_transport_recv_nonblocking(
          &learners_to_clients[i][node_id - nb_paxos_nodes], (void*) msg,
          length / sizeof(uint64_t));

      if (recv_size > 0)
      {
        nb_messages_in_transit_multi[i]++;

        if (nb_messages_in_transit_multi[i] == NB_MSG_MAX_IN_TRANSIT)
        {
#ifdef DEBUG
          printf("Node %i is sending an ack to learner %i\n", node_id, i);
#endif

          urpc_transport_send(
              &learners_to_clients[i][node_id - nb_paxos_nodes], m.content(),
              ACK_SIZE);

          nb_messages_in_transit_multi[i] = 0;
        }

        return recv_size;
      }
    }
  }
  */

  return recv_size;
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  Message m;
  size_t recv_size;

  //todo

  /*
  if (node_id == 0)
  {
    recv_size = recv_for_node0(msg, length);
  }
  else if (node_id == 1)
  {
    recv_size = urpc_transport_recv(&leader_to_acceptor, msg, length / sizeof(uint64_t));

    nb_messages_in_transit_rcv++;

    if (nb_messages_in_transit_rcv == NB_MSG_MAX_IN_TRANSIT)
    {
#ifdef DEBUG
      printf("Node %i is sending an ack to the leader\n", node_id);
#endif

      urpc_transport_send(&leader_to_acceptor, m.content(), ACK_SIZE);

      nb_messages_in_transit_rcv = 0;
    }
  }
  else if (node_id < nb_paxos_nodes)
  {
    recv_size = urpc_transport_recv(&acceptor_to_learners[node_id - 2], msg,
        length / sizeof(uint64_t));

    nb_messages_in_transit_rcv++;

    if (nb_messages_in_transit_rcv == NB_MSG_MAX_IN_TRANSIT)
    {
#ifdef DEBUG
      printf("Node %i is sending an ack to the acceptor\n", node_id);
#endif

      urpc_transport_send(&acceptor_to_learners[node_id - 2], m.content(),
          ACK_SIZE);

      nb_messages_in_transit_rcv = 0;
    }
  }
  else
  {
    recv_size = recv_for_client(msg, length);
  }
  */

  return recv_size * sizeof(uint64_t);
}
