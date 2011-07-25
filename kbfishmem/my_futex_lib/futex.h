/*
 * Small library which shares a futex between multiple processes
 * Code comes from:
 *    -Linux kernel for xchg and cmpxchg code: linux/arch/x86/include/asm/cmpxchg_64.h
 *    -Lockless for the use of futex/futex: http://locklessinc.com/articles/futex_cv_futex/
 */

#ifndef _MY_FUTEX_LIB_
#define _MY_FUTEX_LIB_

typedef int futex;

/********************** Exported interface **********************/

// return the new futex or NULL if an error has occured
futex* futex_init(char *p, int i);

// destroy the futex and return 0 if everything is ok
int futex_destroy(futex *f);

int futex_lock(futex *f);

int futex_unlock(futex *f);

int futex_trylock(futex *f);

/********************** Assembly code **********************/

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

#endif
