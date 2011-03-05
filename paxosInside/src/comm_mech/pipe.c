/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Pipe
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>

#include "../Message.h"
#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define MAX(a,b) (((a)>(b))?(a):(b))

/********** All the variables needed by pipes **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;

// all the pipes :)
// convention is: pipe[1] to send, pipe[0] to receive
int **client_to_leader;
int **leader_to_client;
int **acceptor_to_node;
int leader_to_acceptor[2];

// Initialize resources for both the node and the nodes
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_paxos_nodes;
  nb_clients = _nb_clients;

  // create the pipes
  client_to_leader = (int**) malloc(sizeof(int*) * nb_clients);
  if (!client_to_leader)
  {
    perror("[IPC_Initialize] Allocation error for client_to_leader pipes! ");
    exit(errno);
  }

  // create the pipes
  leader_to_client = (int**) malloc(sizeof(int*) * nb_clients);
  if (!leader_to_client)
  {
    perror("[IPC_Initialize] Allocation error for client_to_leader pipes! ");
    exit(errno);
  }

  acceptor_to_node = (int**) malloc(sizeof(int*) * nb_paxos_nodes);
  if (!acceptor_to_node)
  {
    perror("[IPC_Initialize] Allocation error for acceptor_to_node pipes! ");
    exit(errno);
  }

  for (int i = 0; i < nb_clients; i++)
  {
    client_to_leader[i] = (int*) malloc(sizeof(int) * 2);
    leader_to_client[i] = (int*) malloc(sizeof(int) * 2);

    pipe(client_to_leader[i]);
    pipe(leader_to_client[i]);
  }

  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    acceptor_to_node[i] = (int*) malloc(sizeof(int) * 2);

    pipe(acceptor_to_node[i]);
  }

  pipe(leader_to_acceptor);
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;

  // close all the non-used pipes
  for (int i = 0; i < nb_clients; i++)
  {
    if (node_id == 0)
    {
      close(client_to_leader[i][1]);
      close(leader_to_client[i][0]);
    }
    else
    {
      close(client_to_leader[i][0]);
      close(client_to_leader[i][1]);
      close(leader_to_client[i][0]);
      close(leader_to_client[i][1]);
    }
  }

  if (node_id == 0)
  {
    close(leader_to_acceptor[0]);
  }
  else if (node_id == 1)
  {
    close(leader_to_acceptor[1]);
  }
  else
  {
    close(leader_to_acceptor[0]);
    close(leader_to_acceptor[1]);
  }

  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    if (node_id != 1)
    {
      close(acceptor_to_node[i][1]);
    }

    if (i != node_id)
    {
      close(acceptor_to_node[i][0]);
    }
  }

  if (node_id == 1)
  {
    close(acceptor_to_node[node_id][0]);
    close(acceptor_to_node[node_id][1]);
  }
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;

  for (int i = 0; i < nb_clients; i++)
  {
    if (i == node_id - nb_paxos_nodes)
    {
      close(client_to_leader[i][0]);
      close(leader_to_client[i][1]);
    }
    else
    {
      close(client_to_leader[i][0]);
      close(client_to_leader[i][1]);
      close(leader_to_client[i][0]);
      close(leader_to_client[i][1]);
    }
  }

  close(leader_to_acceptor[0]);
  close(leader_to_acceptor[1]);

  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    close(acceptor_to_node[i][0]);
    close(acceptor_to_node[i][1]);
  }

}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  for (int i = 0; i < nb_clients; i++)
  {
    free(client_to_leader[i]);
    free(leader_to_client[i]);
  }

  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    free(acceptor_to_node[i]);
  }

  free(acceptor_to_node);
  free(leader_to_client);
  free(client_to_leader);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  if (node_id == 0)
  {
    for (int i = 0; i < nb_clients; i++)
    {
      close(client_to_leader[i][0]);
      close(leader_to_client[i][1]);
    }

    close(leader_to_acceptor[1]);
  }
  else if (node_id == 1)
  {
    close(leader_to_acceptor[0]);
  }

  if (node_id == 1)
  {
    for (int i = 0; i < nb_paxos_nodes; i++)
    {
      if (i == node_id)
      {
        continue;
      }

      close(acceptor_to_node[i][1]);
    }
  }
  else
  {
    close(acceptor_to_node[node_id][0]);
  }
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  close(client_to_leader[node_id - nb_paxos_nodes][1]);
  close(leader_to_client[node_id - nb_paxos_nodes][0]);
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  write(leader_to_acceptor[1], msg, length);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    if (i == node_id)
    {
      continue;
    }

    write(acceptor_to_node[i][1], msg, length);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  write(client_to_leader[node_id - nb_paxos_nodes][1], msg, length);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  write(leader_to_client[cid - nb_paxos_nodes][1], msg, length);
}

// select on the file descriptors on which node 0 (aka the leader)
// reads. Then return a fd on which there is something to read.
int get_fd_by_select_node0(void)
{
  fd_set file_descriptors;
  struct timeval listen_time;
  int maxsock;

  while (1)
  {
    // select
    FD_ZERO(&file_descriptors); //initialize file descriptor set

    FD_SET(acceptor_to_node[node_id][0], &file_descriptors);
    maxsock = acceptor_to_node[node_id][0];

    for (int j = 0; j < nb_clients; j++)
    {
      FD_SET(client_to_leader[j][0], &file_descriptors);
      maxsock = MAX(maxsock, client_to_leader[j][0]);
    }

    listen_time.tv_sec = 1;
    listen_time.tv_usec = 0;

    select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

    if (FD_ISSET(acceptor_to_node[node_id][0], &file_descriptors))
    {
      return acceptor_to_node[node_id][0];
    }

    for (int j = 0; j < nb_clients; j++)
    {
      if (FD_ISSET(client_to_leader[j][0], &file_descriptors))
      {
        return client_to_leader[j][0];
      }
    }
  }
}

// get a file descriptor on which there is something to receive
int get_fd_for_recv(void)
{
  if (node_id == 0)
  {
    return get_fd_by_select_node0();
  }
  else if (node_id == 1)
  {
    return leader_to_acceptor[0];
  }
  else if (node_id < nb_paxos_nodes)
  {
    return acceptor_to_node[node_id][0];
  }
  else
  {
    return leader_to_client[node_id - nb_paxos_nodes][0];
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, left, msg_len;
  int fd;

  fd = get_fd_for_recv();

  // receive the message_header
  header_size = read(fd, (void*) msg, sizeof(struct message_header));

  // receive the message content
  msg_len = ((struct message_header*) msg)->len;
  left = msg_len - header_size;
  if (left > 0)
  {
    s = read(fd, (void*) ((char*) msg + header_size), left);
  }

#ifdef DEBUG
  printf("Node %i is receiving a message with fd=%i\n", node_id, fd);
#endif

  return header_size + s;
}
