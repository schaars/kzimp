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

#include "Message.h"
#include "ipc_interface.h"
#include "tcp_net.h"

// debug macro
#define DEBUG
#undef DEBUG

#define MAX(a,b) (((a)>(b))?(a):(b))

/********** All the variables needed by TCP sockets **********/

#define PORT_CORE_0 4250

static int node_id;
static int nb_nodes; // a node is a PaxosInside node
static int nb_clients;

static int *node_sockets; // sockets used to communicate between the nodes
static int *client_sockets; // sockets used to communicate between the leader node and the clients

// Initialize resources for both the node and the nodes
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_nodes = _nb_nodes;
  nb_clients = _nb_clients;
}

// initialize the sockets of node 1
void initialize_node1(void)
{
  int bootstrap_socket;
  struct sockaddr_in bootstrap_sin;
  int i;

#ifdef TCP_NAGLE
  int flag;
#endif

  // create bootstrap socket
  bootstrap_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (bootstrap_socket == -1)
  {
    perror("[IPC_initialize_node] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  flag = 1;
  int result = setsockopt(bootstrap_socket, IPPROTO_TCP, TCP_NODELAY,
      (char*) &flag, sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_node] Error while setting TCP NO DELAY! ");
  }
#endif

  // bind
  bootstrap_sin.sin_addr.s_addr = htonl(INADDR_ANY);
  bootstrap_sin.sin_family = AF_INET;
  bootstrap_sin.sin_port = htons(PORT_CORE_0 + 1);

  if (bind(bootstrap_socket, (struct sockaddr*) &bootstrap_sin,
      sizeof(bootstrap_sin)) == -1)
  {
    perror("[IPC_initialize_node] Error while binding to the socket! ");
    exit(errno);
  }

  // make the socket listening for incoming connections
  if (listen(bootstrap_socket, nb_nodes + 1) == -1)
  {
    perror("[IPC_initialize_node] Error while calling listen! ");
    exit(errno);
  }

  node_sockets = (int*) malloc(sizeof(int) * (nb_nodes - 1));
  if (!node_sockets)
  {
    perror("[IPC_initialize_node] allocation error");
    exit(errno);
  }

  // accepting connections
  for (i = 0; i < nb_nodes - 1; i++)
  {
    struct sockaddr_in csin;
    int sinsize = sizeof(csin);
    node_sockets[i] = accept(bootstrap_socket, (struct sockaddr*) &csin,
        (socklen_t*) &sinsize);

    if (node_sockets[i] == -1)
    {
      perror("[IPC_initialize_node] An invalid socket has been accepted! ");
      continue;
    }

    // TCP NO DELAY
#ifdef TCP_NAGLE
    int flag = 1;
    int result = setsockopt(node_sockets[i], IPPROTO_TCP, TCP_NODELAY,
        (char *) &flag, sizeof(int));
    if (result == -1)
    {
      perror(
          "[IPC_initialize_node] Error while setting TCP NO DELAY for accepted socket! ");
    }
#endif

#ifdef DEBUG
    // print some information about the accepted connection
    printf("[node %i] A connection has been accepted from %s:%i\n", node_id,
        inet_ntoa(csin.sin_addr), ntohs(csin.sin_port));
#endif
  }

  close(bootstrap_socket);
}

// initialize node 0 (the leader)
// waits for connections from the clients
void initialize_node0(void)
{
  int bootstrap_socket;
  struct sockaddr_in bootstrap_sin;
  int i;

#ifdef TCP_NAGLE
  int flag;
#endif

  // create bootstrap socket
  bootstrap_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (bootstrap_socket == -1)
  {
    perror("[initialize_node0] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  flag = 1;
  int result = setsockopt(bootstrap_socket, IPPROTO_TCP, TCP_NODELAY,
      (char*) &flag, sizeof(int));
  if (result == -1)
  {
    perror("[initialize_node0] Error while setting TCP NO DELAY! ");
  }
#endif

  // bind
  bootstrap_sin.sin_addr.s_addr = htonl(INADDR_ANY);
  bootstrap_sin.sin_family = AF_INET;
  bootstrap_sin.sin_port = htons(PORT_CORE_0);

  if (bind(bootstrap_socket, (struct sockaddr*) &bootstrap_sin,
      sizeof(bootstrap_sin)) == -1)
  {
    perror("[initialize_node0] Error while binding to the socket! ");
    exit(errno);
  }

  // make the socket listening for incoming connections
  if (listen(bootstrap_socket, nb_clients + 1) == -1)
  {
    perror("[initialize_node0] Error while calling listen! ");
    exit(errno);
  }

  client_sockets = (int*) malloc(sizeof(int) * (nb_clients));
  if (!client_sockets)
  {
    perror("[initialize_node0] allocation error");
    exit(errno);
  }

#ifdef DEBUG
  printf("[node0] Waiting for %i clients\n", nb_clients);
#endif

  // accepting connections
  for (i = 0; i < nb_clients; i++)
  {
    struct sockaddr_in csin;
    int sinsize = sizeof(csin);
    int fd = accept(bootstrap_socket, (struct sockaddr*) &csin,
        (socklen_t*) &sinsize);

    if (fd == -1)
    {
      perror("[initialize_node0] An invalid socket has been accepted! ");
      continue;
    }

    // TCP NO DELAY
#ifdef TCP_NAGLE
    int flag = 1;
    int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
        (char *) &flag, sizeof(int));
    if (result == -1)
    {
      perror(
          "[initialize_node0] Error while setting TCP NO DELAY for accepted socket! ");
    }
#endif

#ifdef DEBUG
    // print some information about the accepted connection
    printf("[node0] A connection has been accepted from %s:%i\n", inet_ntoa(
            csin.sin_addr), ntohs(csin.sin_port));
    printf("[node0] This should be client %i\n", ntohs(csin.sin_port)
        - PORT_CORE_0);
#endif

    client_sockets[ntohs(csin.sin_port) - PORT_CORE_0 - nb_nodes] = fd;
  }

  close(bootstrap_socket);
}

// initialize a node that is not the node 1
void initialize_nodei(void)
{
  // we need only 1 socket: the one to connect to the node
  node_sockets = (int*) malloc(sizeof(int) * 1);
  if (!node_sockets)
  {
    perror("[IPC_initialize_node] allocation error");
    exit(errno);
  }

  // create socket
  node_sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
  if (node_sockets[0] == -1)
  {
    perror("[IPC_initialize_node] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  int flag = 1;
  int result = setsockopt(node_sockets[0], IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
      sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_node] Error while setting TCP NO DELAY! ");
  }
#endif

  // connect to node
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_CORE_0 + 1);

  while (1)
  {
    if (connect(node_sockets[0], (struct sockaddr *) &(addr), sizeof(addr)) < 0)
    {
      perror("[IPC_initialize_node] Cannot connect");
      sleep(1);
    }
    else
    {
#ifdef DEBUG
      printf("[node %i] Connection successful from %i to node 1!\n", node_id,
          node_id);
#endif
      break;
    }
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  if (node_id == 1)
  {
    initialize_node1();
  }
  else
  {
    if (node_id == 0)
    {
      initialize_node0();
    }

    initialize_nodei();
  }
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  // we need only 1 socket: the one to connect to the node
  node_sockets = (int*) malloc(sizeof(int) * 1);
  if (!node_sockets)
  {
    perror("[IPC_initialize_client] allocation error");
    exit(errno);
  }

  // create socket
  node_sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
  if (node_sockets[0] == -1)
  {
    perror("[IPC_initialize_client] Error while creating the socket! ");
    exit(errno);
  }

  // TCP NO DELAY
#ifdef TCP_NAGLE
  int flag = 1;
  int result = setsockopt(node_sockets[0], IPPROTO_TCP, TCP_NODELAY, (char*) &flag,
      sizeof(int));
  if (result == -1)
  {
    perror("[IPC_initialize_client] Error while setting TCP NO DELAY! ");
  }
#endif

  //bind
  struct sockaddr_in client_sin;
  client_sin.sin_addr.s_addr = htonl(INADDR_ANY);
  client_sin.sin_family = AF_INET;
  client_sin.sin_port = htons(PORT_CORE_0 + node_id);

  if (bind(node_sockets[0], (struct sockaddr*) &client_sin, sizeof(client_sin))
      == -1)
  {
    perror("[initialize_node0] Error while binding to the socket! ");
    exit(errno);
  }

  // connect to node
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_CORE_0);

  while (1)
  {
    if (connect(node_sockets[0], (struct sockaddr *) &(addr), sizeof(addr)) < 0)
    {
      perror("[IPC_initialize_client] Cannot connect");
      sleep(1);
    }
    else
    {
#ifdef DEBUG
      printf("[node %i] Connection successful from %i to node 1!\n", node_id,
          node_id);
#endif
      break;
    }
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
  int i;

  if (node_id == 1)
  {
    for (i = 0; i < nb_nodes; i++)
    {
      close(node_sockets[i]);
    }
  }
  else if (node_id == 0)
  {
    for (i = 0; i < nb_clients; i++)
    {
      close(client_sockets[i]);
    }

    free(client_sockets);
  }
  else
  {
    close(node_sockets[0]);
  }

  free(node_sockets);
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  close(node_sockets[0]);

  free(node_sockets);
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  sendMsg(node_sockets[0], msg, length);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int j = 0; j < nb_nodes - 1; j++)
  {
    sendMsg(node_sockets[j], msg, length);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  sendMsg(node_sockets[0], msg, length);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  sendMsg(client_sockets[cid - nb_nodes], msg, length);
}

// select on the array of file descriptors fds of size fds_size
// if add_fd is not -1, then this is an additionnal fd on which
// select is called
int get_fd_by_select(int *fds, int fds_size, int add_fd)
{
  fd_set file_descriptors;
  struct timeval listen_time;
  int maxsock;

  while (1)
  {
    // select
    FD_ZERO(&file_descriptors); //initialize file descriptor set

    maxsock = fds[0];
    FD_SET(fds[0], &file_descriptors);

    for (int j = 1; j < fds_size; j++)
    {
      FD_SET(fds[j], &file_descriptors);
      maxsock = MAX(maxsock, fds[j]);
    }

    // additionnal file descriptor?
    if (add_fd != -1)
    {
      FD_SET(add_fd, &file_descriptors);
      maxsock = MAX(maxsock, add_fd);
    }

    listen_time.tv_sec = 1;
    listen_time.tv_usec = 0;

    select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

    for (int j = 0; j < fds_size; j++)
    {
      //I want to listen at this replica
      if (FD_ISSET(fds[j], &file_descriptors))
      {
        return fds[j];
      }
    }

    // additionnal file descriptor
    if (add_fd != -1 && FD_ISSET(add_fd, &file_descriptors))
    {
      return add_fd;
    }
  }
}

// get a file descriptor on which there is something to receive
int get_fd_for_recv(void)
{
  if (node_id == 0)
  {
    return get_fd_by_select(client_sockets, nb_clients, node_sockets[0]);
  }
  else if (node_id == 1)
  {
    return get_fd_by_select(node_sockets, nb_nodes - 1, -1);
  }
  else
  {
    // either this node is neither 0 nor 1, or this is a client
    return node_sockets[0];
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, left, msg_len;
  int fd;

  fd = get_fd_for_recv();

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

  return msg_len;
}
