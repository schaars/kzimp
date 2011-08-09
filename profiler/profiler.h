/*
 * profiler.h
 *
 *  Created on: May 10, 2010
 *      Author: blepers
 */

#ifndef PROFILER_H_
#define PROFILER_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/syscall.h>
#define __EXPORTED_HEADERS__
#include <sys/sysinfo.h>
#undef __EXPORTED_HEADERS__
#include <linux/perf_event.h>
//#include <pfmlib_priv.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include "common.h"

#define TIME_SECOND             1000000
#define PAGE_SIZE               (4*1024)

#undef __NR_perf_counter_open
#ifdef __powerpc__
#define __NR_perf_counter_open  319
#elif defined(__x86_64__)
#define __NR_perf_counter_open  298
#elif defined(__i386__)
#define __NR_perf_counter_open  336
#endif


typedef struct pdata {
   int core;
   int nb_events;
   event_t *events;
   int *fd;                /* File descriptor of the perf counter */
   FILE *log;              /* Log file to dump data */
   int pipe[2];            /* Pipe to wake up */
   int tid;		   /* Tid to observe */
} pdata_t;


__attribute__((unused)) static long sys_perf_counter_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
   int ret = syscall(__NR_perf_counter_open, hw_event, pid, cpu, group_fd, flags);
#  if defined(__x86_64__) || defined(__i386__)
   if (ret < 0 && ret > -4096) {
      errno = -ret;
      ret = -1;
   }
#  endif
   return ret;
}

__attribute__((unused)) static void sig_handler(int signal) {
   printf("#signal caught: %d\n", signal);
   fflush(NULL);
   exit(0);
}

__attribute__((unused)) static int hex(char ch) {
        if ((ch >= '0') && (ch <= '9'))
                return ch - '0';
        if ((ch >= 'a') && (ch <= 'f'))
                return ch - 'a' + 10;
        if ((ch >= 'A') && (ch <= 'F'))
                return ch - 'A' + 10;
        return -1;
}

__attribute__((unused)) static uint64_t hex2u64(const char *ptr) {
        const char *p = ptr;
        uint64_t long_val = 0;

        if(p[0] != '0' || (p[1] != 'x' && p[1] != 'X'))
                die("Wrong format for counter. Expected 0xXXXXXX\n");
        p+=2;

        while (*p) {
                const int hex_val = hex(*p);
                if (hex_val < 0)
                        break;

                long_val = (long_val << 4) | hex_val;
                p++;
        }
        return long_val;
}

__attribute__((unused)) static int hex2u64b(const char *ptr, uint64_t *long_val)
{
        const char *p = ptr;
        *long_val = 0;

        while (*p) {
                const int hex_val = hex(*p);

                if (hex_val < 0)
                        break;

                *long_val = (*long_val << 4) | hex_val;
                p++;
        }

        return p - ptr;
}

static inline int sys_perf_event_open(struct perf_event_attr *attr, pid_t pid,
    int cpu, int group_fd, unsigned long flags)
{
  attr->size = sizeof(*attr);
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

#endif /* PROFILER_H_ */
