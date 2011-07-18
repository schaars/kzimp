/* This file is part of multicore_replication_microbench.
 *
 * Communication mechanism: kernel version of Barrelfish message-passing
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

// Define NB_MESSAGES as the size of the channels
// Define MESSAGE_BYTES as the max size of the messages in bytes
// Define WAIT_TYPE as USLEEP or BUSY

#define KBFISH_CHAR_DEV_FILE "/dev/kbfish"

static int node_id;
static int nb_paxos_nodes;
static int nb_clients;
static int nb_learners;
static int total_nb_nodes;

static int client_to_leader; // connection between client 1 and the leader
static int leader_to_acceptor; // connection between leader and acceptor
static int *acceptor_to_learners; // connection between the acceptor and learner i
static int *learners_to_client; // connection between the learner i and the client 0


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
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_clients = _nb_clients;
  nb_learners = nb_paxos_nodes - 2;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

static void init_node(int _node_id)
{
  char chaname[256];
  int i;

  node_id = _node_id;

  if (node_id == 0) // leader
  {
    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, 0);
    client_to_leader = Open(chaname, O_RDWR | O_CREAT);

    // ensure that the receivers have Opened the file
    sleep(2);

    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, 1);
    leader_to_acceptor = Open(chaname, O_RDWR);
  }
  else if (node_id == 1) // acceptor
  {
    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, 1);
    leader_to_acceptor = Open(chaname, O_RDWR | O_CREAT);

    // ensure that the receivers have Opened the file
    sleep(2);

    acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
        sizeof(*acceptor_to_learners) * nb_learners);
    if (!acceptor_to_learners)
    {
      perror("Allocation error of the acceptor_to_learners channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, i + 2);
      acceptor_to_learners[i] = Open(chaname, O_RDWR);
    }
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    learners_to_client = (typeof(learners_to_client)) malloc(
        sizeof(*learners_to_client) * nb_learners);
    if (!learners_to_client)
    {
      perror("Allocation error of the learners_to_client channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, i + 2 + nb_learners);
      learners_to_client[i] = Open(chaname, O_RDWR | O_CREAT);
    }
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    // ensure that the receivers have Opened the file
    sleep(2);

    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, 0);
    client_to_leader = Open(chaname, O_RDWR);
  }
  else // learners
  {
    acceptor_to_learners = (typeof(acceptor_to_learners)) malloc(
        sizeof(*acceptor_to_learners));
    if (!acceptor_to_learners)
    {
      perror("Allocation error of the acceptor_to_learners channels");
      exit(-1);
    }

    snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, node_id);
    acceptor_to_learners[0] = Open(chaname, O_RDWR | O_CREAT);

    // ensure that the receivers have Opened the file
    sleep(2);

    learners_to_client = (typeof(learners_to_client)) malloc(
        sizeof(*learners_to_client));
    if (!learners_to_client)
    {
      perror("Allocation error of the learners_to_client channels");
      exit(-1);
    }

    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "%s%i", KBFISH_CHAR_DEV_FILE, node_id
          + nb_learners);
      learners_to_client[0] = Open(chaname, O_RDWR);
    }
  }
}

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  init_node(_node_id);
}

// Initialize resources for the client of id _client_id
void IPC_initialize_client(int _client_id)
{
  init_node(_client_id);
}

// Clean resources
// Called by the parent process, after the death of the children.
void IPC_clean(void)
{
}

static void clean_node(void)
{
  int i;

  if (node_id == 0) // leader
  {
    close(leader_to_acceptor);
    close(client_to_leader);
  }
  else if (node_id == 1) // acceptor
  {
    for (i = 0; i < nb_learners; i++)
    {
      close(acceptor_to_learners[i]);
    }
    free(acceptor_to_learners);

    close(leader_to_acceptor);
  }
  else if (node_id == nb_paxos_nodes) // client 0
  {
    for (i = 0; i < nb_learners; i++)
    {
      close(learners_to_client[i]);
    }
    free(learners_to_client);
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    close(client_to_leader);
  }
  else // learners
  {
    close(learners_to_client[0]);
    free(learners_to_client);

    close(acceptor_to_learners[0]);
    free(acceptor_to_learners);
  }
}

// Clean resources created for the (paxos) node.
void IPC_clean_node(void)
{
  clean_node();
}

// Clean resources created for the client.
void IPC_clean_client(void)
{
  clean_node();
}

// send the message msg of size length to the node 1
// Indeed the only unicast is from 0 to 1
void IPC_send_node_unicast(void *msg, size_t length)
{
  Write(leader_to_acceptor, msg, length);
}

// send the message msg of size length to all the learners
void IPC_send_node_multicast(void *msg, size_t length)
{
  for (int l = 0; l < nb_learners; l++)
  {
#ifdef DEBUG
    printf("Node %i is going to send a multicast message to learner %i\n",
        node_id, l + 2);
#endif
    Write(acceptor_to_learners[l], msg, length);
  }

#ifdef DEBUG
  printf("Node %i has finished to send multicast messages\n", node_id);
#endif
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  Write(client_to_leader, msg, length);
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
#ifdef DEBUG
  printf("Node %i is going to send a message to client %i\n", node_id, cid);
#endif

  Write(learners_to_client[0], msg, length);
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of Read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  size_t recv_size;

  if (node_id == 0) // leader
  {
    recv_size = Read(client_to_leader, msg, length);
  }
  else if (node_id == 1) // acceptor
  {
    recv_size = Read(leader_to_acceptor, msg, length);
  }
  else if (node_id == nb_paxos_nodes) // client 0
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
      for (i = 0; i < nb_learners; i++)
      {
        FD_SET(learners_to_client[i], &set_read);
        max = ((learners_to_client[i] > max) ? learners_to_client[i] : max);
      }

      ret = select(max + 1, &set_read, NULL, NULL, &tv);
      if (ret)
      {
        for (i = 0; i < nb_learners; i++)
        {
          if (FD_ISSET(learners_to_client[i], &set_read))
          {
            return Read(learners_to_client[i], msg, length);
          }
        }
      }
    }

    return 0;
  }
  else if (node_id > nb_paxos_nodes) // client 1
  {
    // client 1 never receives anything
    recv_size = 0;
  }
  else // learners
  {
    recv_size = Read(acceptor_to_learners[0], msg, length);
  }

  return recv_size;
}
