/*
 * profiler.c
 *
 *  Created on: May 10, 2010
 *      Author: blepers
 */

#include "profiler.h"

int ncpus = 4;
static int sleep_time = 1*TIME_SECOND;

/*
 * Events :
 * - PERF_TYPE_RAW: raw counters. The value must be 0xz0040yyzz.
 *      For 'z-zz' values, see AMD reference manual (eg. 076h = CPU_CLK_UNHALTED).
 *      'yy' is the Unitmask.
 *      The '4' is 0100b = event is enabled (can also be enable/disabled via ioctl).
 *      The '0' before yy indicate which level to monitor (User or OS).
 *              It is modified by the event_attr when .exclude_[user/kernel] == 0.
 *              When it is the case the bits 16 or 17 of .config are set to 1 (== monitor user/kernel).
 *              If this is set to anything else than '0', it can be confusing since the kernel does not modify it when .exclude_xxx is set.
 *
 * - PERF_TYPE_HARDWARE: predefined values of HW counters in Linux (eg PERF_COUNT_HW_CPU_CYCLES = CPU_CLK_UNHALTED).
 */
static event_t default_events[] = {
        /*{ 
           .name = "L3 Accesses",
           .type = PERF_TYPE_RAW,
           .config = 0x40040f7E0,
        },
        { 
           .name = "L3 Misses",
           .type = PERF_TYPE_RAW,
           .config = 0x40040f7E1,
        },
        { 
           .name = "Read Command Latency to Node 0 (#cycles)",
           .type = PERF_TYPE_RAW,
           .config = 0x100408fE2,
        },
        { 
           .name = "Read Commands to Node 0 (count number of previous events)",
           .type = PERF_TYPE_RAW,
           .config = 0x100408fE3,
        },
*/
        { 
           .name = "CLK_UNHALTED",
           .type = PERF_TYPE_HARDWARE,
           .config = PERF_COUNT_HW_CPU_CYCLES,
        },  
      { 
           .name = "RETIRED_INSTR",
           .type = PERF_TYPE_RAW,
           .config = 0x004000C0,
        }
/*,
	 {
		 .name = "CPUDRAM_TO_NODE0",
		 .type = PERF_TYPE_RAW,
		 .config = 0x1004001E0,
	 },
	 {
		 .name = "CPUDRAM_TO_NODE1",
		 .type = PERF_TYPE_RAW,
		 .config = 0x1004002E0,
	 },
	 {
		 .name = "CPUDRAM_TO_NODE2",
		 .type = PERF_TYPE_RAW,
		 .config = 0x1004004E0,
	 },
	 {
		 .name = "CPUDRAM_TO_NODE3",
		 .type = PERF_TYPE_RAW,
		 .config = 0x1004008E0,
	 },
         { 
                  .name = "HT_LINK2",
                  .type = PERF_TYPE_RAW,
                  .config = 0x00403ff8,
         },
         {
                  .name = "HT_LINK2-NOP",
                  .type = PERF_TYPE_RAW,
                  .config = 0x004008f8,
         },
         {
                  .name = "HT_LINK1-NOP",
                  .type = PERF_TYPE_RAW,
                  .config = 0x004008f7,
         },
         { 
                  .name = "HT_LINK0-NOP",
                  .type = PERF_TYPE_RAW,
                  .config = 0x004008f6,
         },*/
};
static int nb_events = sizeof(default_events)/sizeof(*default_events);
static event_t *events = default_events;

uint64_t get_cpu_freq(void) {
   FILE *fd;
   uint64_t freq = 0;
   float freqf = 0;
   char *line = NULL;
   size_t len = 0;

   fd = fopen("/proc/cpuinfo", "r");
   if (!fd) {
      fprintf(stderr, "failed to get cpu frequency\n");
      perror(NULL);
      return freq;
   }

   while (getline(&line, &len, fd) != EOF) {
      if (sscanf(line, "cpu MHz\t: %f", &freqf) == 1) {
         freqf = freqf * 1000000UL;
         freq = (uint64_t) freqf;
         break;
      }
   }

   fclose(fd);
   return freq;
}

