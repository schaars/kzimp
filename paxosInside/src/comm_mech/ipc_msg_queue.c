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

#include "../Message.h"
#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// You can define ONE_QUEUE if you want to have only 1 queue shared by all the nodes (clients + paxos nodes).

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by IPC message queue **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int total_nb_nodes;

static int *ipc_queues; // all the queues

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  key_t key;

  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

#ifdef ONE_QUEUE
  ipc_queues = (int*) malloc(sizeof(int) * 1);
#else
  ipc_queues = (int*) malloc(sizeof(int) * total_nb_nodes);
#endif
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

#ifdef ONE_QUEUE
    break;
#endif
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
