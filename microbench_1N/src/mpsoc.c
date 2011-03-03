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
#include <semaphore.h>

#include "mpsoc.h"
#include "atomic.h"

#define CACHE_LINE_SIZE 64

// Effects: Increases sz to the least multiple of ALIGNMENT greater
// than size.
#define ALIGNED_SIZE(sz) ((sz)-(sz)%CACHE_LINE_SIZE+CACHE_LINE_SIZE)

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

/* what is a message */
struct mpsoc_message
{
  int bitmap; // the bitmap is alone on a cache line in order for the writer to poll on it
  char __p1[CACHE_LINE_SIZE - sizeof(int)];

  size_t len;
  char buf[MESSAGE_MAX_SIZE];
  char __p2[ALIGNED_SIZE(sizeof(size_t) + MESSAGE_MAX_SIZE)];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

struct mpsoc_reader_toread
{
  int nb_msg_in_transit;
  char __p1[CACHE_LINE_SIZE - sizeof(int)];

  int current_pos;
  char __p2[CACHE_LINE_SIZE - sizeof(int)];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

/* number of messages */
static int nb_msg;

/* number of replicas */
static int nb_replicas;

/* special netmask for multicast */
static unsigned int multicast_bitmap_mask;

/* pointer to the first message */
static struct mpsoc_message* messages;

/* writer's next_write */
static int *next_write;

/* writer's lock */
static lock_t* writer_lock;

/* for the readers to know they have something to read */
static struct mpsoc_reader_toread *reader_toread;

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
int mpsoc_init(char* pathname, int num_replicas, int m, unsigned int mmask)
{
  int i, size;
  //int j;

  nb_msg = m;
  nb_replicas = num_replicas;

  /*********************************/
  /* init shared area for messages */
  /*********************************/
  size = nb_msg * sizeof(struct mpsoc_message);

  messages = (struct mpsoc_message*) mpsoc_init_shm(pathname, size, 'a');
  if (!messages)
  {
    printf("Error while allocating shared memory for message\n");
    return -1;
  }

  for (i = 0; i < nb_msg; i++)
  {
    messages[i].bitmap = 0;
    messages[i].len = 0;
  }

  printf("Init shared area for messages. Address=%p, len=%i\n", messages, size);

  // a special mask to apply for multicast
  multicast_bitmap_mask = mmask;

  /*******************************/
  /* init shared area for writer */
  /*******************************/
  size = 2 * CACHE_LINE_SIZE;
  next_write = (int*) mpsoc_init_shm(pathname, size, 'b');
  if (!next_write)
  {
    printf("Error while allocating shared memory for writer\n");
    return -1;
  }
  writer_lock = (lock_t*) (next_write + CACHE_LINE_SIZE);

  *next_write = 0;
  spinlock_unlock(writer_lock);

  printf("Init shared area for writer. Address=%p, len=%i\n", next_write, size);

  /**************************************/
  /* init shared area for reader_toread */
  /**************************************/
  size = sizeof(struct mpsoc_reader_toread) * nb_replicas;
  reader_toread = (struct mpsoc_reader_toread*) mpsoc_init_shm(pathname, size,
      'd');

  if (!reader_toread)
  {
    printf("Error while allocating shared memory for reader_toread\n");
    return -1;
  }

  for (i = 0; i < nb_replicas; i++)
  {
    reader_toread[i].current_pos = 0;
    reader_toread[i].nb_msg_in_transit = 0;
  }

  printf("Init shared area for reader_toread. Addresses=%p, len=%i\n",
      reader_toread, size);

  return 0;
}

/* Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(size_t len, int *nw)
{
  // this lock is mandatory, otherwise we cannot apply the modulo operator in an atomic way
  spinlock_lock(writer_lock);
  *nw = *next_write;
  *next_write = (*next_write + 1) % nb_msg;
  spinlock_unlock(writer_lock);

  while (messages[*nw].bitmap != 0)
  {
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

  //add message
  messages[*nw].len = min(len, MESSAGE_MAX_SIZE);

  return (void*) (messages[*nw].buf);
}

/* Send len bytes of buf to a (or more) replicas.
 * nw is the position of the message in the ring buffer
 * If dest = -1 then send to all replicas, else send to replica dest
 * Returns the number sent, or -1 for errors.
 */
ssize_t mpsoc_sendto(const void *buf, size_t len, int nw, int dest)
{
  int i;
  int ret = -1;

  //update bitmap & add message to reader_array_indexes
  if (dest == -1)
  {
    messages[nw].bitmap = multicast_bitmap_mask;
  }
  else
  {
    messages[nw].bitmap = (1 << dest);
  }

  for (i = 0; i < nb_replicas; i++)
  {
    __sync_fetch_and_add(&(reader_toread[i].nb_msg_in_transit), 1);
  }

  return ret;
}

#ifdef ZERO_COPY

/*
 * Read len bytes into *buf.
 * Give the id of the caller (from 0 to nb_readers-1) as an argument
 * Returns the number of bytes read or -1 for errors
 * pos will contain the position of the message in the circular buffer.
 */
ssize_t mpsoc_recvfrom(void **buf, size_t len, int *pos, int core_id)
{
  int ret;
  struct mpsoc_reader_index *readx;

  ret = -1;

  readx = &reader_indexes[core_id];

  *pos = readx->array[readx->raf];

  if (*pos >= 0 && *pos < nb_msg)
  {
    readx->array[readx->raf] = -1;
    readx->raf = (readx->raf + 1) % nb_msg;

    // we do not modify the bitmap yet. We only get its value
    if (messages[*pos].bitmap & (1 << core_id))
    {
      ret = min(messages[*pos].len, len);
      *buf = messages[*pos].buf;
    }
  }

  return ret;
}

/*
 * say that reader core_id has read the message at position pos
 */
void mpsoc_free(int pos, int core_id)
{
  if (pos >= 0 && pos < nb_msg)
  {
    __sync_fetch_and_and(&(messages[pos].bitmap), ~(1 << core_id));
  }
}

#else

/*
 * Copy len bytes into buf.
 * Give the id of the caller (from 0 to nb_readers-1) as an argument
 * Returns the number of bytes read
 * blocking call
 */
ssize_t mpsoc_recvfrom(void *buf, size_t len, int core_id)
{
  int ret;

  ret = -1;

  while (1)
  {
    if (reader_toread[core_id].nb_msg_in_transit > 0)
    {
      int pos = reader_toread[core_id].current_pos;

      // we do not modify the bitmap yet. We only get its value
      if (messages[pos].bitmap & (1 << core_id))
      {
        ret = min(messages[pos].len, len);
        memcpy(buf, messages[pos].buf, ret);
        __sync_fetch_and_and(&(messages[pos].bitmap), ~(1 << core_id));

        return ret;
      }

      reader_toread[core_id].current_pos = (pos + 1) % nb_msg;
      __sync_fetch_and_sub(&(reader_toread[core_id].nb_msg_in_transit), 1);
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

#endif

// destroys the shared area
void mpsoc_destroy(void)
{
  shmdt(reader_toread);
  shmdt(next_write);
  shmdt(messages);
}
