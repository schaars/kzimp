/*
 * profiler_sched.h
 *
 *  Created on: Feb 21, 2011
 *      Author: fabien
 */

#ifndef PROFILER_SCHED_H_
#define PROFILER_SCHED_H_

#include <time.h>
#include <sys/time.h>

typedef struct thdata {
   int id;        /** Internal no **/
   int tid;       /* Tid to observe */
   int* fd;

   uint64_t last_value[2];
   long double* stats;
   long double avg;

   int current_die;

   char * app_name;
   char * app_cmd;

   struct timeval start;
   int has_finished;
} thdata_t;


#endif /* PROFILER_SCHED_H_ */
