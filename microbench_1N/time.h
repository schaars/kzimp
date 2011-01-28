/* This file is part of multicore_replication_microbench.
 *
 * Functions about time measurement
 */

#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>
#include <sys/time.h>

static uint64_t clock_mhz; // clock frequency in MHz (number of instructions per microseconds)

/****************** rdtsc() related ******************/

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

// initialize clock_mhz
inline void init_clock_mhz()
{
  struct timeval t0, t1;
  uint64_t c0, c1;

  rdtsc(c0);
  gettimeofday(&t0, 0);
  sleep(1);
  rdtsc(c1);
  gettimeofday(&t1, 0);

  //clock_mhz = number of instructions per microseconds
  clock_mhz = (c1 - c0) / ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec
      - t0.tv_usec);
}

/****************** timer ******************/
//TODO
// start
//stop
//elapsed


// Clock frequency in MHz

/*
 * return the difference between t1 and t2, in usec
 * precondition: t1 > t2 and init_clock_mhz() has already been called
 */
static inline uint64_t diffTime(uint64_t t1, uint64_t t2)
{
  return (t1 - t2) / clock_mhz;
}

#endif // _TIME_H
