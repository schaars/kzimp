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

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

/* what is a reader/writer lock */
typedef struct rwlock_t
{
  lock_t m1;
  lock_t m2;
  lock_t m3;
} rwlock_t;

/* what is a message */
#define MPSOC_MESSAGE_SIZE (sizeof(int)+sizeof(size_t)+sizeof(rwlock_t)+MESSAGE_MAX_SIZE)
struct mpsoc_message
{
  int bitmap;
  size_t len;
  rwlock_t lock;
  char buf[MESSAGE_MAX_SIZE];
  char __p[CACHE_LINE_SIZE + (MPSOC_MESSAGE_SIZE / CACHE_LINE_SIZE) * CACHE_LINE_SIZE]; // for padding
} __attribute__((__packed__,  __aligned__(CACHE_LINE_SIZE)));

/* what is a reader index */
struct mpsoc_reader_index
{
  int raf; // read_at_first
  int ral; // read at last
  lock_t lock;
  int *array;
};

/* structure which contains an int
 * and a semaphore for blocking operations */
struct mpsoc_block_ops
{
  volatile int value;
  sem_t semaphore;
};

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

/* nb of readers for each message; for reader/writer lock */
static int *readcount;

/* blocking operations */
static struct mpsoc_block_ops *block_ops_send;

/* reader/writer lock, writer locks.
 * pos is the message to lock
 */
void mpsoc_rw_writerlock(int pos)
{
  rwlock_t *lock;

  lock = &(messages[pos].lock);

  spinlock_lock(&(lock->m2));
  spinlock_lock(&(lock->m3));
}

/* reader/writer lock, writer unlocks.
 * pos is the message to unlock
 */
void mpsoc_rw_writerunlock(int pos)
{
  rwlock_t *lock;

  lock = &(messages[pos].lock);

  spinlock_unlock(&(lock->m3));
  spinlock_unlock(&(lock->m2));
}

/* reader/writer lock, reader locks.
 * pos is the message to lock
 */
void mpsoc_rw_readerlock(int pos)
{
  rwlock_t *lock;

  lock = &(messages[pos].lock);

  spinlock_lock(&(lock->m1));

  readcount[pos] = readcount[pos] + 1;
  if (readcount[pos] == 1)
    spinlock_lock(&(lock->m3));

  spinlock_unlock(&(lock->m1));
}

/* reader/writer lock, reader unlocks.
 * pos is the message to unlock
 */
void mpsoc_rw_readerunlock(int pos)
{
  rwlock_t *lock;

  lock = &(messages[pos].lock);

  spinlock_lock(&(lock->m1));

  readcount[pos] = readcount[pos] - 1;
  if (readcount[pos] == 0)
    spinlock_unlock(&(lock->m3));

  spinlock_unlock(&(lock->m1));
}

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
 */
int mpsoc_init(char* pathname, int num_replicas, int m)
{
  int i, j, size;

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

  rwlock_t *lock;
  for (i = 0; i < nb_msg; i++)
  {
    messages[i].bitmap = 0;
    messages[i].len = 0;

    lock = &(messages[i].lock);
    spinlock_unlock(&(lock->m1));
    spinlock_unlock(&(lock->m2));
    spinlock_unlock(&(lock->m3));
  }

  printf("Init shared area for messages. Address=%p, len=%i\n", messages, size);

  // a special mask to apply for multicast
  multicast_bitmap_mask = 0;
  for (i = 0; i < num_replicas; i++)
  {
    multicast_bitmap_mask = multicast_bitmap_mask | (1 << i);
  }

  /*******************************/
  /* init shared area for writer */
  /*******************************/
  size = sizeof(int) + sizeof(lock_t);
  next_write = (int*) mpsoc_init_shm(pathname, size, 'b');
  if (!next_write)
  {
    printf("Error while allocating shared memory for writer\n");
    return -1;
  }
  writer_lock = (lock_t*) (next_write + 1);

  *next_write = 0;
  spinlock_unlock(writer_lock);

  printf("Init shared area for writer. Address=%p, len=%i\n", next_write, size);

  /*****************************************/
  /* init shared area for read count */
  /*****************************************/
  size = sizeof(int) * nb_msg;
  readcount = (int*) mpsoc_init_shm(pathname, size, 'c');

  if (!readcount)
  {
    printf("Error while allocating shared memory for read count\n");
    return -1;
  }

  for (i = 0; i < nb_msg; i++)
  {
    readcount[i] = 0;
  }

  printf("Init shared area for read count. Addresses=%p, len=%i\n", readcount,
      size);

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

  int size2 = sizeof(int) * nb_msg;
  for (i = 0; i < nb_replicas; i++)
  {
    reader_indexes[i].raf = 0;
    reader_indexes[i].ral = -1;
    spinlock_unlock(&(reader_indexes[i].lock));

    reader_indexes[i].array = (int*) mpsoc_init_shm(pathname, size2, 'e' + i);
    if (!reader_indexes[i].array)
    {
      printf(
          "Error while allocating shared memory for reader_indexes[%i].array\n",
          i);
      return -1;
    }

    for (j = 0; j < nb_msg; j++)
    {
      reader_indexes[i].array[j] = -1;
    }

    printf(
        "Init shared area for reader_indexes[%i].array. Address=%p, len=%i\n",
        i, reader_indexes[i].array, size2);
  }

  printf("Init shared area for reader_indexes. Addresses=%p, len=%i\n",
      reader_indexes, size);

  block_ops_send = (struct mpsoc_block_ops*) mpsoc_init_shm(pathname,
      sizeof(struct mpsoc_block_ops), 'f');
  if (!block_ops_send)
  {
    printf("Error while allocating shared memory for block_ops_recv\n");
    return -1;
  }

  block_ops_send->value = 0;
  if (sem_init(&(block_ops_send->semaphore), 1, 0) == -1)
  {
    perror("Semaphore initialization failed (1)");
    exit(-1);
  }

  return 0;
}

