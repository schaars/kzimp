/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: POSIX message queue
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../Message.h"
#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

/********** All the variables needed by POSIX message queue **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int total_nb_nodes;

static mqd_t *posix_queues; // all the queues

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;

  posix_queues = (mqd_t*) malloc(sizeof(mqd_t) * total_nb_nodes);
  if (!posix_queues)
  {
    perror("Allocation error");
    exit(errno);
  }

  /****** get and set message queues properties ******/
  struct rlimit rlim;
  int res;

#ifdef DEBUG
  res = getrlimit(RLIMIT_MSGQUEUE, &rlim);
  if (res == -1)
  {
    perror("getrlimit error");
  }
  else
  {
    printf("===== BEFORE MODIFICATION\nsoft=%i, hard=%i\n",
        (int) rlim.rlim_cur, (int) rlim.rlim_max);
  }
#endif

  rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
  res = setrlimit(RLIMIT_MSGQUEUE, &rlim);
  if (res == -1)
  {
    perror("setrlimit error");
    //exit(-1);
  }

#ifdef DEBUG
  res = getrlimit(RLIMIT_MSGQUEUE, &rlim);
  if (res == -1)
  {
    perror("getrlimit error");
  }
  else
  {
    printf("===== AFTER MODIFICATION\nsoft=%i, hard=%i\n", (int) rlim.rlim_cur,
        (int) rlim.rlim_max);
  }
#endif

  char filename[256];

  for (int i = 0; i < total_nb_nodes; i++)
  {
    sprintf(filename, "/posix_message_queue_paxosInside%i", i + 1);

    if ((posix_queues[i] = mq_open(filename, O_RDWR | O_CREAT, 0600, NULL))
        == -1)
    {
      perror("mq_open");
      exit(1);
    }

    struct mq_attr attr;
    int ret = mq_getattr(posix_queues[i], &attr);
    if (!ret)
    {
#ifdef DEBUG
      printf(
          "New queue. Attributes are:\n\tflags=%li, maxmsg=%li, msgsize=%li, curmsgs=%li\n",
          attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
#endif
    }
    else
    {
      perror("mq_getattr");
    }
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  node_id = _node_id;
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  node_id = _client_id;
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
  char filename[256];

  for (int i = 0; i < total_nb_nodes; i++)
  {
    sprintf(filename, "/posix_message_queue_microbench%i", i + 1);
    mq_unlink(filename);
  }
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  for (int i = 0; i < total_nb_nodes; i++)
  {
    mq_close(posix_queues[i]);
  }
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  for (int i = 0; i < total_nb_nodes; i++)
  {
    mq_close(posix_queues[i]);
  }
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  mq_send(posix_queues[1], (char*) msg, length, 0);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int i = 0; i < nb_paxos_nodes; i++)
  {
    if (i == 1)
    {
      continue;
    }

    mq_send(posix_queues[i], (char*) msg, length, 0);
  }
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  mq_send(posix_queues[0], (char*) msg, length, 0);
}

// send the message msg of size length to the client of id cid
// called by the leader
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
  mq_send(posix_queues[cid], (char*) msg, length, 0);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  return mq_receive(posix_queues[node_id], (char*) msg, length, NULL);
}
