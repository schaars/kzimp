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

#include "ipc_interface.h"
#include "tcp_net.h"

// debug macro
#define DEBUG
#undef DEBUG

#define MAX(a,b) (((a)>(b))?(a):(b))

/********** All the variables needed by TCP sockets **********/

#define PORT_CORE_0 4242

static int core_id;
static int nb_nodes; // a node is a PaxosInside node
static int nb_clients;

static int *node_sockets; // sockets used to communicate between the nodes

// Initialize resources for both the node and the nodes
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_nodes = _nb_nodes;
  nb_clients = _nb_clients;
}

// initialize the sockets of node 1
void initialize_node1()
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
  bootstrap_sin.sin_port = htons(PORT_CORE_0);

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
    printf("[node] A connection has been accepted from %s:%i\n", inet_ntoa(
            csin.sin_addr), ntohs(csin.sin_port));
#endif
  }

  close(bootstrap_socket);
}

// initialize a node that is not the node 1
void initialize_nodei()
{
  //todo: if core_id is 0 then initialize everything for comm with the clients

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
  addr.sin_port = htons(PORT_CORE_0);

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
      printf("[node %i] Connection successful from %i to node 1!\n", core_id,
          core_id);
#endif
      break;
    }
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _core_id)
{
  core_id = _core_id;

  if (core_id == 1)
  {
    initialize_node1();
  }
  else
  {
    initialize_nodei();
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

// Clean resources created for the node.
void IPC_clean_node_node(void)
{
}

// clean resources allocated for node 1
void clean_node1()
{
  int i;

  for (i = 0; i < nb_nodes; i++)
  {
    close(node_sockets[i]);
  }

  free(node_sockets);
}

// clean resources allocated for a node different than 1
void clean_nodei()
{
  close(node_sockets[0]);

  free(node_sockets);
}

// Clean resources created for the node.
void IPC_clean_node(void)
{

  if (core_id == 1)
  {
    clean_node1();
  }
  else
  {
    clean_nodei();
  }
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

// get a file descriptor on which there is something to receive
int get_fd_for_recv()
{
  fd_set file_descriptors;
  struct timeval listen_time;
  int maxsock;

  if (core_id != 1)
  {
    return node_sockets[0];
  }

  while (1)
  {
    // select
    FD_ZERO(&file_descriptors); //initialize file descriptor set

    maxsock = node_sockets[0];
    FD_SET(node_sockets[0], &file_descriptors);

    for (int j = 1; j < nb_nodes - 1; j++)
    {
      FD_SET(node_sockets[j], &file_descriptors);
      maxsock = MAX(maxsock, node_sockets[j]);
    }

    listen_time.tv_sec = 1;
    listen_time.tv_usec = 0;

    select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

    for (int j = 0; j < nb_nodes - 1; j++)
    {
      //I want to listen at this replica
      if (FD_ISSET(node_sockets[j], &file_descriptors))
      {
        return node_sockets[j];
      }
    }
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(struct message_header *msg, size_t length)
{
  size_t header_size, s, left;
  int fd;

  fd = get_fd_for_recv();

#ifdef DEBUG
  printf("Node %i is receiving a message with fd=%i\n", core_id, fd);
#endif

  // receive the message_header
  header_size = recvMsg(fd, (void*) msg, sizeof(struct message_header));

  // receive the message content
  left = msg->len - header_size;
  if (left > 0)
  {
    s = recvMsg(fd, (void*) (msg + header_size), left);
  }

  return msg->len;
}
