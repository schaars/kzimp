/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../Message.h"

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define UNIX_SOCKET_FILE_NAME "/tmp/multicore_replication_checkpointing"

/********** All the variables needed by UDP sockets **********/

static int node_id;
static int nb_nodes;

static int sock;
static struct sockaddr_un *addresses; // for each node, its address


// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;

  // create & fill addresses
  addresses = (struct sockaddr_un*) malloc(sizeof(struct sockaddr_un)
      * nb_nodes);
  if (!addresses)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    bzero((char *) &addresses[i], sizeof(addresses[i]));
    addresses[i].sun_family = AF_UNIX;
    snprintf(addresses[i].sun_path, sizeof(char) * 108, "%s_%i",
        UNIX_SOCKET_FILE_NAME, i);

#ifdef DEBUG
    printf("Node %i bound on %s\n", i, addresses[i].sun_path);
#endif
  }
}

int create_socket(struct sockaddr_un *addr)
{
  int s;

  // create socket
  s = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (s == -1)
  {
    perror("[IPC_initialize_one_node] Error while creating the socket! ");
    exit(errno);
  }

  // bind socket
  int ret = bind(s, (struct sockaddr *) addr, sizeof(*addr));
  if (ret == -1)
  {
    perror("[IPC_initialize_one_node] Error while calling bind! ");
    exit(errno);
  }

#ifdef DEBUG
  // print some information about the accepted connection
  printf("[node %i] Socket %i bound on %s\n", node_id, s, addr->sun_path);
#endif

  return s;
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  sock = create_socket(&addresses[node_id]);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  char filename[108];

  free(addresses);

  for (int i = 0; i < nb_nodes; i++)
  {
    snprintf(filename, sizeof(char) * 108, "%s_%i", UNIX_SOCKET_FILE_NAME, i);
    unlink(filename);
  }
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  close(sock);
}

void unix_send_one_node(void *msg, size_t length, int dest)
{
  sendto(sock, (char*) msg, length, 0, (struct sockaddr*) &addresses[dest],
      sizeof(addresses[dest]));
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    unix_send_one_node(msg, length, i);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  unix_send_one_node(msg, length, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
// Return 0 if the message is invalid
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;

  recv_size = recvfrom(sock, (char*) msg, length, 0, 0, 0);

  return recv_size;
}
