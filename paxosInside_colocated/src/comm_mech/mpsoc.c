/****
 * Memory Passing Sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/mman.h>

#include "mpsoc.h"

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
char* mpsoc_init_shm(char *p, size_t s, int i)
{
  key_t key;
  int shmid;
  char* ret;

  ret = NULL;

  key = ftok(p, i);
  if (key == -1)
  {
    printf("In mpsoc_init_shm with p=%s, s=%i and i=%i\n", p, (int) s, i);
    perror("ftok error: ");
    return ret;
  }

  shmid = shmget(key, s, IPC_CREAT | 0666);
  if (shmid == -1)
  {
    int errsv = errno;

    printf("In mpsoc_init_shm with p=%s, s=%i and i=%i\n", p, (int) s, i);
    perror("shmid error: ");

    switch (errsv)
    {
    case EACCES:
      printf("errno = EACCES\n");
      break;
    case EINVAL:
      printf("errno = EINVAL\n");
      break;
    case ENFILE:
      printf("errno = ENFILE\n");
      break;
    case ENOENT:
      printf("errno = ENOENT\n");
      break;
    case ENOMEM:
      printf("errno = ENOMEM\n");
      break;
    case ENOSPC:
      printf("errno = ENOSPC\n");
      break;
    case EPERM:
      printf("errno = EPERM\n");
      break;
    default:
      printf("errno = %i\n", errsv);
      break;
    }

    return ret;
  }

  ret = (char*) shmat(shmid, NULL, 0);

  return ret;
}

/* initialize mpsoc. Basically it creates the shared memory zone to handle m messages
 * at most at the same time for n processes. pathname is the name of an
 * existing file, required by ftok. Returns 0 on success, -1 for errors
 * mmask is the mask to apply on multicast messages.
 */
int mpsoc_init(struct mpsoc_ctrl *c, char* pathname, int num_replicas, int m,
    unsigned int mmask)
{
  int i, size;

  c->nb_msg = m;
  c->nb_replicas = num_replicas;

  /*********************************/
  /* init shared area for messages */
  /*********************************/
  size = c->nb_msg * sizeof(struct mpsoc_message);

  c->messages = (struct mpsoc_message*) mpsoc_init_shm(pathname, size, 'a');
  if (!c->messages)
  {
    printf("Error while allocating shared memory for message\n");
    return -1;
  }

  for (i = 0; i < c->nb_msg; i++)
  {
    c->messages[i].bitmap = 0;
    c->messages[i].len = 0;
  }

  printf("Init shared area for messages. Address=%p, len=%i\n", c->messages,
      size);

  // a special mask to apply for multicast
  c->multicast_bitmap_mask = mmask;

  /*******************************/
  /* init shared area for writer */
  /*******************************/
  size = 2 * CACHE_LINE_SIZE;
  c->next_write = (int*) mpsoc_init_shm(pathname, size, 'b');
  if (!c->next_write)
  {
    printf("Error while allocating shared memory for writer\n");
    return -1;
  }
  c->writer_lock = (lock_t*) (c->next_write + CACHE_LINE_SIZE);

  *c->next_write = 0;
  spinlock_unlock(c->writer_lock);

  printf("Init shared area for writer. Address=%p, len=%i\n", c->next_write,
      size);

  /***************************************/
  /* init shared area for reader_indexes */
  /***************************************/
  size = sizeof(struct mpsoc_reader_index) * c->nb_replicas;
  c->reader_indexes = (struct mpsoc_reader_index*) mpsoc_init_shm(pathname,
      size, 'd');

  if (!c->reader_indexes)
  {
    printf("Error while allocating shared memory for reader_indexes\n");
    return -1;
  }

  for (i = 0; i < c->nb_replicas; i++)
  {
    c->reader_indexes[i].next_read = 0;
  }

  printf("Init shared area for reader_indexes. Addresses=%p, len=%i\n",
      c->reader_indexes, size);

  return 0;
}

/* Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(struct mpsoc_ctrl *c, size_t len, int *nw)
{
  // the sender does not send messages to himself
  // get the current value of next_write and then update it
  spinlock_lock(c->writer_lock);
  *nw = *c->next_write;
  *c->next_write = (*c->next_write + 1) % c->nb_msg;
  spinlock_unlock(c->writer_lock);

  // wait for the bitmap to become 0
  while (c->messages[*nw].bitmap != 0)
  {
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

  //add message
  c->messages[*nw].len = min(len, MESSAGE_MAX_SIZE);

  return (void*) (c->messages[*nw].buf);
}

/* Send len bytes of buf to a (or more) replicas.
 * nw is the position of the message in the ring buffer
 * If dest = -1 then send to all replicas, else send to replica dest
 * Returns the number sent, or -1 for errors.
 */
ssize_t mpsoc_sendto(struct mpsoc_ctrl *c, const void *buf, size_t len, int nw,
    int dest)
{
  int ret = -1;

  //update bitmap
  if (dest == -1)
  {
    c->messages[nw].bitmap = c->multicast_bitmap_mask;
  }
  else
  {
    c->messages[nw].bitmap = (1 << dest);
  }

  return ret;
}

/*
 * Copy len bytes into buf.
 * Give the id of the caller (from 0 to nb_readers-1) as an argument
 * Returns the number of bytes read
 * blocking call
 */
ssize_t mpsoc_recvfrom(struct mpsoc_ctrl *c, void *buf, size_t len, int core_id)
{
  int ret, pos;

  ret = -1;
  pos = c->reader_indexes[core_id].next_read;

  while (1)
  {
    // we do not modify the bitmap yet. We only get its value
    if (c->messages[pos].bitmap & (1 << core_id))
    {
      ret = min(c->messages[pos].len, len);
      memcpy(buf, c->messages[pos].buf, ret);
      __sync_fetch_and_and(&(c->messages[pos].bitmap), ~(1 << core_id));

      c->reader_indexes[core_id].next_read = (pos + 1) % c->nb_msg;

      return ret;
    }

#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

  return ret;
}

// non-blocking version of recvfrom
// return 0 if there is no message
ssize_t mpsoc_recvfrom_nonblocking(struct mpsoc_ctrl *c, void *buf, size_t len,
    int core_id)
{
  int ret, pos;

  ret = 0;
  pos = c->reader_indexes[core_id].next_read;

  // we do not modify the bitmap yet. We only get its value
  if (c->messages[pos].bitmap & (1 << core_id))
  {
    ret = min(c->messages[pos].len, len);
    memcpy(buf, c->messages[pos].buf, ret);
    __sync_fetch_and_and(&(c->messages[pos].bitmap), ~(1 << core_id));

    c->reader_indexes[core_id].next_read = (pos + 1) % c->nb_msg;
  }

  return ret;
}

// destroys the shared area
void mpsoc_destroy(struct mpsoc_ctrl *c)
{
  shmdt(c->reader_indexes);
  shmdt(c->next_write);
  shmdt(c->messages);
}
