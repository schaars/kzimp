/* This file is part of multicore_replication_microbench.
 *
 * Functions about time measurement
 */

#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

// return the current time in usec
static inline uint64_t get_current_time()
{
  struct timeval t;
  gettimeofday(&t, 0);
  return (t.tv_sec * 1000000 + t.tv_usec);
}

#endif // _TIME_H
