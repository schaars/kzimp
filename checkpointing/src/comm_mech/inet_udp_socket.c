/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Message.h"
#include "../MessageTag.h"

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define DEBUG_MORE
#undef DEBUG_MORE

#define UDP_SEND_MAX_SIZE 65507

// must be after the content but before the end of the buffer
#define SEQ_ID_OFFSET 200

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by UDP sockets **********/

#define PORT_CORE_0 4242 // ports on which the nodes bind.  They recv on PORT_CORE_0+node_id
#define PORT_RECV_0 6000 // ports on which the nodes send to node 0. They send to PORT_RECV_0+node_id
static int node_id;
static int nb_nodes;

static int *sock;
static struct sockaddr_in *addresses; // for each node, its address
static struct sockaddr_in *addresses_node0; // addresses used by the nodes>0 to send to node 0

#if 0
// last sent message
static char last_sent_msg[MESSAGE_MAX_SIZE];
static size_t last_sent_length;
#endif

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;

  // create & fill addresses
  addresses = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in)
      * nb_nodes);
  if (!addresses)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    addresses[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    addresses[i].sin_family = AF_INET;
    addresses[i].sin_port = htons(PORT_CORE_0 + i);

#ifdef DEBUG
    printf("Node %i bound on port %i\n", i, PORT_CORE_0 + i);
#endif
  }

  // create & fill addresses
  addresses_node0 = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in)
      * nb_nodes);
  if (!addresses_node0)
  {
    perror("IPC_initialize malloc error ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    addresses_node0[i].sin_addr.s_addr = inet_addr("127.0.0.1");
    addresses_node0[i].sin_family = AF_INET;
    addresses_node0[i].sin_port = htons(PORT_RECV_0 + i);

#ifdef DEBUG
    printf("Node %i bound on port %i\n", i, PORT_RECV_0 + i);
#endif
  }
}

int create_socket(struct sockaddr_in *addr)
{
  int s;

  // create socket
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
  {
    perror("[IPC_initialize_one_node] Error while creating the socket! ");
    exit(errno);
  }

  // get buffers size
  /*
   int optval;
   socklen_t optval_size;
   optval_size = sizeof(optval);
   getsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, &optval_size);
   printf("RCVBUF=%i\n", optval);
   */

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

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  if (node_id == 0)
  {
    sock = (typeof(sock)) malloc(sizeof(*sock) * nb_nodes);
    if (!sock)
    {
      perror("Allocation error of the sockets at node 0");
      exit(-1);
    }

    sock[0] = create_socket(&addresses[0]);

    // communication from 0 to 0 does not exist
    for (int i = 1; i < nb_nodes; i++)
    {
      sock[i] = create_socket(&addresses_node0[i]);
    }
  }
  else
  {
    sock = (typeof(sock)) malloc(sizeof(*sock));
    if (!sock)
    {
      perror("Allocation error of the sockets at node 0");
      exit(-1);
    }

    sock[0] = create_socket(&addresses[node_id]);
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  free(addresses);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  if (node_id == 0)
  {
    for (int i = 0; i < nb_nodes; i++)
    {
      close(sock[i]);
    }
  }
  else
  {
    close(sock[0]);
  }

  free(sock);
}

void udp_send_one_node(void *msg, size_t length, struct sockaddr_in *addr)
{
  size_t sent, to_send;
  char seq_id = 0;

#ifdef DEBUG
  printf("[node %i] Sending on %s:%i\n", node_id, inet_ntoa(addr->sin_addr),
      ntohs(addr->sin_port));
#endif

  sent = 0;
  while (sent < length)
  {
    to_send = MIN(length - sent, UDP_SEND_MAX_SIZE);

    if (MESSAGE_MAX_SIZE > UDP_SEND_MAX_SIZE)
    {
      ((char*) msg + sent + SEQ_ID_OFFSET)[0] = seq_id;
      seq_id++;
    }

    sent += sendto(sock[0], (char*) msg + sent, to_send, 0,
        (struct sockaddr*) addr, sizeof(*addr));
  }
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    udp_send_one_node(msg, length, &addresses[i]);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  udp_send_one_node(msg, length, &addresses_node0[node_id]);
}

/*************************** INITIAL CODE ***********************************/
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
// Return 0 if the message is invalid
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, r, msg_len;
  int fd;
  char seq_id, expected_seq_id;

  fd = get_fd_for_receive();

#ifdef DEBUG
  printf("Node %i is going to receive on %i\n", node_id, fd);
#endif

  // get the header
  header_size = recvfrom(fd, (char*) msg, UDP_SEND_MAX_SIZE, 0, 0, 0);
  expected_seq_id = 0;
  seq_id = *((char*) msg + SEQ_ID_OFFSET);

#ifdef DEBUG_MORE
  if (node_id == 0)
  printf("[node %i] seq_id=%i, header_size=%lu\n", node_id, seq_id,
      header_size);
#endif

  if (seq_id != expected_seq_id)
  {
#ifdef DEBUG_MORE
    if (node_id == 0)
    printf("[node %i] Error: wrong seq number: %i instead of %i", node_id,
        seq_id, expected_seq_id);
#endif
    return 0;
  }

  // get the content
  msg_len = ((struct message_header*) msg)->len;

#ifdef DEBUG_MORE
  if (node_id == 0)
  printf("[node %i] Header size is %lu, msg_len is %lu\n", node_id,
      header_size, msg_len);
#endif

  s = header_size;
  while (s < msg_len)
  {
#ifdef DEBUG_MORE
    if (node_id == 0)
    printf("[node %i] s=%lu, msg_len=%lu\n", node_id, s, msg_len);
#endif

    r = recvfrom(fd, (char*) msg + s, msg_len - s, 0, 0, 0);
    s += r;

    expected_seq_id++;
    seq_id = *((char*) msg + s - r + SEQ_ID_OFFSET);

#ifdef DEBUG_MORE
    if (node_id == 0)
    printf("[node %i] seq_id=%i, r=%lu\n", node_id, seq_id, r);
#endif

    if (seq_id != expected_seq_id)
    {
#ifdef DEBUG_MORE
      if (node_id == 0)
      printf("[node %i] Error: wrong seq number: %i instead of %i", node_id,
          seq_id, expected_seq_id);
#endif
      return 0;
    }
  }

#ifdef DEBUG_MORE
  if (node_id == 0)
  printf("[node %i]l s=%lu, msg_len=%lu\n", node_id, s, msg_len);
#endif

  return s;
}
