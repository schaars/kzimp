/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: kzimp
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef ONE_CHANNEL_PER_NODE
#include <time.h>
#endif

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define MESSAGE_MAX_SIZE as the max size of a message in the channel
// Define ONE_CHANNEL_PER_NODE if you want to run the version with 1 channel per learner i -> client 0

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

#define KZIMP_CHAR_DEV_FILE "/dev/kzimp"

/********** All the variables needed by ULM **********/
static int node_id;
static int nb_nodes;

static int multicast_0_to_all; // node 0 -> all but 0
#ifdef ONE_CHANNEL_PER_NODE
static int *nodei_to_0; // node i -> node 0 for all i. We do not use nodei_to_0[0]
#else
static int nodes_to_0; // all nodes but 0 -> node 0
#endif

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes)
{
  nb_nodes = _nb_nodes;

#ifdef ONE_CHANNEL_PER_NODE
  nodei_to_0 = (int*) malloc(sizeof(int) * nb_nodes);
  if (!nodei_to_0)
  {
    perror("malloc of nodei_to_0 failed");
  }
#endif
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 0);
    multicast_0_to_all = open(chaname, O_WRONLY);

#ifdef ONE_CHANNEL_PER_NODE
    int i;
    for (i = 1; i < nb_nodes; i++)
    {
      snprintf(chaname, 256, "/dev/kzimp%i", i);
      nodei_to_0[i] = open(chaname, O_WRONLY);
    }
#else
    snprintf(chaname, 256, "/dev/kzimp%i", 1);
    nodes_to_0 = open(chaname, O_RDONLY);
#endif
  }
  else
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 0);
    multicast_0_to_all = open(chaname, O_RDONLY);

#ifdef ONE_CHANNEL_PER_NODE
    snprintf(chaname, 256, "/dev/kzimp%i", node_id);
    nodei_to_0[node_id] = open(chaname, O_WRONLY);
#else
    snprintf(chaname, 256, "/dev/kzimp%i", 1);
    nodes_to_0 = open(chaname, O_WRONLY);
#endif
  }
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
#ifdef ONE_CHANNEL_PER_NODE
  free(nodei_to_0);
#endif
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  close(multicast_0_to_all);

#ifdef ONE_CHANNEL_PER_NODE
  if (node_id == 0)
  {
    int i;
    for (i = 1; i < nb_nodes; i++)
    {
      close(nodei_to_0[i]);
    }
  }
  else
  {
    close(nodei_to_0[node_id]);
  }
#else
  close(nodes_to_0);
#endif
}

// send the message msg of size length to all the nodes
void IPC_send_multicast(void *msg, size_t length)
{
  write(multicast_0_to_all, msg, length);
}

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
#ifdef ONE_CHANNEL_PER_NODE
  write(nodei_to_0[node_id], msg, length);
#else
  write(nodes_to_0, msg, length);
#endif
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
// blocking
size_t IPC_receive(void *msg, size_t length)
{
  ssize_t recv_size;

  if (node_id == 0)
  {
#ifdef ONE_CHANNEL_PER_NODE
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
        FD_SET(nodei_to_0[i], &set_read);
        max = ((nodei_to_0[i] > max) ? nodei_to_0[i] : max);
      }

      ret = select(max + 1, &set_read, NULL, NULL, &tv);
      if (ret)
      {
        for (i = 1; i < nb_nodes; i++)
        {
          if (FD_ISSET(nodei_to_0[i], &set_read))
          {
            return read(nodei_to_0[i], msg, length);
          }
        }
      }
    }

    recv_size = 0;
#else
    recv_size = read(nodes_to_0, msg, length);
#endif
  }
  else
  {
    recv_size = read(multicast_0_to_all, msg, length);
  }

  return (size_t) recv_size;
}
