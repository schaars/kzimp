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

#if defined(KZIMP_READ_SPLICE)
#include <sys/mman.h>
#include <sys/ioctl.h>
#endif

#ifdef ONE_CHANNEL_PER_NODE
#include <time.h>
#endif

#include "ipc_interface.h"

// debug macro
#define DEBUG
#undef DEBUG

// Define MESSAGE_MAX_SIZE as the max size of a message in the channel
// Define ONE_CHANNEL_PER_NODE if you want to run the version with 1 channel per learner i -> client 0
// Define KZIMP_READ_SPLICE if you want to use the reader_splice version of kzimp (no copy when receiving)
// Note that it does not work with ONE_CHANNEL_PER_LEARNER.
// You also need to define CHANNEL_SIZE

#if defined(KZIMP_READ_SPLICE) && !defined(CHANNEL_SIZE)
#error "KZIMP_READ_SPLICE must come with CHANNEL_SIZE"
#endif

#if defined(KZIMP_READ_SPLICE) && defined(ONE_CHANNEL_PER_LEARNER)
#error "KZIMP_READ_SPLICE with ONE_CHANNEL_PER_LEARNER not implemented"
#endif

#ifdef KZIMP_READ_SPLICE
#define KZIMP_IOCTL_SPLICE_START_READ 0x7
#define KZIMP_IOCTL_SPLICE_FINISH_READ 0x8
#endif

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

#ifdef KZIMP_READ_SPLICE
static char *messages; // mmapped area
static size_t msg_area_len; // size of the mmapped area
#endif

// open wrapper which handles the errors
int Open(const char* pathname, int flags)
{
  int r = open(pathname, flags);
  if (r == -1)
  {
    perror(">>> Error while opening channel\n");
    printf(
        "Node %i experiences an error at %i when opening file %s with flags %x\n",
        node_id, __LINE__, pathname, flags);
    exit(-1);
  }
  return r;
}

// write wrapper which handles the errors
ssize_t Write(int fd, const void *buf, size_t count)
{
  int r = write(fd, buf, count);
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

#ifdef KZIMP_READ_SPLICE
  msg_area_len = (size_t)MESSAGE_MAX_SIZE * (size_t)CHANNEL_SIZE;
#endif
}

#ifdef KZIMP_READ_SPLICE
static void mmap_messages(int fd)
{
  messages = (char*)mmap(NULL, msg_area_len, PROT_READ, MAP_SHARED, fd, 0);
  if (messages == (void*) -1)
  {
    perror("mmap");
    exit(-1);
  }
}

static void munmap_messages(int fd)
{
  munmap(messages, msg_area_len);
}
#endif

// Initialize resources for the node
void IPC_initialize_node(int _node_id)
{
  char chaname[256];

  node_id = _node_id;

  if (node_id == 0)
  {
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 0);
    multicast_0_to_all = Open(chaname, O_WRONLY);

#ifdef ONE_CHANNEL_PER_NODE
    int i;
    for (i = 1; i < nb_nodes; i++)
    {
      snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, i);
      nodei_to_0[i] = Open(chaname, O_RDONLY);
    }
#else
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 1);
    nodes_to_0 = Open(chaname, O_RDONLY);

#ifdef KZIMP_READ_SPLICE
    mmap_messages(nodes_to_0);
#endif
#endif
  }
  else
  {
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 0);
    multicast_0_to_all = Open(chaname, O_RDONLY);

#ifdef KZIMP_READ_SPLICE
    mmap_messages(multicast_0_to_all);
#endif

#ifdef ONE_CHANNEL_PER_NODE
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, node_id);
    nodei_to_0[node_id] = Open(chaname, O_WRONLY);
#else
    snprintf(chaname, 256, "%s%i", KZIMP_CHAR_DEV_FILE, 1);
    nodes_to_0 = Open(chaname, O_WRONLY);
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
#ifdef KZIMP_READ_SPLICE
  if (node_id == 0)
  {
    munmap_messages(nodes_to_0);
  }
  else
  {
    munmap_messages(multicast_0_to_all);
  }
#endif

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
  Write(multicast_0_to_all, msg, length);
}

// send the message msg of size length to the node 0
void IPC_send_unicast(void *msg, size_t length, int nid)
{
#ifdef ONE_CHANNEL_PER_NODE
  Write(nodei_to_0[node_id], msg, length);
#else
  Write(nodes_to_0, msg, length);
#endif
}

#ifdef KZIMP_READ_SPLICE

inline int get_fd(void)
{
  if (node_id == 0)
  {
    return nodes_to_0;
  }
  else
  {
    return multicast_0_to_all;
  }
}

// receive a message.
// Return MAX_MESSAGE_SIZE if everything is ok, 0 otherwise.
size_t IPC_receive(char **msg)
{
  int idx = -1;

  while (idx < 0)
  {
    idx = ioctl(get_fd(), KZIMP_IOCTL_SPLICE_START_READ);
  }

  *msg = messages + idx * MESSAGE_MAX_SIZE;

  return MESSAGE_MAX_SIZE;
}

// finalize the reception of a message
void IPC_receive_finalize(void)
{
  ioctl(get_fd(), KZIMP_IOCTL_SPLICE_FINISH_READ);
}

#else

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
            return Read(nodei_to_0[i], msg, length);
          }
        }
      }
    }

    recv_size = 0;
#else
    recv_size = Read(nodes_to_0, msg, length);
#endif
  }
  else
  {
    recv_size = Read(multicast_0_to_all, msg, length);
  }

  return (size_t) recv_size;
}

#endif
