/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: IPC Message Queue
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

/********** All the variables needed by UDP sockets **********/

static int node_id;
static int nb_nodes;

static int *ipc_queues; // all the queues


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  key_t key;

  nb_nodes = _nb_nodes;

  ipc_queues = (int*) malloc(sizeof(int) * nb_nodes);
  if (!ipc_queues)
  {
    perror("Allocation error");
    exit(errno);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    if ((key = ftok("/tmp/ipc_msg_queue_checkpointing", 'A' + i)) == -1)
    {
      perror("ftok error. Did you touch /tmp/ipc_msg_queue_checkpointing? ");
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

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  for (int i = 0; i < nb_nodes; i++)
  {
    msgctl(ipc_queues[i], IPC_RMID, NULL);
  }

  free(ipc_queues);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(struct ipc_message *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    msg->mtype = 1;
    msgsnd(ipc_queues[i], msg, length, 0);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(struct ipc_message *msg, size_t length, int nid)
{
  msg->mtype = 1;
  msgsnd(ipc_queues[0], msg, length, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
// Return 0 if the message is invalid
size_t IPC_receive(struct ipc_message *msg, size_t length)
{
  return msgrcv(ipc_queues[node_id], msg, length, 0, 0);
}
