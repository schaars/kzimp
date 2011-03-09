#ifndef _MPSOC_H
#define _MPSOC_H

#include "atomic.h"

#define CACHE_LINE_SIZE 64

// Effects: Increases sz to the least multiple of ALIGNMENT greater
// than size.
#define ALIGNED_SIZE(sz) ((sz)-(sz)%CACHE_LINE_SIZE+CACHE_LINE_SIZE)

// what is a message
#define MPSOC_MESSAGE_SIZE ( sizeof(int) + CACHE_LINE_SIZE - sizeof(int) \
    + sizeof(size_t) + MESSAGE_MAX_SIZE)

struct mpsoc_message
{
  int bitmap; // the bitmap is alone on a cache line in order for the writer to poll on it
  char __p1[CACHE_LINE_SIZE - sizeof(int)];

  size_t len;
  char buf[MESSAGE_MAX_SIZE];

  char __p2[CACHE_LINE_SIZE - MPSOC_MESSAGE_SIZE % CACHE_LINE_SIZE
      + CACHE_LINE_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

// what is a reader index
struct mpsoc_reader_index
{
  int next_read; // the next position to read in the shared buffer
  char __p1[CACHE_LINE_SIZE - sizeof(int)];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

// control structure of mpsoc, so that we can use multiple instances of it
struct mpsoc_ctrl
{
  /* number of messages */
  int nb_msg;

  /* number of replicas */
  int nb_replicas;

  /* special netmask for multicast */
  unsigned int multicast_bitmap_mask;

  /* pointer to the first message */
  struct mpsoc_message* messages;

  /* writer's next_write */
  int *next_write;

  /* writer's lock */
  lock_t* writer_lock;

  /* reader indexes */
  struct mpsoc_reader_index *reader_indexes;
};

/****
 * Memory Passing Sockets
 */

/* initialize mpsoc. Basically it creates the shared memory zone to handle m messages
 * at most at the same time for n processes. pathname is the name of an
 * existing file, required by ftok. Returns 0 on success, -1 for errors
 * mmask is the mask to apply on multicast messages.
 */
int mpsoc_init(struct mpsoc_ctrl *c, char* pathname, int num_replicas, int m,
    unsigned int mmask);

/*
 * Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(struct mpsoc_ctrl *c, size_t len, int *nw);

/* Send len bytes of buf to a (or more) replicas.
 * nw is the position of the message in the ring buffer
 * If dest = -1 then send to all replicas, else send to replica dest
 * Returns the number sent, or -1 for errors.
 */
ssize_t mpsoc_sendto(struct mpsoc_ctrl *c, const void *buf, size_t len, int nw,
    int dest);

/*
 * Copy len bytes into buf.
 * Give the id of the caller (from 0 to nb_readers-1) as an argument
 * Returns the number of bytes read
 * blocking call
 */
ssize_t
    mpsoc_recvfrom(struct mpsoc_ctrl *c, void *buf, size_t len, int core_id);

// destroys the shared area
void mpsoc_destroy(struct mpsoc_ctrl *c);

#endif