static void* thread_loop(void *pdata) {
   int i;
   pdata_t *data = (pdata_t*) pdata;

   cpu_set_t mask;
   CPU_ZERO(&mask);
   CPU_SET(data->core, &mask);
   if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
      fprintf(stdout, "#[%d] Error: cannot set affinity ! (%s)\n", data->core, strerror(errno));
      return NULL;
   }

   struct perf_event_attr *events_attr = calloc(data->nb_events * sizeof(*events_attr), 1);
   assert(events_attr != NULL);
   data->fd = malloc(data->nb_events*sizeof(*data->fd));
   assert(data->fd);

   for (i = 0; i < data->nb_events; i++) {
      events_attr[i].size = sizeof(struct perf_event_attr);
      events_attr[i].type = data->events[i].type;
      events_attr[i].config = data->events[i].config;
      events_attr[i].exclude_kernel = data->events[i].exclude_kernel;
      events_attr[i].exclude_user = data->events[i].exclude_user;
      data->fd[i] = sys_perf_counter_open(&events_attr[i], -1, data->core, -1, 0);
      if (data->fd[i] < 0) {
         fprintf(stdout, "#[%d] sys_perf_counter_open failed: %s\n", data->core, strerror(errno));
	 return NULL;
      }
   }
	
   uint64_t *last_counts = calloc(data->nb_events, sizeof(uint64_t));
   while (1) {
      uint64_t single_count, rdtsc;

      rdtscll(rdtsc);
      for (i = 0; i < data->nb_events; i++) {
         assert(read(data->fd[i], &single_count, sizeof(uint64_t)) == sizeof(uint64_t));
         printf("%d\t%d\t%llu\t%llu\n", i, data->core, (long long unsigned) rdtsc, (long long unsigned) (single_count-last_counts[i]));
	 last_counts[i] = single_count;
      }

      usleep(sleep_time);
   }

   return NULL;
}



void parse_options(int argc, char **argv) {
   int i = 1;
   event_t *evts = NULL;
   int nb_evts = 0;
   for(;;) {
	   if(i>=argc) break;
	   if(!strcmp(argv[i], "-e")) {
		   if(i+4 >= argc)
			   die("Missing argument for -e NAME COUNTER KERNEL USER\n");
		   evts = realloc(evts, (nb_evts+1)*sizeof(*evts));
		   evts[nb_evts].name = strdup(argv[i+1]);
		   evts[nb_evts].type = PERF_TYPE_RAW;
		   evts[nb_evts].config = hex2u64(argv[i+2]);
		   evts[nb_evts].exclude_kernel = atoi(argv[i+3]);
		   evts[nb_evts].exclude_user = atoi(argv[i+4]);
		   nb_evts++;

		   i+=5;
	   } else if(!strcmp(argv[i], "-c")) {
		   if(i+1 >= argc)
			   die("Missing argument for -c NB_CORES\n");
		   ncpus = atoi(argv[i+1]);

		   i+=2;
	   } else {
		   die("Unknown option %s\n", argv[i]);
	   }
   }

   if(nb_evts != 0) {
	   nb_events = nb_evts;
	   events = evts;
   }
}

int main(int argc, char**argv) {
   signal(SIGPIPE, sig_handler);
   signal(SIGTERM, sig_handler);
   signal(SIGINT, sig_handler);
   
   parse_options(argc, argv);

   int i;
   uint64_t clk_speed = get_cpu_freq();

   printf("#Clock speed: %llu\n", (long long unsigned)clk_speed);
   for(i = 0; i< nb_events; i++) {
      printf("#Event %d: %s (%llx) (Exclude Kernel: %s; Exclude User: %s)\n", i, events[i].name, (long long unsigned)events[i].config, (events[i].exclude_kernel)?"yes":"no", (events[i].exclude_user)?"yes":"no");
   }
   printf("#Event\tCore\tTime\t\t\tSamples\n");

   pthread_t threads[ncpus];
   for(i = 0; i < ncpus; i++) {
      pdata_t *data = malloc(sizeof(*data));
      data->core = i;
      data->nb_events = nb_events;
      data->events = events;
      if(i != ncpus -1) {
         pthread_create(&threads[i], NULL, thread_loop, data);
      } else {
         thread_loop(data);
      }
   }

   sleep(10);
   for(i = 0; i < ncpus - 1; i++) {
      pthread_join(threads[i], NULL);
   }

   printf("#END??\n");
   return 0;
}
