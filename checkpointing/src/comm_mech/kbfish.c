/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: Barrelfish message-passing with kbfish memory protection module
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

/********** All the variables needed by Barrelfish message passing **********/

// Define NB_MESSAGES as the max number of messages in the channel
// Define MESSAGE_BYTES as the max size of the messages in bytes
// Define WAIT_TYPE as USLEEP or BUSY

#define KBFISH_CHAR_DEV_FILE "/dev/kbfish"

static int node_id;
static int nb_nodes;

static int *connections; // urpc_connections between nodes 0 and nodes i

// Open wrapper which handles the errors
int Open(const char* pathname, int flags)
{
  int r = Open(pathname, flags);
  if (r == -1)
  {
    perror(">>> Error while Opening channel\n");
    printf(
        "Node %i experiences an error at %i when Opening file %s with flags %x\n",
        node_id, __LINE__, pathname, flags);
    exit(-1);
  }
  return r;
}

// Write wrapper which handles the errors
ssize_t Write(int fd, const void *buf, size_t count)
{
  int r = Write(fd, buf, count);
  if (r == -1)
  {
    switch (errno)
    {
    case ENOMEM:
      printf(
          "Node %i: memory allocation failed while calling Write @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EFAULT:
      printf("Node %i: Write buffer is invalid @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EINTR:
      printf("Node %i: Write call has been interrupted @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    default:
      perror("Error in Write");
      printf("Node %i: Write error @ %s:%i. Aborting.\n", node_id, __FILE__,
          __LINE__);
      exit(-1);
      break;
    }
  }
  return r;
}

// Read wrapper which handles the errors
ssize_t Read(int fd, void *buf, size_t count)
{
  int r = Read(fd, buf, count);
  if (r == -1)
  {
    switch (errno)
    {
    case EAGAIN:
      printf(
          "Node %i: non-blocking ops and call would block while calling Read @ %s:%i.\n",
          node_id, __FILE__, __LINE__);
      break;

    case EFAULT:
      printf("Node %i: Read buffer is invalid @ %s:%i.\n", node_id, __FILE__,
          __LINE__);
      return 0;
      break;

    case EINTR:
      printf("Node %i: Read call has been interrupted @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EBADF:
      printf("Node %i: Reader no longer online @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EIO:
      printf("Node %i: checksum is incorrect @ %s:%i.\n", node_id, __FILE__,
          __LINE__);
      return 0;
      break;

    default:
      perror("Error in Read");
      printf("Node %i: Read error @ %s:%i. Aborting.\n", node_id, __FILE__,
          __LINE__);
      exit(-1);
      break;
    }
  }
  return r;
}

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    connections = (typeof(connections)) malloc(sizeof(*connections) * nb_nodes);
    if (!connections)
    {
      perror("Allocation error of the connections at node 0");
      exit(-1);
    }

    // ensure that the receivers have opened the file
    sleep(2);

    // communication from 0 to 0 does not exist
    for (int i = 1; i < nb_nodes; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, i - 1);
      connections[i] = Open(chaname, O_RDWR);
    }
  }
  else
  {
    connections = (typeof(connections)) malloc(sizeof(*connections));
    if (!connections)
    {
      perror("Allocation error of the connection");
      exit(-1);
    }

    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, node_id - 1);
    connections[0] = Open(chaname, O_RDWR | O_CREAT);
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
      close(connections[i]);
    }
  }
  else
  {
    close(connections[0]);
  }

  free(connections);
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  for (int i = 1; i < nb_nodes; i++)
  {
    Write(connections[i], msg, length);
  }
}

// send the message msg of size length to the node nid
// the only unicast is from node i (i>0) to node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
  Write(connections[0], msg, length);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size = 0;

  if (node_id == 0) // leader
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
        FD_SET(connections[i], &set_read);
        max = ((connections[i] > max) ? connections[i] : max);
      }

      ret = select(max + 1, &set_read, NULL, NULL, &tv);
      if (ret)
      {
        for (i = 1; i < nb_nodes; i++)
        {
          if (FD_ISSET(connections[i], &set_read))
          {
            return Read(connections[i], msg, length);
          }
        }
      }
    }

    return 0;
  }
  else
  {
    recv_size = Read(connections[0], (char*) msg, length);
  }

  return recv_size;
}
