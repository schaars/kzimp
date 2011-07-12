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

#ifdef ONE_CHANNEL_PER_LEARNER
#include <time.h>
#endif

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define MESSAGE_MAX_SIZE as the max size of a message in the channel
// Define ONE_CHANNEL_PER_LEARNER if you want to run the version with 1 channel per learner i -> client 0

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

#define KZIMP_CHAR_DEV_FILE "/dev/kzimp"

/********** All the variables needed by ULM **********/

static int node_id;
static int nb_paxos_nodes;
static int nb_learners;
static int nb_clients;
static int total_nb_nodes;

// channels file descriptors
static int client_to_leader; // client 1 -> leader
static int leader_to_acceptor; // leader -> acceptor
static int acceptor_multicast; // acceptor -> learners
#ifdef ONE_CHANNEL_PER_LEARNER
static int *learneri_to_client; // learner i -> client 0
#else
static int learners_to_client; // learners -> client 0
#endif

// Initialize resources for both the node and the clients
// First initialization function called
void IPC_initialize(int _nb_nodes, int _nb_clients)
{
  nb_paxos_nodes = _nb_nodes;
  nb_learners = nb_paxos_nodes - 2;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

static void init_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 0);
    client_to_leader = open(chaname, O_RDONLY);

    snprintf(chaname, 256, "/dev/kzimp%i", 1);
    leader_to_acceptor = open(chaname, O_WRONLY);

    if (client_to_leader < 0 || leader_to_acceptor < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }
  }
  else if (node_id == 1)
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 1);
    leader_to_acceptor = open(chaname, O_RDONLY);

    snprintf(chaname, 256, "/dev/kzimp%i", 2);
    acceptor_multicast = open(chaname, O_WRONLY);

    if (leader_to_acceptor < 0 || acceptor_multicast < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }
  }
  else if (node_id == nb_paxos_nodes)
  {
#ifdef ONE_CHANNEL_PER_LEARNER
    learneri_to_client = (int*) malloc(sizeof(int) * nb_learners);
    if (!learneri_to_client)
    {
      perror("Allocation failed: ");
      exit(-1);
    }

    int i;
    for (i = 0; i < nb_learners; i++)
    {
      snprintf(chaname, 256, "/dev/kzimp%i", i+3);
      learneri_to_client[i] = open(chaname, O_RDONLY);

      if (learneri_to_client[i] < 0)
      {
        printf("Node %i experiences an error at %i\n", node_id, __LINE__);
        perror(">>> Error while opening channels\n");
      }
    }
#else
    snprintf(chaname, 256, "/dev/kzimp%i", 3);
    learners_to_client = open(chaname, O_RDONLY);

    if (learners_to_client < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }
#endif
  }
  else if (node_id > nb_paxos_nodes)
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 0);
    client_to_leader = open(chaname, O_WRONLY);

    if (client_to_leader < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }
  }
  else
  {
    snprintf(chaname, 256, "/dev/kzimp%i", 2);
    acceptor_multicast = open(chaname, O_RDONLY);

    if (acceptor_multicast < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef ONE_CHANNEL_PER_LEARNER
    learneri_to_client = (int*) malloc(sizeof(int));
    if (!learneri_to_client)
    {
      perror("Allocation failed: ");
      exit(-1);
    }

    snprintf(chaname, 256, "/dev/kzimp%i", node_id - 2 + 3);
    *learneri_to_client = open(chaname, O_WRONLY);

    if (*learneri_to_client < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#else
    snprintf(chaname, 256, "/dev/kzimp%i", 3);
    learners_to_client = open(chaname, O_WRONLY);

    if (learners_to_client < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }
#endif
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
  if (node_id == 0)
  {
    close(client_to_leader);
    close(leader_to_acceptor);
  }
  else if (node_id == 1)
  {
    close(leader_to_acceptor);
    close(acceptor_multicast);
  }
  else if (node_id == nb_paxos_nodes)
  {
#ifdef ONE_CHANNEL_PER_LEARNER
    int i;
    for (i = 0; i < nb_learners; i++)
    {
      close(learneri_to_client[i]);
    }
    free(learneri_to_client);
#else
    close(learners_to_client);
#endif
  }
  else if (node_id > nb_paxos_nodes)
  {
    close(client_to_leader);
  }
  else
  {
    close(acceptor_multicast);
#ifdef ONE_CHANNEL_PER_LEARNER
    close(*learneri_to_client);
    free(learneri_to_client);
#else
    close(learners_to_client);
#endif
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
  write(leader_to_acceptor, msg, length);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  write(acceptor_multicast, msg, length);
}

// send the message msg of size length to the node 0
// called by a client
void IPC_send_client_to_node(void *msg, size_t length)
{
  write(client_to_leader, msg, length);
}

// send the message msg of size length to the client of id cid
// called by the learners
void IPC_send_node_to_client(void *msg, size_t length, int cid)
{
#ifdef ONE_CHANNEL_PER_LEARNER
  write(*learneri_to_client, msg, length);
#else
  write(learners_to_client, msg, length);
#endif
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  if (node_id == 0)
  {
    return read(client_to_leader, msg, length);
  }
  else if (node_id == 1)
  {
    return read(leader_to_acceptor, msg, length);
  }
  else if (node_id == nb_paxos_nodes)
  {
#ifdef ONE_CHANNEL_PER_LEARNER
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
        FD_SET(learneri_to_client[i], &set_read);
        max = ((learneri_to_client[i] > max) ? learneri_to_client[i] : max);
      }

      ret = select(max + 1, &set_read, NULL, NULL, &tv);
      if (ret)
      {
        for (i = 0; i < nb_learners; i++)
        {
          if (FD_ISSET(learneri_to_client[i], &set_read))
          {
            return read(learneri_to_client[i], msg, length);
          }
        }
      }
    }

    return 0;
#else
    return read(learners_to_client, msg, length);
#endif
  }
  else
  {
    return read(acceptor_multicast, msg, length);
  }
}
