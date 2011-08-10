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

#include "../Message.h"
#include "ipc_interface.h"

#ifdef OPEN_LOOP
#include "../Response.h"
#endif

// debug macro
#define DEBUG
#undef DEBUG

#define UDP_SEND_MAX_SIZE 65507

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by UDP sockets **********/

#define PORT_CORE_0 4242
#define PORT_CLIENT_0 6000

#ifdef IPV6
typedef struct sockaddr_in6 SOCKADDR;
#else
typedef struct sockaddr_in SOCKADDR;
#endif

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static int sock; // the socket
static int *client0_sock; // sockets of client 0, one per learner

static SOCKADDR *addresses; // for each node (clients + PaxosInside nodes), its address
static SOCKADDR *client0_addr; // addresses of the client 0, one per learner

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  // create & fill addresses
  addresses = (SOCKADDR*) malloc(sizeof(SOCKADDR)
      * total_nb_nodes);
  if (!addresses)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < total_nb_nodes; i++)
  {
#ifdef IPV6
    addresses[i].sin6_addr = in6addr_loopback;
    addresses[i].sin6_family = AF_INET6;
    addresses[i].sin6_port = htons(PORT_CORE_0 + i);
#else
    addresses[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    addresses[i].sin_family = AF_INET;
    addresses[i].sin_port = htons(PORT_CORE_0 + i);
#endif

#ifdef DEBUG
    printf("Node %i bound on port %i\n", i, PORT_CORE_0 + i);
#endif
  }

  // create & fill client 0 addresses
  client0_addr = (SOCKADDR*) malloc(sizeof(SOCKADDR)
      * nb_learners);
  if (!client0_addr)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < nb_learners; i++)
  {
#ifdef IPV6
    client0_addr[i].sin6_addr = in6addr_loopback;
    client0_addr[i].sin6_family = AF_INET6;
    client0_addr[i].sin6_port = htons(PORT_CLIENT_0 + i);
#else
    client0_addr[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    client0_addr[i].sin_family = AF_INET;
    client0_addr[i].sin_port = htons(PORT_CLIENT_0 + i);
#endif

#ifdef DEBUG
    printf("Learner %i sends on port %i\n", i, PORT_CLIENT_0 + i);
#endif
  }
}

int create_socket(SOCKADDR *addr)
{
  int s;

  // create socket
#ifdef IPV6
  s = socket(AF_INET6, SOCK_DGRAM, 0);
#else
  s = socket(AF_INET, SOCK_DGRAM, 0);
#endif
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
  printf("[node %i] Socket %i bound on %s:%i\n", node_id, s, inet_ntoa(
          addr->sin_addr), ntohs(addr->sin_port));
#endif

  return s;
}

void initialize_one_node(void)
{
  int i;

  if (node_id == nb_paxos_nodes)
  {
    // client 0 has a special procedure
    client0_sock = (typeof(client0_sock)) malloc(sizeof(*client0_sock)
        * nb_learners);
    if (!client0_sock)
    {
      perror("IPC_initialize malloc error ");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      client0_sock[i] = create_socket(&client0_addr[i]);
    }
  }
  else
  {
    // create socket
    sock = create_socket(&addresses[node_id]);
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
  free(client0_addr);
  free(addresses);
}

void clean_one_node(void)
{
  int i;

  if (node_id == nb_paxos_nodes)
  {
    // client 0 has a special procedure
    for (i = 0; i < nb_learners; i++)
    {
      close(client0_sock[i]);
    }
    free(client0_sock);
  }
  else
  {
    close(sock);
  }
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
  SOCKADDR *addr;

  if (dest == nb_paxos_nodes)
  {
    addr = &client0_addr[node_id - 2];
  }
  else
  {
    addr = &addresses[dest];
  }

#ifdef DEBUG
  if (node_id >= 2 && node_id < nb_paxos_nodes)
  printf("[node %i] Sending on %s:%i\n", node_id, inet_ntoa(addr->sin_addr),
      ntohs(addr->sin_port));
#endif

  sent = 0;
  while (sent < length)
  {
    to_send = MIN(length - sent, UDP_SEND_MAX_SIZE);

    sent += sendto(sock, (char*) msg + sent, to_send, 0,
        (struct sockaddr*) addr, sizeof(*addr));
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
  for (int i = 0; i < nb_learners; i++)
  {
    udp_send_one_node(msg, length, i + 2);
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
  udp_send_one_node(msg, length, nb_paxos_nodes);
}

int get_fd_for_receive(void)
{
  if (node_id == nb_paxos_nodes)
  {
    fd_set file_descriptors;
    struct timeval listen_time;
    int maxsock;

    while (1)
    {
      // select
      FD_ZERO(&file_descriptors); //initialize file descriptor set

      maxsock = client0_sock[0];
      FD_SET(client0_sock[0], &file_descriptors);
//      printf("Node %i is going to select on %i\n", node_id, client0_sock[0]);

      for (int j = 1; j < nb_learners; j++)
      {
        FD_SET(client0_sock[j], &file_descriptors);
        maxsock = MAX(maxsock, client0_sock[j]);
//        printf("Node %i is going to select on %i\n", node_id, client0_sock[j]);
      }

      listen_time.tv_sec = 1;
      listen_time.tv_usec = 0;

      select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

      for (int j = 0; j < nb_learners; j++)
      {
        //I want to listen at this replica
        if (FD_ISSET(client0_sock[j], &file_descriptors))
        {
          return client0_sock[j];
        }
      }
    }
  }
  else
  {
    return sock;
  }
}

size_t receive_chunk(int s, char *msg, size_t length)
{
  size_t recv_size = 0;
  while (recv_size < length)
  {
    recv_size += recvfrom(s, msg + recv_size, length - recv_size, 0, 0, 0);
  }

  return recv_size;
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, msg_len;
  int sk;

  sk = get_fd_for_receive();

#ifdef DEBUG
  if (node_id >= 2 && node_id < nb_paxos_nodes)
  printf("Node %i is going to receive on %i\n", node_id, sk);
#endif

  // get the header
  header_size = recvfrom(sk, (char*)msg, UDP_SEND_MAX_SIZE, 0, 0, 0);

  // get the content
  msg_len = ((struct message_header*) msg)->len;

#ifdef DEBUG
  printf("[node %i] Header size is %lu, msg_len is %lu\n", node_id,
      header_size, msg_len);
#endif

  s = header_size;
  while (s < msg_len)
  {
#ifdef DEBUG
     printf("[node %i] s=%lu, msg_len=%lu, length=%lu\n", s, msg_len, length);
#endif

     s += recvfrom(sk, (char*)msg + s, msg_len - s, 0, 0, 0);
  }

#ifdef OPEN_LOOP
  // I am client 0. If the message comes from learner 0, then set the value to 1
  // otherwise set it to 0
  if (node_id == nb_paxos_nodes) {
     struct message_response* req = (struct message_response*) msg;
     req->value = ((sk == client0_sock[0]) ? 1 : 0);
  }
#endif

  return s;
}
