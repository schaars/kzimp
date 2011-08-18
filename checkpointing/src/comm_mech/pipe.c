/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: pipes
 * pipefd[0] refers to the read end of the pipe.
 * pipefd[1] refers to the write end of the pipe.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// --[ HACK ]--
#define F_SETPIPE_SZ 1031

/********** All the variables needed by ULM **********/
static int node_id;
static int nb_nodes;

static int **node0_to_all; // the pipes from node 0 to the other nodes
static int **nodei_to_0; // the pipes from node i to node 0, for all i > 0

void init_pipe(int **pipefd)
{
  int ret;

  *pipefd = (int *) malloc(sizeof(**pipefd) * 2);
  if (!*pipefd)
  {
    printf("pipefd allocation error\n");
    perror("");
  }

  if (pipe(*pipefd) == -1)
  {
    perror("Pipe error");
  }

  // set the size of the pipe's buffer
  ret = fcntl((*pipefd)[0], F_SETPIPE_SZ, 1048576);
  if (!ret)
  {
    perror("[IPC_Initialize] error when setting the size of pipefd[i][0]! ");
    exit(errno);
  }

  ret = fcntl((*pipefd)[1], F_SETPIPE_SZ, 1048576);
  if (!ret)
  {
    perror("[IPC_Initialize] error when setting the size of pipefd[i][1]! ");
    exit(errno);
  }
}

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  int i;

  nb_nodes = _nb_nodes;

  node0_to_all = (int **) malloc(sizeof(*node0_to_all) * nb_nodes);
  if (!node0_to_all)
  {
    printf("node0_to_all allocation error\n");
    perror("");
  }

  nodei_to_0 = (int **) malloc(sizeof(*nodei_to_0) * nb_nodes);
  if (!nodei_to_0)
  {
    printf("nodei_to_0 allocation error\n");
    perror("");
  }

  for (i = 0; i < nb_nodes; i++)
  {
    init_pipe(&node0_to_all[i]);
    init_pipe(&nodei_to_0[i]);
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  int i;

  node_id = _node_id;

  if (node_id == 0)
  {
    // I send to all but me and read from all but me
    close(node0_to_all[0][0]);
    close(node0_to_all[0][1]);
    close(nodei_to_0[0][0]);
    close(nodei_to_0[0][1]);

    for (i = 1; i < nb_nodes; i++)
    {
      close(node0_to_all[i][0]);
      close(nodei_to_0[i][1]);
    }
  }
  else
  {
    // I read from node_id and send to node_id
    for (i = 0; i < nb_nodes; i++)
    {
      if (i != node_id)
      {
        close(node0_to_all[i][0]);
        close(node0_to_all[i][1]);
        close(nodei_to_0[i][0]);
        close(nodei_to_0[i][1]);
      }
      else
      {
        close(node0_to_all[i][1]);
        close(nodei_to_0[i][0]);
      }
    }
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  int i;

  for (i = 0; i < nb_nodes; i++)
  {
    free(node0_to_all[i]);
    free(nodei_to_0[i]);
  }
  free(node0_to_all);
  free(nodei_to_0);
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  int i;

  if (node_id == 0)
  {
    for (i = 1; i < nb_nodes; i++)
    {
      close(node0_to_all[i][1]);
      close(nodei_to_0[i][0]);
    }
  }
  else
  {
    close(node0_to_all[node_id][0]);
    close(nodei_to_0[node_id][1]);
  }
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

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  int i;

  for (i = 1; i < nb_nodes; i++)
  {
    Write(node0_to_all[i][1], msg, length);
  }
}

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  Write(nodei_to_0[node_id][1], msg, length);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  ssize_t recv_size;

  if (node_id == 0)
  {
    while (1)
    {
      int i, ret;
      fd_set set_read;
      struct timeval tv;

      FD_ZERO(&set_read);

      tv.tv_sec = 1; // timeout in seconds
      tv.tv_usec = 0;

      int max = 0;
      for (i = 1; i < nb_nodes; i++)
      {
        FD_SET(nodei_to_0[i][0], &set_read);
        max = ((nodei_to_0[i][0] > max) ? nodei_to_0[i][0] : max);
      }

      ret = select(max + 1, &set_read, NULL, NULL, &tv);
      if (ret)
      {
        for (i = 1; i < nb_nodes; i++)
        {
          if (FD_ISSET(nodei_to_0[i][0], &set_read))
          {
            return read(nodei_to_0[i][0], msg, length);
          }
        }
      }
    }

    recv_size = 0;
  }
  else
  {
    return read(node0_to_all[node_id][0], msg, length);
  }

  return (size_t) recv_size;
}
