/*
 * Shared Memory Message Passing
 * Version:     2010
 * Author:	Pierre Louis Aublin <pierre-louis.aublin@inria.fr>
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef PTHREAD_LOCK
#include <pthread.h>
#endif

#include "smmp.h"


#ifdef PTHREAD_LOCK
#define SMMP_MESSAGE_HEADER_SIZE (sizeof(int)*2 + sizeof(pthread_mutex_t) + sizeof(size_t))
#else
#define SMMP_MESSAGE_HEADER_SIZE (sizeof(int)*3 + sizeof(size_t))
#endif

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)


struct smmp_message {
  int bitmap;
  int nb_readers;
#ifdef PTHREAD_LOCK
  pthread_mutex_t lock;
#else
  volatile int lock;
#endif
  size_t length; // actual length of the buffer
  char buffer[MESSAGE_MAX_SIZE]; // the content
};


/* totality of shared mem*/
static char* smmp_mem_start;
static char* smmp_mem_end;
static size_t smmp_mem_len;

/* shared mem for messages */
static char* smmp_msg_start;
static char* smmp_msg_end;
static size_t smmp_msg_len;

/* max number of messages at the same time */
static int nb_msg;

static int *next_write;
#ifdef PTHREAD_LOCK
static pthread_mutex_t *writer_lock;
#else
static volatile int *writer_lock;
#endif
static struct smmp_message* message;

#ifndef PTHREAD_LOCK
/* wait & lock value at address l */
void lock(volatile int *l) {
  while (*l == 1)
     usleep(1);
  *l = 1;
}

/* unlock value at address l */
void unlock(volatile int *l) {
 *l = 0;
}
#endif


/* initialize the shared memory with max_nb_msg
 * Return -1 if failed
 */
int smmp_init(char *pathname, int max_nb_msg) {
  key_t key;
  int shmid, i, pagesize;
 
  nb_msg = max(2, max_nb_msg);

  /****** init shared mem ******/
  key = ftok(pathname, 'a');
  if (key == -1) {
    perror("ftok error in smmp_init: ");
    return -1;
  }

  pagesize = sysconf(_SC_PAGE_SIZE);

  smmp_msg_len = sizeof(struct smmp_message) * nb_msg;
  smmp_mem_len = sizeof(int) + smmp_msg_len + pagesize;
#ifdef PTHREAD_LOCK
  smmp_mem_len += sizeof(pthread_mutex_t);
#else
  smmp_mem_len += sizeof(int)*2;
#endif

  shmid = shmget(key, smmp_mem_len, IPC_CREAT | 0666);
  if (shmid == -1) {
    perror("shmid error in smmp_init: ");
    
    switch (errno) {
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
      printf("errno = %i\n", errno);
      break;
   }

    return -1;
  }

  smmp_mem_start = (char*)shmat(shmid, NULL, 0);
  if (smmp_mem_start == (char*) -1) {
    perror("shmat error in smmp_init: ");
    return -1;
  }

  smmp_mem_end = smmp_mem_start + smmp_mem_len - 1;
  smmp_msg_start = smmp_mem_start + sizeof(int);
#ifdef PTHREAD_LOCK
  smmp_msg_start += sizeof(pthread_mutex_t);
#else
  smmp_msg_start += sizeof(int);
#endif

  /* smmp_msg_start need to be aligned on pagesize for mprotect */
  //printf("smmp_msg_start = %p, pagesize = %i\n", smmp_msg_start, pagesize);
  while ((int)&(((struct smmp_message*)smmp_msg_start)->buffer) % pagesize != 0)
     smmp_msg_start++;
  //printf("smmp_msg_start = %p\n", smmp_msg_start);


  smmp_msg_end = smmp_msg_start + smmp_msg_len - 1;

  //printf("mem: start=%p, end=%p, len=%i\nmsg: start=%p, end=%p, len=%i\n", smmp_mem_start, smmp_mem_end, smmp_mem_len, smmp_msg_start, smmp_msg_end, smmp_msg_len);

  /****** init structures (lock + bitmap) ******/
  next_write = (int*)smmp_mem_start;
#ifdef PTHREAD_LOCK
  writer_lock = (pthread_mutex_t*)(next_write+1);
#else
  writer_lock = (int*)(next_write+1);
#endif
  message = (struct smmp_message*)smmp_msg_start;

  *next_write = 0;
#ifdef PTHREAD_LOCK
  pthread_mutex_init(writer_lock, NULL);
#else
  unlock(writer_lock);
#endif
  for (i=0; i<nb_msg; i++) {
    message[i].length = 0;
    message[i].bitmap = 0;
    message[i].nb_readers = 0;
#ifdef PTHREAD_LOCK
    pthread_mutex_init(&(message[i].lock), NULL);
#else
	 unlock(&(message[i].lock));
#endif

/*
    int ret = mprotect(&(message[*next_write].buffer), MESSAGE_MAX_SIZE, PROT_READ);
    if (ret == -1) {
       switch (errno) {
          case EACCES:
             printf("Cannot give the specified access\n");
             break;
          case EFAULT:
             printf("Cannot access memory\n");
             break;
          case EINVAL:
             printf("Address not valid (or not aligned)\n");
             break;
          case ENOMEM:
             printf("Error problem. See manpage\n");
             break;
       }
    } 
*/
  }

  return 0;
}


