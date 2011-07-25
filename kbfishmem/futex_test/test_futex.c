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


/**************************************************************/
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <linux/futex.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
/**************************************************************/


#define NB_THREADS_PER_CORE 2

#define SHM_SIZE 4096
#define NB_MSG 100

int core_id;

// pointer to the shared area
char *shm_addr;


// pointer to the message_present int
int *message_present;

// pointer to the message
int *message;

// the (f/m)utex
typedef int mutex;
mutex *shm_mutex;


/**************************************************************/

/* Code comes from:
 *    -Linux kernel for xchg and cmpxchg code: linux/arch/x86/include/asm/cmpxchg_64.h
 *    -Lockless for the use of futex/mutex: http://locklessinc.com/articles/mutex_cv_futex/
 */

#define MUTEX_INITIALIZER {0}

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

int mutex_init(mutex *m)
{
	*m = 0;
	return 0;
}

int mutex_destroy(mutex *m)
{
	/* Do nothing */
	return 0;
}

/* From the Linux kernel: linux/arch/x86/include/asm/cmpxchg_64.h */
#define __xchg(x, ptr, size)						\
({									\
	typeof(*(ptr)) __x = (x);					\
	switch (size) {							\
	case 1:								\
	{								\
		volatile unsigned char *__ptr = (volatile unsigned char *)(ptr);		\
		asm volatile("xchgb %0,%1"				\
			     : "=q" (__x), "+m" (*__ptr)		\
			     : "0" (__x)				\
			     : "memory");				\
		break;							\
	}								\
	case 2:								\
	{								\
		volatile unsigned short *__ptr = (volatile unsigned short *)(ptr);		\
		asm volatile("xchgw %0,%1"				\
			     : "=r" (__x), "+m" (*__ptr)		\
			     : "0" (__x)				\
			     : "memory");				\
		break;							\
	}								\
	case 4:								\
	{								\
		volatile unsigned int *__ptr = (volatile unsigned int *)(ptr);		\
		asm volatile("xchgl %0,%1"				\
			     : "=r" (__x), "+m" (*__ptr)		\
			     : "0" (__x)				\
			     : "memory");				\
		break;							\
	}								\
	case 8:								\
	{								\
		volatile unsigned long *__ptr = (volatile unsigned long *)(ptr);		\
		asm volatile("xchgq %0,%1"				\
			     : "=r" (__x), "+m" (*__ptr)		\
			     : "0" (__x)				\
			     : "memory");				\
		break;							\
	}								\
	default:							\
      break; \
	}								\
	__x;								\
})

#define xchg(ptr, v)							\
	__xchg((v), (ptr), sizeof(*ptr))

#define LOCK_PREFIX_HERE \
                ".section .smp_locks,\"a\"\n"   \
                ".balign 4\n"                   \
                ".long 671f - .\n" /* offset */ \
                ".previous\n"                   \
                "671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "


/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __raw_cmpxchg(ptr, old, new, size, lock)			\
({									\
	__typeof__(*(ptr)) __ret;					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	switch (size) {							\
	case 1:								\
	{								\
		volatile unsigned char *__ptr = (volatile unsigned char *)(ptr);		\
		asm volatile(lock "cmpxchgb %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "q" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case 2:								\
	{								\
		volatile unsigned short *__ptr = (volatile unsigned short *)(ptr);		\
		asm volatile(lock "cmpxchgw %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case 4:								\
	{								\
		volatile unsigned int *__ptr = (volatile unsigned int *)(ptr);		\
		asm volatile(lock "cmpxchgl %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case 8:								\
	{								\
		volatile unsigned long *__ptr = (volatile unsigned long *)(ptr);		\
		asm volatile(lock "cmpxchgq %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	default:							\
      break;   \
	}								\
	__ret;								\
})

#define __cmpxchg(ptr, old, new, size)					\
	__raw_cmpxchg((ptr), (old), (new), (size), LOCK_PREFIX)

#define cmpxchg(ptr, old, new)						\
	__cmpxchg((ptr), (old), (new), sizeof(*ptr))


int mutex_lock(mutex *m)
{
	int c;

   c = cmpxchg(m, 0, 1);
   if (!c) return 0;

   /* The lock is now contended */
   if (c == 1) c = xchg(m, 2);

   while (c)
   {
      /* Wait in the kernel */
      //printf("[core %i] Going to sleep\n", core_id);
      sys_futex(m, FUTEX_WAIT, 2, NULL, NULL, 0);
      c = xchg(m, 2);
   }

   return 0;
}

int mutex_unlock(mutex *m)
{
	/* Unlock, and if not contended then exit. */
   if (*m == 2)
   {
      *m = 0;
   }
   else if (xchg(m, 0) == 1) return 0;

   if (*m)
   {
      /* Need to set to state 2 because there may be waiters */
      if (cmpxchg(m, 1, 2)) return 0;
   }

   /* We need to wake someone up */
   //printf("[core %i] Waking up someone\n", core_id);
   sys_futex(m, FUTEX_WAKE, 1, NULL, NULL, 0);

   return 0;
}

int mutex_trylock(mutex *m)
{
	/* Try to take the lock, if is currently unlocked */
	unsigned c = cmpxchg(m, 0, 1);
	if (!c) return 0;
	return EBUSY;
}

/**************************************************************/


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

void init_shared_segment(void) {
   //TODO: initialize the shared segment at address shm_addr
   // for the mutex and the read shared region
   message_present = (int*)shm_addr;
   message = (int*)(message_present + 1);
   shm_mutex = (mutex*)(message+1);

   printf("shm_addr=%p, message_present=%p, message=%p, mutex=%p\n", shm_addr, message_present, message, shm_mutex);

   *message_present = 0;
   mutex_init(shm_mutex);
}

void do_reader(void) {
   int i, j;
   int v;
   int perc;

   perc=0;
   i=0;
   while (i < NB_MSG) {
#ifdef FUTEX
      while (*message_present == 0) {
         mutex_lock(shm_mutex);
      }
#endif

      if (*message_present == 1) {
         v = *message;
         *message_present = 0;

         //printf("[core %i] Has read %i\n", core_id, v);

         if (i%(NB_MSG/10) == 0) {
            perc += 10;
            printf("Progress: %i%%\t", perc);
            for (j=0; j<perc/10; j++) {
               printf("+");
            }
            printf("\n");
         }

         i++;
      }
   }
}

void do_writer(void) {
   int i;

   i=0;
   while (i < NB_MSG) {
      if (*message_present == 0) {
         *message = i;
         *message_present = 1;

#ifdef FUTEX
         mutex_unlock(shm_mutex);
#endif

         //printf("[core %i] Has written %i\n", core_id, i);

         i++;
         usleep(500000);
      }
   }
}


int main(int argc, char **argv) {
   //create shared segment
   shm_addr = init_shm(argv[0], SHM_SIZE, 'a');

   init_shared_segment();

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
   if (core_id == 0) {
      do_reader();
   } else {
      do_writer();
   }

   return 0;
}
