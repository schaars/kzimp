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
#include <sys/select.h>

#ifdef KZIMP_SPLICE
#include <sys/mman.h>
#include <sys/ioctl.h>
#endif

#ifdef ONE_CHANNEL_PER_LEARNER
#include <time.h>
#endif

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define MESSAGE_MAX_SIZE as the max size of a message in the channel
// Define ONE_CHANNEL_PER_LEARNER if you want to run the version with 1 channel per learner i -> client 0
// Define KZIMP_SPLICE if you want to use the splice version of kzimp (no copy when sending)
// If KZIMP_SPLICE is defined, you also have to define CHANNEL_SIZE
#ifdef KZIMP_SPLICE
#define PAGE_SIZE 4096
#define KZIMP_IOCTL_SPLICE_WRITE 0x1
#endif

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

#ifdef KZIMP_SPLICE
static char *big_messages[CHANNEL_SIZE]; // array of messages
static int next_msg_idx;
#endif

#ifdef KZIMP_SPLICE
char* get_next_message(void)
{
  return big_messages[next_msg_idx];
}
#endif


// write wrapper which handles the errors
ssize_t Write(int fd, const void *buf, size_t count)
{
  int r;

#ifndef KZIMP_SPLICE
  r = write(fd, buf, count);
#else
  unsigned long kzimp_addr_struct[3];

  //  -arg[0]: user-space address of the message
  //  -arg[1]: offset in the big memory area
  //  -arg[2]: length
  kzimp_addr_struct[0] = (unsigned long) big_messages[next_msg_idx];
  kzimp_addr_struct[1] = next_msg_idx;
  kzimp_addr_struct[2] = count;

  r = ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, kzimp_addr_struct);

  next_msg_idx = (next_msg_idx+1)%CHANNEL_SIZE;
#endif

  if (r == -1)
  {
    switch (errno)
    {
    case ENOMEM:
      printf(
          "Node %i: memory allocation failed while calling write @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EFAULT:
      printf("Node %i: write buffer is invalid @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EINTR:
      printf("Node %i: write call has been interrupted @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EACCES:
      printf("Node %i: bad permission for fd %i @ %s:%i. Aborting.\n", node_id,
          fd, __FILE__, __LINE__);
      exit(-1);
      break;

    case EAGAIN:
      printf("Node %i: call would block @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EINVAL:
      printf("Node %i: bad ioctl command @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    default:
      perror("Error in write");
      printf("Node %i: write error @ %s:%i. Aborting.\n", node_id, __FILE__,
          __LINE__);
      exit(-1);
      break;
    }
  }
  return r;
}

// read wrapper which handles the errors
ssize_t Read(int fd, void *buf, size_t count)
{
  int r = read(fd, buf, count);
  if (r == -1)
  {
    switch (errno)
    {
    case EAGAIN:
      printf(
          "Node %i: non-blocking ops and call would block while calling read @ %s:%i.\n",
          node_id, __FILE__, __LINE__);
      break;

    case EFAULT:
      printf("Node %i: read buffer is invalid @ %s:%i.\n", node_id, __FILE__,
          __LINE__);
      return 0;
      break;

    case EINTR:
      printf("Node %i: read call has been interrupted @ %s:%i. Aborting.\n",
          node_id, __FILE__, __LINE__);
      exit(-1);
      break;

    case EBADF:
      printf("Node %i: reader no longer online @ %s:%i. Aborting.\n", node_id,
          __FILE__, __LINE__);
      exit(-1);
      break;

    case EIO:
      printf("Node %i: checksum is incorrect @ %s:%i.\n", node_id, __FILE__,
          __LINE__);
      return 0;
      break;

    default:
      perror("Error in read");
      printf("Node %i: read error @ %s:%i. Aborting.\n", node_id, __FILE__,
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
  nb_learners = nb_paxos_nodes - 2;
  nb_clients = _nb_clients;
  total_nb_nodes = nb_paxos_nodes + nb_clients;
}

#ifdef KZIMP_SPLICE
static void mmap_big_messages(int fd)
{
  int i;
  for (i = 0; i < CHANNEL_SIZE; i++)
  {
    big_messages[i] = (char*)mmap(NULL, MESSAGE_MAX_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED,
        fd, PAGE_SIZE * i);
    if (big_messages[i] == (void *) -1)
    {
      perror("mmap failed");
      exit(-1);
    }
  }

  next_msg_idx = 0;
}

static void munmap_big_messages(int fd)
{
  int i;
  for (i = 0; i < CHANNEL_SIZE; i++)
  {
    munmap(big_messages[i], MESSAGE_MAX_SIZE);
  }
}
#endif

static void init_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 0);
    client_to_leader = open(chaname, O_RDONLY);

    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 1);
    leader_to_acceptor = open(chaname, O_WRONLY);

    if (client_to_leader < 0 || leader_to_acceptor < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef KZIMP_SPLICE
    mmap_big_messages(leader_to_acceptor);
#endif
  }
  else if (node_id == 1)
  {
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 1);
    leader_to_acceptor = open(chaname, O_RDONLY);

    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 2);
    acceptor_multicast = open(chaname, O_WRONLY);

    if (leader_to_acceptor < 0 || acceptor_multicast < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef KZIMP_SPLICE
    mmap_big_messages(acceptor_multicast);
#endif
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
      snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, i+3);
      learneri_to_client[i] = open(chaname, O_RDONLY);

      if (learneri_to_client[i] < 0)
      {
        printf("Node %i experiences an error at %i\n", node_id, __LINE__);
        perror(">>> Error while opening channels\n");
      }
    }
#else
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 3);
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
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 0);
    client_to_leader = open(chaname, O_WRONLY);

    if (client_to_leader < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef KZIMP_SPLICE
    mmap_big_messages(client_to_leader);
#endif
  }
  else
  {
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 2);
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

    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, node_id - 2 + 3);
    *learneri_to_client = open(chaname, O_WRONLY);

    if (*learneri_to_client < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef KZIMP_SPLICE
    mmap_big_messages(*learneri_to_client);
#endif

#else
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 3);
    learners_to_client = open(chaname, O_WRONLY);

    if (learners_to_client < 0)
    {
      printf("Node %i experiences an error at %i\n", node_id, __LINE__);
      perror(">>> Error while opening channels\n");
    }

