/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
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

#define MAX(a, b) (((a)>(b))?(a):(b))

/********** All the variables needed by UDP sockets **********/

#define PORT_CORE_0 4242 // ports on which the node 0 binds.
static int node_id;
static int nb_nodes;

static int *sock; // the socket, 1 per node. Note that node 0 uses sock[0] as
// the bootstrap socket, while the other nodes use only sock[0]

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;
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
  struct sockaddr_in bootstrap_sin;

#ifdef TCP_NAGLE
  int flag;
#endif

  // create bootstrap socket
  s = socket(AF_INET, SOCK_STREAM, 0);
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
    bootstrap_sin.sin_addr.s_addr = htonl(INADDR_ANY);
    bootstrap_sin.sin_family = AF_INET;
    bootstrap_sin.sin_port = htons(port);

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
  struct sockaddr_in addr;

  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

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
  struct sockaddr_in csin;
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
    printf("[node %i] A connection has been accepted from %s:%i\n", node_id,
        inet_ntoa(csin.sin_addr), ntohs(csin.sin_port));
#endif

    break;
  }

  return ns;
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  if (node_id == 0)
  {
    sock = (typeof(sock)) malloc(sizeof(*sock) * nb_nodes);
    if (!sock)
    {
      perror("sock allocation error");
      exit(-1);
    }

    sock[0] = create_socket(PORT_CORE_0, nb_nodes);

    for (int i = 1; i < nb_nodes; i++)
    {
      sock[i] = get_connection(sock[0]);
    }
  }
  else
  {
    sock = (typeof(sock)) malloc(sizeof(*sock));
    if (!sock)
    {
      perror("sock allocation error");
      exit(-1);
    }

    sock[0] = create_socket(0, 0);
    connect_socket(sock[0], PORT_CORE_0);
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
      close(sock[i]);
    }
  }
  close(sock[0]);
  free(sock);
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    sendMsg(sock[i], msg, length);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  sendMsg(sock[0], msg, length);
}

int get_fd_for_receive(void)
{
  if (node_id == 0)
  {
    fd_set file_descriptors;
    struct timeval listen_time;
    int maxsock;

    while (1)
    {
      // select
      FD_ZERO(&file_descriptors); //initialize file descriptor set

      maxsock = sock[1];
      FD_SET(sock[1], &file_descriptors);

      for (int j = 2; j < nb_nodes; j++)
      {
        FD_SET(sock[j], &file_descriptors);
        maxsock = MAX(maxsock, sock[j]);
      }

      listen_time.tv_sec = 1;
      listen_time.tv_usec = 0;

      select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

      for (int j = 1; j < nb_nodes; j++)
      {
        //I want to listen at this replica
        if (FD_ISSET(sock[j], &file_descriptors))
        {
          return sock[j];
        }
      }
    }
  }
  else
  {
    return sock[0];
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, left, msg_len;
  int fd;

  s = 0;

  fd = get_fd_for_receive();
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
