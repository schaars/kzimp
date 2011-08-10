/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Inet sockets using TCP
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "../Message.h"
#include "ipc_interface.h"
#include "tcp_net.h"

// debug macro
#define DEBUG
#undef DEBUG

// You can define TCP_NAGLE if you want to activate TCP_NODELAY

#define MAX(a,b) (((a)>(b))?(a):(b))

/********** All the variables needed by TCP sockets **********/

#define PORT_CORE_0 4242

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

static int client_to_leader; // client 1 -> leader
static int leader_to_acceptor; // leader -> acceptor
static int* acceptor_to_learners; // acceptor -> learner i, for all i
static int* learners_to_client; // learner i -> client 0, for all i

// Initialize resources for both the node and the nodes
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_paxos_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

/*
 * Create a socket.
 * TCP NO DELAY if needed.
 * If port != 0 then it is considered as a server socket:
 *  -bind on port
 *  -listen for incoming connection, using backlog as the backlog.
 * Return the new socket.
 */
int create_socket(int port, int backlog)
{
  int s;
  SOCKADDR bootstrap_sin;

#ifdef TCP_NAGLE
  int flag;
#endif

  // create bootstrap socket
#ifdef IPV6
  s = socket(AF_INET6, SOCK_STREAM, 0);
#else
  s = socket(AF_INET, SOCK_STREAM, 0);
#endif
  if (s == -1)
  {
    perror("[create_server_socket] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  flag = 1;
  int result = setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
      (char*) &flag, sizeof(int));
  if (result == -1)
  {
    perror("[create_server_socket] Error while setting TCP NO DELAY! ");
  }
#endif

  if (port != 0)
  {
    // bind
#ifdef IPV6
    bootstrap_sin.sin6_addr = in6addr_any;
    bootstrap_sin.sin6_family = AF_INET6;
    bootstrap_sin.sin6_port = htons(port);
#else
    bootstrap_sin.sin_addr.s_addr = htonl(INADDR_ANY);
    bootstrap_sin.sin_family = AF_INET;
    bootstrap_sin.sin_port = htons(port);
#endif

    if (bind(s, (struct sockaddr*) &bootstrap_sin, sizeof(bootstrap_sin)) == -1)
    {
      perror("[create_server_socket] Error while binding to the socket! ");
      exit(errno);
    }

    // make the socket listening for incoming connections
    if (listen(s, backlog) == -1)
    {
      perror("[create_server_socket] Error while calling listen! ");
      exit(errno);
    }
  }

  return s;
}

// connect socket s to port port on localhost
void connect_socket(int s, int port)
{
  SOCKADDR addr;

#ifdef IPV6
  addr.sin6_addr = in6addr_loopback;
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);
#else
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
#endif

  while (1)
  {
    if (connect(s, (struct sockaddr *) &(addr), sizeof(addr)) < 0)
    {
#ifdef DEBUG
      perror("[connect_socket] Cannot connect");
#endif
      sleep(1);
    }
    else
    {
#ifdef DEBUG
      printf("[node %i] Connection successful to port %i (node %i I guess)!\n",
          node_id, port, port - PORT_CORE_0);
#endif
      break;
    }
  }
}

// Get one connection, froms socket s, and return it.
int get_connection(int s)
{
  int ns;
  SOCKADDR csin;
  int sinsize = sizeof(csin);

  while (1)
  {
    ns = accept(s, (struct sockaddr*) &csin, (socklen_t*) &sinsize);

    if (ns == -1)
    {
      perror("[IPC_initialize_node] An invalid socket has been accepted! ");
      continue;
    }

    // TCP NO DELAY
#ifdef TCP_NAGLE
    int flag = 1;
    int result = setsockopt(ns, IPPROTO_TCP, TCP_NODELAY,
        (char *) &flag, sizeof(int));
    if (result == -1)
    {
      perror(
          "[IPC_initialize_node] Error while setting TCP NO DELAY for accepted socket! ");
    }
#endif

#ifdef DEBUG
    // print some information about the accepted connection
#ifdef IPV6
    printf("[node %i] A connection has been accepted from port %i\n", node_id,
        ntohs(csin.sin6_port));
#else
    printf("[node %i] A connection has been accepted from %s:%i\n", node_id,
        inet_ntoa(csin.sin_addr), ntohs(csin.sin_port));
#endif
#endif

    break;
  }

  return ns;
}

void initialize_leader(void)
{
  /***** listen from client 1 *****/
  // create a socket
  int bootstrap = create_socket(PORT_CORE_0, 1);

  // get connection
  client_to_leader = get_connection(bootstrap);

#ifdef DEBUG
  printf("Leader has received a connection from a client\n");
#endif

  close(bootstrap);

  /***** connect to acceptor *****/
  // create a socket
  leader_to_acceptor = create_socket(0, 0);

  // connect to the leader
  connect_socket(leader_to_acceptor, PORT_CORE_0 + 1);

#ifdef DEBUG
  printf("Leader is connected to the acceptor\n");
#endif
}

void initialize_acceptor(void)
{
  int i;

  /***** listen from leader *****/
  // create a socket
  int bootstrap = create_socket(PORT_CORE_0 + node_id, 1);

  // get connection
  leader_to_acceptor = get_connection(bootstrap);

#ifdef DEBUG
  printf("Acceptor has received a connection from the leader\n");
#endif

  close(bootstrap);

  /***** connect to learners *****/
  acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
      sizeof(*acceptor_to_learners) * nb_learners);
  if (!acceptor_to_learners)
  {
    perror("acceptor_to_learners allocation error");
    exit(-1);
  }

  for (i = 0; i < nb_learners; i++)
  {
    acceptor_to_learners[i] = create_socket(0, 0);
    connect_socket(acceptor_to_learners[i], PORT_CORE_0 + 2 + i);

#ifdef DEBUG
    printf("Acceptor connected to learner %i on port %i. acceptor_to_learners[%i]=%i @ %p\n", i,
        PORT_CORE_0 + 2 + i, i, acceptor_to_learners[i], &acceptor_to_learners[i]);
#endif
  }
}