/* Allocate a message of size len in shared mem.
 * Return the address of the message and (in nw variable)
 * the position of the message in the ring buffer
 */
void* mpsoc_alloc(size_t len, int *nw)
{
  while (__sync_fetch_and_add(&(block_ops_send->value), 0) == nb_msg)
  {
    sem_wait(&(block_ops_send->semaphore));
  }

  spinlock_lock(writer_lock);
  *nw = *next_write;
  *next_write = (*next_write + 1) % nb_msg;
  spinlock_unlock(writer_lock);

  mpsoc_rw_writerlock(*nw);

  //add message
  messages[*nw].bitmap = 0;
  messages[*nw].len = min(len, MESSAGE_MAX_SIZE);

  __sync_fetch_and_add(&(block_ops_send->value), 1);

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
  int ret = -1;

  //update bitmap & add message to reader_array_indexes
  if (dest == -1)
  {
    messages[nw].bitmap = multicast_bitmap_mask;

    for (i = 0; i < nb_replicas; i++)
    {
      spinlock_lock(&(reader_indexes[i].lock));

      readx = &reader_indexes[i];
      readx->ral = (readx->ral + 1) % nb_msg;
      readx->array[readx->ral] = nw;

      spinlock_unlock(&(reader_indexes[i].lock));
    }
  }
  else
  {
    messages[nw].bitmap = (1 << dest);

    spinlock_lock(&(reader_indexes[dest].lock));

    readx = &reader_indexes[dest];
    readx->ral = (readx->ral + 1) % nb_msg;
    readx->array[readx->ral] = nw;

    spinlock_unlock(&(reader_indexes[dest].lock));
  }

  mpsoc_rw_writerunlock(nw);

  return ret;
}

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

  // get a *position
  spinlock_lock(&(reader_indexes[core_id].lock));

  *pos = readx->array[readx->raf];

  if (*pos >= 0 && *pos < nb_msg)
  {
    readx->array[readx->raf] = -1;
    readx->raf = (readx->raf + 1) % nb_msg;
  }

  spinlock_unlock(&(reader_indexes[core_id].lock));

  if (*pos >= 0 && *pos < nb_msg)
  {
    // get a message at *position *pos
    mpsoc_rw_readerlock(*pos);

    if (messages[*pos].bitmap & (1 << core_id))
    {
      ret = min(messages[*pos].len, len);
      *buf = messages[*pos].buf;
      messages[*pos].bitmap &= ~(1 << core_id);

      if (messages[*pos].bitmap == 0)
      {
        if (__sync_fetch_and_sub(&(block_ops_send->value), 1) == nb_msg)
        {
          sem_post(&(block_ops_send->semaphore));
        }
      }
    }

    //mpsoc_rw_readerunlock(*pos);
  }

  return ret;
}

/*
 * return the lock at position pos in the circular buffer
 */
void mpsoc_free(int pos)
{
  if (pos >= 0 && pos < nb_msg)
  {
    mpsoc_rw_readerunlock(pos);
  }
}

// destroys the shared area
void mpsoc_destroy(void)
{
  int i;

  shmdt(block_ops_send);

  for (i = 0; i < nb_replicas; i++)
  {
    shmdt(reader_indexes[i].array);
  }

  shmdt(reader_indexes);
  shmdt(readcount);
  shmdt(next_write);
  shmdt(messages);
}
