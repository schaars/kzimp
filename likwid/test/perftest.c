#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "likwid.h"
#include "perfmon.h"
#include "timer.h"
#include "cpuid.h"

#define SIZE 1000000
double sum = 0, a[SIZE], b[SIZE], c[SIZE];

struct esd_metv {
  char** metric;
  int* index;
};

/* global variables */
static struct esd_metv metricv;
static int    nmetrics = 0;

int main(int argc, char** argv) {
  char *var = "MEM";
   double alpha = 3.124;
  char *token;
  int numthreads = 1;
  int* threads;
  int i;
 
   for (i = 0; i < SIZE; i++) {
      a[i] = 2.0;
      b[i] = 3.0;
      c[i] = 3.0;
   }

  /* Initialize likwid perfmon interface */
  cpuid_init();
  numa_init();
  affinity_init();
  timer_init();
#if (defined (ELG_OMPI) || defined (ELG_OMP))
   numthreads = omp_num_threads();
   threads = (int*) malloc(numthreads*sizeof(int));
    #pragma omp parallel num_threads(numthreads)
    {
      threads[omp_thread_num] = affinity_threadGetProcessorId();
    }
#else
    threads = (int*) malloc(numthreads*sizeof(int));
    threads[0] = affinity_processGetProcessorId();
#endif
     perfmon_init(numthreads,threads,stdout);
     //var = calloc();
     //strcpy(var, env);
     printf("%s\n", var);

     nmetrics = perfmon_setupEventSetC(var, &(metricv.metric));
     for(i = 0; i < nmetrics; ++i) {
        printf("Counter: %d, name: %s\n",i,metricv.metric[i]);
     }
     printf("Number of metrics: %d\n",nmetrics);
     perfmon_startCounters();
     for (i = 0; i < SIZE; i++) {
         a[i] = b[i] + alpha * c[i];
         sum += a[i];
     }

     perfmon_readCounters();

     for(i = 0; i < nmetrics; ++i) {
        printf("Counter: %d, name: %s, result: %g\n",
                i,metricv.metric[i],perfmon_getEventResult(0,i));
     }
     perfmon_stopCounters();

     printf( "OK, dofp result = %e\n", sum);
     perfmon_finalize();
     return 0;
}
