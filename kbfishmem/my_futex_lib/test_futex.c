/* A test of futexes */

#define _GNU_SOURCE

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
#include <sched.h>

#ifdef FUTEX
#include "futex.h"
#endif

#define NB_THREADS_PER_CORE 2

#define SHM_SIZE (sizeof(int)*2)
#define NB_MSG 100

int core_id;

// pointer to the shared area
char *shm_addr;

// pointer to the message_present int
int *message_present;

// pointer to the message
int *message;

#ifdef FUTEX
futex *shm_mutex;
#endif

/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
char* init_shm(char *p, size_t s, int i)
{
  key_t key;
  int shmid;
  char* ret;

  ret = NULL;

  key = ftok(p, i);
  if (key == -1)
  {
    printf("In init_shm with p=%s, s=%i and i=%i\n", p, (int) s, i);
    perror("ftok error: ");
    return ret;
  }

  shmid = shmget(key, s, IPC_CREAT | 0666);
  if (shmid == -1)
  {
    int errsv = errno;

    printf("In init_shm with p=%s, s=%i and i=%i\n", p, (int) s, i);
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

void init_shared_segment(char *s, int i)
{
  message_present = (int*) shm_addr;
  message = (int*) (message_present + 1);

#ifdef FUTEX
  shm_mutex = futex_init(s, i);
#endif

#ifdef FUTEX
  printf("shm_addr=%p, message_present=%p, message=%p, futex=%p\n", shm_addr,
      message_present, message, shm_mutex);
#else
  printf("shm_addr=%p, message_present=%p, message=%p\n", shm_addr,
      message_present, message);
#endif

  *message_present = 0;
}

void do_reader(void)
{
  int i, j;
  int v;
  int perc;

  perc = 0;
  i = 0;
  while (i < NB_MSG)
  {
#ifdef FUTEX
    while (*message_present == 0)
    {
      futex_lock(shm_mutex);
    }
#endif

    if (*message_present == 1)
    {
      v = *message;
      *message_present = 0;

      //printf("[core %i] Has read %i\n", core_id, v);

      if (i % (NB_MSG / 10) == 0)
      {
        perc += 10;
        printf("Progress: %i%%\t", perc);
        for (j = 0; j < perc / 10; j++)
        {
          printf("+");
        }
        printf("\n");
      }

      i++;
    }
  }
}

void do_writer(void)
{
  int i;

  i = 0;
  while (i < NB_MSG)
  {
    if (*message_present == 0)
    {
      *message = i;
      *message_present = 1;

#ifdef FUTEX
      futex_unlock(shm_mutex);
#endif

      //printf("[core %i] Has written %i\n", core_id, i);

      i++;
      usleep(100000);
    }
  }
}

int main(int argc, char **argv)
{
  //create shared segment
  shm_addr = init_shm(argv[0], SHM_SIZE, 'a');

  init_shared_segment(argv[0], 'b');

  //fork
  fflush(NULL);
  sync();

  // fork in order to create the children
  core_id = 0;
  if (!fork())
  {
    core_id = 1;
  }

  // set affinity to 1 core
  cpu_set_t mask;

  CPU_ZERO(&mask);
  CPU_SET(core_id * NB_THREADS_PER_CORE, &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
    printf("[core %i] Error while calling sched_setaffinity()\n", core_id);
    perror("");
  }

  printf("[core %i] Is ready\n", core_id);

  //1 process reads, the other writes
  if (core_id == 0)
  {
    do_reader();
  }
  else
  {
    do_writer();
  }

#ifdef FUTEX
  futex_destroy(shm_mutex);
#endif

  return 0;
}
