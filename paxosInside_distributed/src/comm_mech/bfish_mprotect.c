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

static struct ump_channel client_to_leader; // connection between client 1 and the leader
static struct ump_channel leader_to_acceptor; // connection between leader and acceptor
static struct ump_channel *acceptor_to_learners; // connection between the acceptor and learner i
static struct ump_channel *learners_to_clients; // connection between the learner i and the client 0


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

static void init_node(int _node_id)
{
  char chaname[256];
  int i;

  node_id = _node_id;

  if (node_id == 0) // leader
  {
    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, 0);
    client_to_leader = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 1);

    // ensure that the receivers have opened the file
    sleep(2);

    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, 1);
    leader_to_acceptor = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 0);
  }
  else if (node_id == 1) // acceptor
  {
    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, 1);
    leader_to_acceptor = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 1);

    // ensure that the receivers have opened the file
    sleep(2);

    acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
        sizeof(*acceptor_to_learners) * nb_learners);
    if (!acceptor_to_learners)
    {
      perror("Allocation error of the acceptor_to_learners channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, i + 2);
      acceptor_to_learners[i] = open_channel(chaname, NB_MESSAGES,
          MESSAGE_BYTES, 0);
    }
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    learners_to_clients = (typeof(learners_to_clients)) malloc(
        sizeof(*learners_to_clients) * nb_learners);
    if (!learners_to_clients)
    {
      perror("Allocation error of the learners_to_clients channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, i + 2
          + nb_learners);
      learners_to_clients[i] = open_channel(chaname, NB_MESSAGES,
          MESSAGE_BYTES, 1);
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    // ensure that the receivers have opened the file
    sleep(2);

    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, 0);
    client_to_leader = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 0);
  }
  else // learners
  {
    acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
        sizeof(*acceptor_to_learners));
    if (!acceptor_to_learners)
    {
      perror("Allocation error of the acceptor_to_learners channels");
      exit(-1);
    }

    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, node_id);
    acceptor_to_learners[0] = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES,
        1);

    // ensure that the receivers have opened the file
    sleep(2);

    learners_to_clients = (typeof(learners_to_clients)) malloc(
        sizeof(*learners_to_clients));
    if (!learners_to_clients)
    {
      perror("Allocation error of the learners_to_clients channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, node_id
          + nb_learners);
      learners_to_clients[0] = open_channel(chaname, NB_MESSAGES,
          MESSAGE_BYTES, 0);
    }
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  init_node(_node_id);
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  init_node(_client_id);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

static void clean_node(void)
{
  int i;

  if (node_id == 0) // leader
  {
    close_channel(&leader_to_acceptor);
    close_channel(&client_to_leader);
  }
  else if (node_id == 1) // acceptor
  {
    for (i = 0; i < nb_learners; i++)
    {
      close_channel(&acceptor_to_learners[i]);
    }
    free(acceptor_to_learners);

    close_channel(&leader_to_acceptor);
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    for (i = 0; i < nb_learners; i++)
    {
      close_channel(&learners_to_clients[i]);
    }
    free(learners_to_clients);
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    close_channel(&client_to_leader);
  }
  else // learners
  {
    close_channel(&learners_to_clients[0]);
    free(learners_to_clients);

    close_channel(&acceptor_to_learners[0]);
    free(acceptor_to_learners);
  }
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

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  send_msg(&leader_to_acceptor, (char*) msg, length);
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
    send_msg(&acceptor_to_learners[l], (char*) msg, length);
  }

#ifdef DEBUG
  printf("Node %i has finished to send multicast messages\n", node_id);
#endif
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  send_msg(&client_to_leader, (char*) msg, length);
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
#ifdef DEBUG
  printf("Node %i is going to send a message to client %i\n", node_id, cid);
#endif

  send_msg(&learners_to_clients[0], (char*) msg, length);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size;
  struct ump_channel* rc;

  if (node_id == 0) // leader
  {
    recv_size = recv_msg(&client_to_leader, (char*) msg, length);
  }
  else if (node_id == 1) // acceptor
  {
    recv_size = recv_msg(&leader_to_acceptor, (char*) msg, length);
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    // the call is blocking because of the 3rd argument equal to 0
    rc = bfish_mprotect_select(learners_to_clients, nb_learners, 0);
    recv_size = recv_msg(rc, (char*) msg, length);
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    // client 1 never receives anything
    recv_size = 0;
  }
  else // learners
  {
    recv_size = recv_msg(&acceptor_to_learners[0], (char*) msg, length);
  }

  return recv_size;
}
