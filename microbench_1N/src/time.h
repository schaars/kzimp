/* This file is part of multicore_replication_microbench.
 *
 * Functions about time measurement
 */

#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

// the well-known rdtsc(), in 32 and 64 bits versions
// has to be used with a uint_64t
#ifdef __x86_64__
#define rdtsc(val) { \
    unsigned int __a,__d;                                        \
    asm volatile("rdtsc" : "=a" (__a), "=d" (__d));              \
    (val) = ((unsigned long)__a) | (((unsigned long)__d)<<32);   \
}

#else
#define rdtsc(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

// return the current time in usec
static inline uint64_t get_current_time()
{
  struct timeval t;
  gettimeofday(&t, 0);
  return (t.tv_sec * 1000000 + t.tv_usec);
}

#endif // _TIME_H