#ifdef KZIMP_SPLICE
    mmap_big_messages(learners_to_client);
#endif

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

#ifdef KZIMP_SPLICE
    munmap_big_messages(leader_to_acceptor);
#endif

    close(leader_to_acceptor);
  }
  else if (node_id == 1)
  {
    close(leader_to_acceptor);

#ifdef KZIMP_SPLICE
    munmap_big_messages(acceptor_multicast);
#endif

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

#ifdef KZIMP_SPLICE
    munmap_big_messages(client_to_leader);
#endif

    close(client_to_leader);
  }
  else
  {
    close(acceptor_multicast);

#ifdef ONE_CHANNEL_PER_LEARNER

#ifdef KZIMP_SPLICE
    munmap_big_messages(*learneri_to_client);
#endif

    close(*learneri_to_client);
    free(learneri_to_client);
#else

#ifdef KZIMP_SPLICE
    munmap_big_messages(learners_to_client);
#endif

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
  Write(leader_to_acceptor, msg, length);
}

// send the message msg of size length to all the nodes
void IPC_send_node_multicast(void *msg, size_t length)
{
  Write(acceptor_multicast, msg, length);
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
#ifdef ONE_CHANNEL_PER_LEARNER
  Write(*learneri_to_client, msg, length);
#else
  Write(learners_to_client, msg, length);
#endif
}

// receive a message and place it in msg (which is a buffer of size length).
// Return the number of read bytes.
size_t IPC_receive(void *msg, size_t length)
{
  if (node_id == 0)
  {
    return Read(client_to_leader, msg, length);
  }
  else if (node_id == 1)
  {
    return Read(leader_to_acceptor, msg, length);
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
            return Read(learneri_to_client[i], msg, length);
          }
        }
      }
    }

    return 0;
#else
    return Read(learners_to_client, msg, length);
#endif
  }
  else
  {
    return Read(acceptor_multicast, msg, length);
  }
}