/* send buf of size len to dest through shared memory.
 * If dest = -1 then send to all cores.
 * Return -1 if failed, the written length otherwise.
 */
int smmp_send(void *buf, int len, int dest) {
  int ret = -1;

  if ( !(dest == -1 || (dest >=0 && dest <= nb_cores)) )
    return ret;

#ifdef PTHREAD_LOCK
  pthread_mutex_lock(writer_lock);
#else
  lock(writer_lock);
#endif

  while (ret == -1) {
#ifdef PTHREAD_LOCK
    pthread_mutex_lock(&(message[*next_write].lock));
#else
	 lock(&(message[*next_write].lock));
#endif

    if (message[*next_write].nb_readers == 0) {

      /********************* writing *************/
      ret = min(len, MESSAGE_MAX_SIZE);
      message[*next_write].length = ret;

      //mprotect(&(message[*next_write].buffer), ret, PROT_WRITE);
      memcpy(&(message[*next_write].buffer), buf, ret);
      //mprotect(&(message[*next_write].buffer), ret, PROT_READ);
      
      if (dest == -1)
	message[*next_write].bitmap = ~0;
      else {
	message[*next_write].bitmap = (1 << dest);
      }
      /******************* end writing *****************/

    }

#ifdef PTHREAD_LOCK
    pthread_mutex_unlock(&(message[*next_write].lock));
#else
	 unlock(&(message[*next_write].lock));
#endif
    *next_write = (*next_write +1) % nb_msg;
  }

#ifdef PTHREAD_LOCK
  pthread_mutex_unlock(writer_lock);
#else
  unlock(writer_lock);
#endif

  return ret;
}


/* receive a message in buf of size len from shared memory,
 * with a memcpy. Return -1 if failed.
 */
int smmp_recv_memcpy(void *buf, int len) {
  int i;
  int ret = -1;

  for (i=0; i<nb_msg; i++) {
#ifdef PTHREAD_LOCK
    pthread_mutex_lock(&(message[i].lock));
#else
	 lock(&(message[i].lock));
#endif

    /********************* reading ***************/
    if (message[i].bitmap & (1 << core_id)) {
      ret = min(len, message[i].length);
      memcpy(buf, &(message[i].buffer), ret);
      message[i].bitmap &= ~(1 << core_id);
    }
    /******************* end reading *************/

#ifdef PTHREAD_LOCK
    pthread_mutex_unlock(&(message[i].lock));
#else
    unlock(&(message[i].lock));
#endif

    if (ret != -1)
      return ret;
  }

  return ret;
}


/* receive a message in buf of size *len from shared memory.
 * Return -1 if failed.
 */
int smmp_recv(void **buf, int *len) {
  int i;
  int ret = -1;

  for (i=0; i<nb_msg; i++) {
#ifdef PTHREAD_LOCK
    pthread_mutex_lock(&(message[i].lock));
#else
	 lock(&(message[i].lock));
#endif

    /********************* reading ***************/
    if (message[i].bitmap & (1 << core_id)) {
      *len = ret = message[i].length;
      *buf = (void*)(message[i].buffer);
      //buf[0] = 0; // should send a sigsev
      message[i].nb_readers++;
    }
    /******************* end reading *************/

#ifdef PTHREAD_LOCK
    pthread_mutex_unlock(&(message[i].lock));
#else
	 unlock(&(message[i].lock));
#endif
	 
    if (ret != -1)
      return ret;
  }

  return ret;
}


/* Say that we read the message at address buf of size len,
 * updating the  bitmap fore core core_id.
 * Return -1 if problem
 */
int smmp_message_is_read(void *buf, int len) {
  struct smmp_message *message; 

  // find structure
  message = (struct smmp_message*)(buf - SMMP_MESSAGE_HEADER_SIZE);
  if (message->length != len) {
    printf("Error in smmp_message_is_read: address %p is not valid\n", buf);
    return -1;
  }

#ifdef PTHREAD_LOCK
  pthread_mutex_lock(&(message->lock));
#else
  lock(&(message->lock));
#endif

  // update bitmap
  message->bitmap &= ~(1 << core_id);
  message->nb_readers--;

#ifdef PTHREAD_LOCK
  pthread_mutex_unlock(&(message->lock));
#else
  unlock(&(message->lock));
#endif

  return 0;
}
