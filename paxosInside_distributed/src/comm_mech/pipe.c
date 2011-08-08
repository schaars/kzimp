/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Pipe
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../Message.h"
#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// --[ HACK ]--
#define F_SETPIPE_SZ 1031

#define MAX(a,b) (((a)>(b))?(a):(b))

/********** All the variables needed by pipes **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

// all the pipes :)
// convention is: pipe[1] to send, pipe[0] to receive
int client_to_leader[2];
int leader_to_acceptor[2];
int **acceptor_to_learners;
int **learner_to_clients;

void init_pipe(int *pipefd)
{
  int ret;

  if (pipe(pipefd) == -1)
  {
    perror("Pipe error");
  }

  // set the size of the pipe's buffer
  ret = fcntl(pipefd[0], F_SETPIPE_SZ, 1048576);
  if (!ret)
  {
    perror("[IPC_Initialize] error when setting the size of pipefd[0]! ");
    exit(errno);
  }

  ret = fcntl(pipefd[1], F_SETPIPE_SZ, 1048576);
  if (!ret)
  {
    perror("[IPC_Initialize] error when setting the size of pipefd[1]! ");
    exit(errno);
  }
}

// Initialize resources for both the node and the nodes
// First initialization function called
void IPC_initialize(int _nb_paxos_nodes, int _nb_clients)
{
  int i;

  nb_paxos_nodes = _nb_paxos_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  // --[ malloc the arrays of pipe fds --]
  acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
      sizeof(*acceptor_to_learners) * nb_learners);
  if (!acceptor_to_learners)
  {
    perror("[IPC_Initialize] Allocation error for acceptor_to_learner pipes! ");
    exit(errno);
  }

  learner_to_clients = (typeof(learner_to_clients)) malloc(
      sizeof(*learner_to_clients) * nb_learners);
  if (!learner_to_clients)
  {
    perror("[IPC_Initialize] Allocation error for learner_to_client pipes! ");
    exit(errno);
  }

  for (i = 0; i < nb_learners; i++)
  {
    acceptor_to_learners[i] = (typeof(*acceptor_to_learners)) malloc(
        sizeof(**acceptor_to_learners) * 2);
    if (!acceptor_to_learners[i])
    {
      perror(
          "[IPC_Initialize] Allocation error for acceptor_to_learner[i] pipes! ");
      exit(errno);
    }

    learner_to_clients[i] = (typeof(*learner_to_clients)) malloc(
        sizeof(**learner_to_clients) * 2);
    if (!learner_to_clients[i])
    {
      perror(
          "[IPC_Initialize] Allocation error for learner_to_client[i] pipes! ");
      exit(errno);
    }
  }

  // --[ initialize the pipes ]--
  init_pipe(client_to_leader);
  init_pipe(leader_to_acceptor);

  for (i = 0; i < nb_learners; i++)
  {
    init_pipe(acceptor_to_learners[i]);
    init_pipe(learner_to_clients[i]);
  }
}

void initialize_one_node(void)
{
  int i;

  if (node_id == 0) // leader
  {
    // the leader needs to read in client_to_leader and write in leader_to_acceptor
    close(client_to_leader[1]);
    close(leader_to_acceptor[0]);

    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i][0]);
      close(acceptor_to_learners[i][1]);
      close(learner_to_clients[i][0]);
      close(learner_to_clients[i][1]);
    }
  }
  else if (node_id == 1) // acceptor
  {
    // reads in leader_to_acceptor and writes in acceptor_to_learner
    close(client_to_leader[0]);
    close(client_to_leader[1]);
    close(leader_to_acceptor[1]);

    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i][0]);
      close(learner_to_clients[i][0]);
      close(learner_to_clients[i][1]);
    }
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    //client 0 reads in learner_to_clients
    close(client_to_leader[0]);
    close(client_to_leader[1]);
    close(leader_to_acceptor[0]);
    close(leader_to_acceptor[1]);

    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i][0]);
      close(acceptor_to_learners[i][1]);
      close(learner_to_clients[i][1]);
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    //client 1 writes in client_to_leader
    close(client_to_leader[0]);
    close(leader_to_acceptor[0]);
    close(leader_to_acceptor[1]);

    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i][0]);
      close(acceptor_to_learners[i][1]);
      close(learner_to_clients[i][0]);
      close(learner_to_clients[i][1]);
    }
  }
  else // learners
  {
    // the learners read in acceptor_to_learners and write in learner_to_clients
    close(client_to_leader[0]);
    close(client_to_leader[1]);
    close(leader_to_acceptor[0]);
    close(leader_to_acceptor[1]);

    for (i = 0; i < nb_learners; i++)
    {
      if (i != node_id - 2)
      {
        close(acceptor_to_learners[i][0]);
        close(learner_to_clients[i][1]);
      }
      close(acceptor_to_learners[i][1]);
      close(learner_to_clients[i][0]);
    }
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
  int i;

  for (i = 0; i < nb_learners; i++)
  {
    free(acceptor_to_learners[i]);
    free(learner_to_clients[i]);
  }

  free(acceptor_to_learners);
  free(learner_to_clients);
}

void clean_one_node(void)
{
  int i;

  if (node_id == 0) // leader
  {
    close(client_to_leader[0]);
    close(leader_to_acceptor[1]);
  }
  else if (node_id == 1) // acceptor
  {
    close(leader_to_acceptor[0]);

    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i][1]);
    }
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    for (i = 0; i < nb_learners; i++)
    {
      close(learner_to_clients[i][0]);
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    close(client_to_leader[1]);
  }
  else // learners
  {
    close(acceptor_to_learners[node_id - 2][0]);
    close(learner_to_clients[node_id - 2][1]);
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

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  int r = write(leader_to_acceptor[1], msg, length);
  if (r < 0)
  {
    perror("IPC_send_node_unicast");
  }
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  int r;

  for (int i = 0; i < nb_learners; i++)
  {
    r = write(acceptor_to_learners[i][1], msg, length);
    if (r < 0)
    {
      perror("IPC_send_unicast");
    }
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  int r = write(client_to_leader[1], msg, length);
  if (r < 0)
  {
    perror("IPC_send_node_unicast");
  }
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  int r = write(learner_to_clients[node_id - 2][1], msg, length);
  if (r < 0)
  {
    perror("IPC_send_node_unicast");
  }
}

// get a file descriptor on which there is something to receive
int get_fd_for_recv(void)
{
  if (node_id == 0) // leader
  {
    return client_to_leader[0];
  }
  else if (node_id == 1) // acceptor
  {
    return leader_to_acceptor[0];
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    //select
    fd_set file_descriptors;
    struct timeval listen_time;
    int maxsock;

    while (1)
    {
      // select
      FD_ZERO(&file_descriptors); //initialize file descriptor set

      maxsock = 0;
      for (int j = 0; j < nb_learners; j++)
      {
        FD_SET(learner_to_clients[j][0], &file_descriptors);
        maxsock = MAX(maxsock, learner_to_clients[j][0]);
      }

      listen_time.tv_sec = 1;
      listen_time.tv_usec = 0;

      select(maxsock + 1, &file_descriptors, NULL, NULL, &listen_time);

      for (int j = 0; j < nb_learners; j++)
      {
        if (FD_ISSET(learner_to_clients[j][0], &file_descriptors))
        {
          return learner_to_clients[j][0];
        }
      }
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    printf("Node %i is receiving something. Weird!\n", node_id);
    return -1; // should not be there: client 1 never receives anything
  }
  else // learners
  {
    return acceptor_to_learners[node_id - 2][0];
  }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t header_size, s, left, msg_len;
  int fd;

  s = 0;

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
