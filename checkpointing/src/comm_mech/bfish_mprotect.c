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

// Define NB_MESSAGES as the max number of messages in the channel
// Define MESSAGE_BYTES as the max size of the messages in bytes
// Define WAIT_TYPE as USLEEP or BUSY

#define KBFISH_MEM_CHAR_DEV_FILE "/dev/kbfishmem"

static int node_id;
static int nb_nodes;

static struct ump_channel *connections; // urpc_connections between nodes 0 and nodes i


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    connections = (typeof(connections)) malloc(sizeof(*connections) * nb_nodes);
    if (!connections)
    {
      perror("Allocation error of the connections at node 0");
      exit(-1);
    }

    // ensure that the receivers have opened the file
    sleep(2);

    // communication from 0 to 0 does not exist
    for (int i = 1; i < nb_nodes; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, i - 1);
      connections[i] = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 0);
    }
  }
  else
  {
    connections = (typeof(connections)) malloc(sizeof(*connections));
    if (!connections)
    {
      perror("Allocation error of the connection");
      exit(-1);
    }

    snprintf(chaname, 256, "%s%i", KBFISH_MEM_CHAR_DEV_FILE, node_id - 1);
    connections[0] = open_channel(chaname, NB_MESSAGES, MESSAGE_BYTES, 1);
  }
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
    for (int i = 1; i < nb_nodes; i++)
    {
      close_channel(&connections[i]);
    }
  }
  else
  {
    close_channel(&connections[0]);
  }

  free(connections);
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    send_msg(&connections[i], (char*) msg, length);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  send_msg(&connections[0], (char*) msg, length);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;
  struct ump_channel* rc;

  if (node_id == 0) // leader
  {
    // the call is blocking because of the 3rd argument equal to 0
    // connections+1 because there is no channel on connections[0]
    rc = bfish_mprotect_select(connections + 1, nb_nodes - 1, 0);
    recv_size = recv_msg(rc, (char*) msg, length);
  }
  else
  {
    recv_size = recv_msg(&connections[0], (char*) msg, length);
  }

  return recv_size;
}
