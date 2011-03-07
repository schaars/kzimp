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

struct mpsoc_reader_index_entry
{
  int v;
  char __p[CACHE_LINE_SIZE - sizeof(int)];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

/* what is a reader index */
#define MPSOC_READER_INDEX_SIZE ( sizeof(int) + CACHE_LINE_SIZE - sizeof(int) \
    + sizeof(int) + CACHE_LINE_SIZE - sizeof(int) \
    + NB_MESSAGES * sizeof(struct mpsoc_reader_index_entry))

struct mpsoc_reader_index
{
  int raf; // read_at_first
  char __p1[CACHE_LINE_SIZE - sizeof(int)];

  int ral; // read at last
  lock_t reader_index_lock;
  char __p2[CACHE_LINE_SIZE - sizeof(int) - sizeof(lock_t)];

  struct mpsoc_reader_index_entry array[NB_MESSAGES];

  char __p[CACHE_LINE_SIZE - MPSOC_READER_INDEX_SIZE % CACHE_LINE_SIZE
      + CACHE_LINE_SIZE];
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

/* reader indexes */
static struct mpsoc_reader_index *reader_indexes;

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
  int j;

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

  /***************************************/
  /* init shared area for reader_indexes */
  /***************************************/
  size = sizeof(struct mpsoc_reader_index) * nb_replicas;
  reader_indexes = (struct mpsoc_reader_index*) mpsoc_init_shm(pathname, size,
      'd');

  if (!reader_indexes)
  {
    printf("Error while allocating shared memory for reader_indexes\n");
    return -1;
  }

  for (i = 0; i < nb_replicas; i++)
  {
    reader_indexes[i].raf = 0;
    reader_indexes[i].ral = -1;
    spinlock_unlock(&reader_indexes[i].reader_index_lock);

    for (j = 0; j < nb_msg; j++)
    {
      reader_indexes[i].array[j].v = -1;
    }
  }

  printf("Init shared area for reader_indexes. Addresses=%p, len=%i\n",
      reader_indexes, size);

  return 0;
}

/* Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(size_t len, int *nw)
{
  while (1)
  {
    // this lock is mandatory, otherwise we cannot apply the modulo operator in an atomic way
    spinlock_lock(writer_lock);
    *nw = *next_write;
    *next_write = (*next_write + 1) % nb_msg;

    if (messages[*nw].bitmap == 0)
    {
      // we modify the bitmap so that another writer cannot get the same position.
      // this is safe: the bitmap is currently 0, meaning that all the readers have read it.
      messages[*nw].bitmap = 0xdeadbeef;
      spinlock_unlock(writer_lock);

      break;
    }

    spinlock_unlock(writer_lock);

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
  struct mpsoc_reader_index *readx;
  int i;
  int new_pos;
  int ret = -1;

  //update bitmap & add message to reader_array_indexes
  if (dest == -1)
  {
    messages[nw].bitmap = multicast_bitmap_mask;

    for (i = 0; i < nb_replicas; i++)
    {
      if (multicast_bitmap_mask & (1 << i))
      {
        // we need a lock on that since multiple writers can modify concurrently this value
        readx = &reader_indexes[i];

        spinlock_lock(&readx->reader_index_lock);
        new_pos = (readx->ral + 1) % nb_msg;
        readx->array[new_pos].v = nw;
        readx->ral = new_pos;
        spinlock_unlock(&readx->reader_index_lock);
      }
    }
  }
  else
  {
    messages[nw].bitmap = (1 << dest);

    // we need a lock on that since multiple writers can modify concurrently this value
    readx = &reader_indexes[dest];

    spinlock_lock(&readx->reader_index_lock);
    new_pos = (readx->ral + 1) % nb_msg;
    readx->array[new_pos].v = nw;
    readx->ral = new_pos;
    spinlock_unlock(&readx->reader_index_lock);
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
  int ret, pos;
  struct mpsoc_reader_index *readx;

  ret = -1;
  readx = &reader_indexes[core_id];

  while (1)
  {
    pos = readx->array[readx->raf].v;

    if (pos >= 0 && pos < nb_msg)
    {
      readx->array[readx->raf].v = -1;
      readx->raf = (readx->raf + 1) % nb_msg;

      // we do not modify the bitmap yet. We only get its value
      if (messages[pos].bitmap & (1 << core_id))
      {
        ret = min(messages[pos].len, len);
        memcpy(buf, messages[pos].buf, ret);
        __sync_fetch_and_and(&(messages[pos].bitmap), ~(1 << core_id));

        return ret;
      }
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
  shmdt(reader_indexes);
  shmdt(next_write);
  shmdt(messages);
}
