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

#ifdef VMSPLICE
#include <sys/eventfd.h>
#endif

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

#ifdef VMSPLICE
static int *eventfd_nodes; // one eventfd per node; VMSPLICE only
static int *eventfd_learners; // one eventfd per learner, used by the acceptor; VMSPLICE only
#endif

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

#ifdef VMSPLICE
  eventfd_nodes = (typeof(eventfd_nodes))malloc(sizeof(*eventfd_nodes) * total_nb_nodes);
  if (!eventfd_nodes)
  {
    perror("[IPC_Initialize] Allocation error for eventfd_nodes! ");
    exit(errno);
  }

  for (i=0; i<total_nb_nodes; i++) {
     eventfd_nodes[i] = eventfd(0, 0);
     if (eventfd_nodes[i] == -1) {
        perror("eventfd");
        exit(-1);
     }
  }

  eventfd_learners = (typeof(eventfd_learners))malloc(sizeof(*eventfd_learners) * nb_learners);
  if (!eventfd_learners)
  {
    perror("[IPC_Initialize] Allocation error for eventfd_learners! ");
    exit(errno);
  }

  for (i=0; i<nb_learners; i++) {
     eventfd_learners[i] = eventfd(0, 0);
     if (eventfd_learners[i] == -1) {
        perror("eventfd");
        exit(-1);
     }
  }
#endif

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

#ifdef VMSPLICE
  for (i=0; i<nb_learners; i++) {
     close(eventfd_learners[i]);
  }
  free(eventfd_learners);

  for (i=0; i< total_nb_nodes; i++) {
   close(eventfd_nodes[i]);
  }
  free(eventfd_nodes);
#endif
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

// wrapper to write or vmsplice
void Write(int fd, void *msg, size_t length) {
   int r;

#ifdef VMSPLICE
  struct iovec iov;
  iov.iov_base = msg;
  iov.iov_len = length;
  r = vmsplice(fd, &iov, 1, 0);
#else
  r = write(fd, msg, length);
#endif

  if (r < 0)
  {
    perror("Write");
  }
}

#ifdef VMSPLICE
void _get_notified(int fd) {
  int r;
  uint64_t vv;

#ifdef DEBUG
     printf("Node %i is waiting on %i\n", node_id, fd);
#endif

     r = read(fd, &vv, sizeof(vv));
     if (r < 0)
     {
        perror("Write read");
     }

#ifdef DEBUG
     printf("Node %i has waited on %i\n", node_id, fd);
#endif
}

void get_notified(int is_acceptor) {
   if (is_acceptor) {
      for (int i = 0; i < nb_learners; i++)
      {
         _get_notified(eventfd_learners[i]);
      }
   } else {
      _get_notified(eventfd_nodes[node_id]);
   }
}
#else
#define get_notified(is_acceptor) do {} while (0)
#endif

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
   Write(leader_to_acceptor[1], msg, length);
   get_notified(0);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
   for (int i = 0; i < nb_learners; i++)
   {
      Write(acceptor_to_learners[i][1], msg, length);
   }
   get_notified(1);
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
   Write(client_to_leader[1], msg, length);
   get_notified(0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
   Write(learner_to_clients[node_id - 2][1], msg, length);
   get_notified(0);
}

// get a file descriptor on which there is something to receive
// src_id will contain the id of the sender
int get_fd_for_recv(int *src_id)
{
   if (node_id == 0) // leader
   {
      *src_id = nb_paxos_nodes+1;
      return client_to_leader[0];
   }
   else if (node_id == 1) // acceptor
   {
      *src_id = 0;
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
               *src_id = 2 + j;
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
      *src_id = 1;
      return acceptor_to_learners[node_id - 2][0];
   }
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
   ssize_t s;
   int fd;
   int src_id;

   fd = get_fd_for_recv(&src_id);

   // receive the whole message
   s = read(fd, (void*)msg, length);
   if (s == -1) {
      perror("IPC_receive");
   } else if (s == 0) {
      printf("EOF on node %i, fd %i\n", node_id, fd);
      exit(-1);
   }

#ifdef DEBUG
   printf("Node %i is receiving a message with fd=%i\n", node_id, fd);
#endif

#ifdef VMSPLICE
#ifdef DEBUG
   printf("Node %i is writing on node %i: %i\n", node_id, src_id, eventfd_nodes[src_id]);
#endif

   uint64_t vv = 0x1;
   int r;

   if (node_id >= 2 && node_id < 2+nb_learners) {
      r = write(eventfd_learners[node_id-2], &vv, sizeof(vv));
   } else {
      r = write(eventfd_nodes[src_id], &vv, sizeof(vv));
   }

   if (r < 0)
   {
      perror("IPC_receive write");
   }
#endif

   return (size_t)s;
}