void initialize_client0(void)
{
  int i;

  /***** listen from learners *****/
  // create a socket
  int bootstrap = create_socket(PORT_CORE_0 + nb_paxos_nodes, nb_paxos_nodes);

  learners_to_client = (typeof(learners_to_client)) malloc(
      sizeof(*learners_to_client) * nb_learners);
  if (!learners_to_client)
  {
    perror("acceptor_to_learners allocation error");
    exit(-1);
  }

  for (i = 0; i < nb_learners; i++)
  {
    learners_to_client[i] = get_connection(bootstrap);
#ifdef DEBUG
    printf("Client 0 has received a connection from a learner\n");
#endif
  }

  close(bootstrap);
}

void initialize_client1(void)
{
  // create a socket
  client_to_leader = create_socket(0, 0);

  // connect to the leader
  connect_socket(client_to_leader, PORT_CORE_0);

#ifdef DEBUG
  printf("Client is connected to the leader\n");
#endif
}

void initialize_learner(void)
{
  /***** listen from acceptor *****/
  // create a socket
  int bootstrap = create_socket(PORT_CORE_0 + node_id, 1);

  acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
      sizeof(*acceptor_to_learners));
  if (!acceptor_to_learners)
  {
    perror("acceptor_to_learners allocation error");
    exit(-1);
  }

  // get connection
  acceptor_to_learners[0] = get_connection(bootstrap);

#ifdef DEBUG
  printf("Learner %i has received a connection from the acceptor on port %i\n",
      node_id, PORT_CORE_0 + node_id);
#endif

  close(bootstrap);

  /***** connect to client 0 *****/
  learners_to_client = (typeof(learners_to_client)) malloc(
      sizeof(*learners_to_client));
  if (!learners_to_client)
  {
    perror("learners_to_client allocation error");
    exit(-1);
  }

  learners_to_client[0] = create_socket(0, 0);
  connect_socket(learners_to_client[0], PORT_CORE_0 + nb_paxos_nodes);

#ifdef DEBUG
  printf("Learner %i connected to client 0\n", node_id);
#endif
}

void initialize_node(void)
{
  if (node_id == 0) // leader
  {
    initialize_leader();
  }
  else if (node_id == 1) // acceptor
  {
    initialize_acceptor();
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    initialize_client0();
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    initialize_client1();
  }
  else // learners
  {
    initialize_learner();
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  initialize_node();
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  initialize_node();
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
    close(leader_to_acceptor);
    close(client_to_leader);
  }
  else if (node_id == 1) // acceptor
  {
    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i]);
    }
    free(acceptor_to_learners);
    close(leader_to_acceptor);
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    for (i = 0; i < nb_learners; i++)
    {
      close(learners_to_client[i]);
    }
    free(learners_to_client);
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    close(client_to_leader);
  }
  else // learners
  {
    close(learners_to_client[0]);
    free(learners_to_client);

    close(acceptor_to_learners[0]);
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
  sendMsg(leader_to_acceptor, msg, length);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int j = 0; j < nb_learners; j++)
  {
    sendMsg(acceptor_to_learners[j], msg, length);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  sendMsg(client_to_leader, msg, length);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  sendMsg(learners_to_client[0], msg, length);
}

// get a file descriptor on which there is something to receive
int get_fd_for_recv(void)
{
  if (node_id == 0) // leader
  {
    return client_to_leader;
  }
  else if (node_id == 1) // acceptor
  {
    return leader_to_acceptor;
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    fd_set file_descriptors;
    struct timeval listen_time;
    int maxsock;

    while (1)
    {
      // select
      FD_ZERO(&file_descriptors); //initialize file descriptor set

      maxsock = learners_to_client[0];
      FD_SET(learners_to_client[0], &file_descriptors);

      for (int j = 1; j < nb_learners; j++)
      {
        FD_SET(learners_to_client[j], &file_descriptors);
        maxsock = MAX(maxsock, learners_to_client[j]);
      }

      listen_time.tv_sec = 1;
      listen_time.tv_usec = 0;

      select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

      for (int j = 0; j < nb_learners; j++)
      {
        //I want to listen at this replica
        if (FD_ISSET(learners_to_client[j], &file_descriptors))
        {
          return learners_to_client[j];
        }
      }
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    // client 1 never receives anything
  }
  else // learners
  {
    return acceptor_to_learners[0];
  }

  return 0;
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, left, msg_len;
  int fd;

  s = 0;

  fd = get_fd_for_recv();
  if (fd < 0)
  {
    printf("Node %i has fd=%i in recv\n", node_id, fd);
    return 0;
  }

#ifdef DEBUG
  printf("Node %i is receiving a message with fd=%i\n", node_id, fd);
#endif

  // receive the message_header
  header_size = recvMsg(fd, (void*) msg, sizeof(struct message_header));

  // receive the message content
  msg_len = ((struct message_header*) msg)->len;
  left = msg_len - header_size;
  if (left > 0)
  {
    s = recvMsg(fd, (void*) ((char*) msg + header_size), left);
  }

  return header_size + s;
}
