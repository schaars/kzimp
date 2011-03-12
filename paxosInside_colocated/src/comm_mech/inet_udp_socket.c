/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Inet sockets using UDP
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define UDP_SEND_MAX_SIZE 65507

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by UDP sockets **********/

#define PORT_CORE_0 4242

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int total_nb_nodes;

static int sock; // the socket

struct sockaddr_in *addresses; // for each node (clients + PaxosInside nodes), its address

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  // create & fill addresses
  addresses = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in)
      * total_nb_nodes);
  if (!addresses)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < total_nb_nodes; i++)
  {
    addresses[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    addresses[i].sin_family = AF_INET;
    addresses[i].sin_port = htons(PORT_CORE_0 + i);
  }
}

void initialize_one_node(void)
{
  // create socket
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("[IPC_initialize_one_node] Error while creating the socket! ");
    exit(errno);
  }

  // bind socket
  int ret = bind(sock, (struct sockaddr *) &addresses[node_id],
      sizeof(addresses[node_id]));
  if (ret == -1)
  {
    perror("[IPC_initialize_one_node] Error while calling bind! ");
    exit(errno);
  }
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
  free(addresses);
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

void udp_send_one_node(void *msg, size_t length, int dest)
{
  size_t sent, to_send;

  sent = 0;
  while (sent < length)
  {
    to_send = MIN(length - sent, UDP_SEND_MAX_SIZE);

    sent += sendto(sock, (char*)msg + sent, to_send, 0,
        (struct sockaddr*) &addresses[dest], sizeof(addresses[dest]));
  }
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  udp_send_one_node(msg, length, 1);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    // send to nodes from 0 to nb_paxos_nodes-1 but 1
    // indeed 1 is the acceptor which multicasts this message
    if (i == 1)
    {
      continue;
    }

    udp_send_one_node(msg, length, i);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  udp_send_one_node(msg, length, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  udp_send_one_node(msg, length, cid);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;
  while (recv_size < length)
  {
    recv_size += recvfrom(sock, (char*)msg + recv_size, length - recv_size, 0, 0, 0);
  }

  return recv_size;
}
