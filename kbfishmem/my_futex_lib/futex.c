/*
 * Small library which shares a futex between multiple processes
 * Code comes from:
 *    -Linux kernel for xchg and cmpxchg code: linux/arch/x86/include/asm/cmpxchg_64.h
 *    -Lockless for the use of futex/futex: http://locklessinc.com/articles/futex_cv_futex/
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>

/* shared memory */
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>

/* futex */
#include <linux/futex.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include "futex.h"

static int sys_futex(void *addr1, int op, int val1, struct timespec *timeout,
    void *addr2, int val3)
{
  return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

/* init a shared area of size s with pathname p and project id i.
 * Return a pointer to it, or NULL in case of errors.
 */
static char* init_shm(char *p, size_t s, int i)
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

// return the new futex or NULL if an error has occured
futex* futex_init(char *p, int i)
{
  futex *f;
  f = (futex*) init_shm(p, sizeof(*f), i);

  if (f)
  {
    *f = 0;
  }

  return f;
}

// destroy the futex and return 0 if everything is ok
int futex_destroy(futex *f)
{
  if (f)
  {
    shmdt(f);
  }

  return 0;
}

int futex_lock(futex *f)
{
  int c;

  c = cmpxchg(f, 0, 1);
  if (!c)
    return 0;

  /* The lock is now contended */
  if (c == 1)
    c = xchg(f, 2);

  while (c)
  {
    /* Wait in the kernel */
    //printf("[core %i] Going to sleep\n", core_id);
    sys_futex(f, FUTEX_WAIT, 2, NULL, NULL, 0);
    c = xchg(f, 2);
  }

  return 0;
}

int futex_unlock(futex *f)
{
  /* Unlock, and if not contended then exit. */
  if (*f == 2)
  {
    *f = 0;
  }
  else if (xchg(f, 0) == 1)
    return 0;

  if (*f)
  {
    /* Need to set to state 2 because there may be waiters */
    if (cmpxchg(f, 1, 2))
      return 0;
  }

  /* We need to wake someone up */
  //printf("[core %i] Waking up someone\n", core_id);
  sys_futex(f, FUTEX_WAKE, 1, NULL, NULL, 0);

  return 0;
}

int futex_trylock(futex *f)
{
  /* Try to take the lock, if is currently unlocked */
  unsigned c = cmpxchg(f, 0, 1);
  if (!c)
    return 0;
  return EBUSY;
}