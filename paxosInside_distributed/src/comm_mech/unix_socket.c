/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Unix domain sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "ipc_interface.h"

// debug macro
#define DEBUG
//#undef DEBUG

#define UNIX_SOCKET_FILE_NAME "/tmp/multicore_replication_paxosInside"

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by Unix domain sockets **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static int sock; // the socket

static struct sockaddr_un *addresses; // for each node (clients + PaxosInside nodes), its address

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_paxos_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  // create & fill addresses
  addresses = (struct sockaddr_un*) malloc(sizeof(struct sockaddr_un)
      * total_nb_nodes);
  if (!addresses)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < total_nb_nodes; i++)
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

void initialize_one_node(void)
{
  // create socket
  sock = create_socket(&addresses[node_id]);
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  initialize_one_node();
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  initialize_one_node();
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  char filename[108];

  free(addresses);

  for (int i = 0; i < total_nb_nodes; i++)
  {
    snprintf(filename, sizeof(char) * 108, "%s_%i", UNIX_SOCKET_FILE_NAME, i);
    unlink(filename);
  }
}

void clean_one_node(void)
{
  close(sock);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  clean_one_node();
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  clean_one_node();
}

void unix_send_one_node(void *msg, size_t length, int dest)
{
  size_t sent;

#ifdef DEBUG
  int nb_iter = 0;
#endif

  // normally we should get only once in the loop
  sent = 0;
  while (sent < length)
  {
    sent += sendto(sock, (char*) msg + sent, length - sent, 0,
        (struct sockaddr*) &addresses[dest], sizeof(addresses[dest]));

#ifdef DEBUG
    nb_iter++;
    if (nb_iter > 1)
    {
      printf("[%s:%i] Multiple times in recvfrom? What the hell?\n", __func__,
          __LINE__);
    }
#endif
  }
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  unix_send_one_node(msg, length, 1);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int i = 0; i < nb_learners; i++)
  {
    unix_send_one_node(msg, length, i + 2);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  unix_send_one_node(msg, length, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  unix_send_one_node(msg, length, nb_paxos_nodes);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;

#ifdef DEBUG
  int nb_iter = 0;
#endif

  // normally we should get only once in the loop
  while (recv_size < length)
  {
    recv_size += recvfrom(sock, (char*) msg + recv_size, length - recv_size, 0,
        0, 0);

#ifdef DEBUG
    nb_iter++;
    if (nb_iter > 1)
    {
      printf("[%s:%i] Multiple times in recvfrom? What the hell?\n", __func__,
          __LINE__);
    }
#endif
  }

  return recv_size;
}
