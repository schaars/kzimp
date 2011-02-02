/* test of POSIX semaphores
 *
 * Link with -lrt
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

#define NB_ELEMENTS 10

static int core_id;

static sem_t *my_semaphore;

void print_help_and_exit(char *program_name)
{
  fprintf(stderr,
      "Usage: %s -r nb_receivers -n nb_messages -s messages_size_in_B\n",
      program_name);
  exit(-1);
}

/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
void* init_shared_memory_segment(char *p, size_t s, int i)
{
  key_t key;
  int shmid;
  void* ret;

  ret = NULL;

  key = ftok(p, i);
  if (key == -1)
  {
    perror("ftok error: ");
    return ret;
  }

  printf("Size of the shared memory segment to create: %d\n", (int) s);

  shmid = shmget(key, s, IPC_CREAT | 0666);
  if (shmid == -1)
  {
    int errsv = errno;

    perror("shmget error: ");

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

  ret = shmat(shmid, NULL, 0);

  return ret;
}

int main(int argc, char **argv)
{
  int i;

  // create shared memory area
  my_semaphore = (sem_t*) init_shared_memory_segment(argv[0], sizeof(sem_t),
      'a');

  // create semaphore
  sem_init(my_semaphore, 1, NB_ELEMENTS);

  // sem_init(3)

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
  CPU_SET(core_id, &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
    printf("[core %i] Error while calling sched_setaffinity()\n", core_id);
    perror("");
  }

  // if I am the father, then do_producer()
  // else, do_consumer()
  if (core_id == 0)
  {
    uint64_t start, stop;

    for (i = 0; i < NB_ELEMENTS * 2; i++)
    {
      start = get_current_time();
      sem_wait(my_semaphore);
      stop = get_current_time();

      printf("Element %i ... OK after %lu usec\n", i, (unsigned long) (stop
          - start));
    }

    // close shared semaphore
    sem_destroy(my_semaphore);

    int status;
    wait(&status);
  }
  else
  {
    sleep(5);

    for (i = 0; i < NB_ELEMENTS * 2; i++)
    {
      sem_post(my_semaphore);

      printf("Reading element %i\n", i);
      sleep(1);
    }
  }

  // close shared memory
  shmdt(my_semaphore);

  return 0;
}
