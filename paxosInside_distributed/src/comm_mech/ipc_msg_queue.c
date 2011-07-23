/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: IPC message queue
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by IPC message queue **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static int *ipc_queues; // all the queues

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  key_t key;

  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  ipc_queues = (int*) malloc(sizeof(int) * total_nb_nodes);
  if (!ipc_queues)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < total_nb_nodes; i++)
  {
    if ((key = ftok("/tmp/ipc_msg_queue_paxosInside", 'A' + i)) == -1)
    {
      perror("ftok error. Did you touch /tmp/ipc_msg_queue_paxosInside? ");
      exit(1);
    }

    if ((ipc_queues[i] = msgget(key, 0600 | IPC_CREAT)) == -1)
    {
      perror("msgget");
      exit(1);
    }
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
  for (int i = 0; i < total_nb_nodes; i++)
  {
    msgctl(ipc_queues[i], IPC_RMID, NULL);
  }

  free(ipc_queues);
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
  msg->mtype = 1;
  msgsnd(ipc_queues[1], msg, length, 0);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(struct ipc_message *msg, size_t length)
{
  for (int i = 0; i < nb_learners; i++)
  {
    msg->mtype = 1;
    msgsnd(ipc_queues[i + 2], msg, length, 0);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(struct ipc_message *msg, size_t length)
{
  msg->mtype = 1;
  msgsnd(ipc_queues[0], msg, length, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(struct ipc_message *msg, size_t length, int cid)
{
  msg->mtype = 1;
  msgsnd(ipc_queues[nb_paxos_nodes], msg, length, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(struct ipc_message *msg, size_t length)
{
  return msgrcv(ipc_queues[node_id], msg, length, 0, 0);
}
