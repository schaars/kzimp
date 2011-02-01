#ifndef _ATOMIC_H_
#define _ATOMIC_H_

/* Thx to B. Lepers, S. Geneves, F. Gaud & F. Mottet - Sardes team, Inria Rhone-Alpes */

typedef volatile unsigned int lock_t;

inline unsigned int test_and_set(lock_t *addr) {
  /*
    register unsigned int _res = 1;

    __asm__ __volatile__(
             " xchg   %0, %1 \n"
             : "=q" (_res), "=m" (*addr)
             : "0" (_res));
    return _res;
    */

    return __sync_lock_test_and_set(addr, 1);
}

inline void spinlock_lock(lock_t *addr) {
    while (*addr || test_and_set(addr));
}

inline void spinlock_unlock(lock_t *addr) {
    *addr = 0;
}

#endif
