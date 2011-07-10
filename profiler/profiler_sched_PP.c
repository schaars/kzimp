#include "profiler.h"
#include "profiler_sched_PP.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <numa.h>
#include <libgen.h>
#include <math.h>

#define MIGRATE_PAGES 1

int ncpus = 4;
static int sleep_time = 0.3* TIME_SECOND;
static const int how_many_time_before_decision = 3;
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
         { .name = "CPUDRAM_TO_ALL_NODES", .type = PERF_TYPE_RAW, .config = 0x100400FE0LL, },
         { .name = "RETIRED_INSTR", .type = PERF_TYPE_RAW, .config = 0x004000C0, },

/**{ .name = "CPUDRAM_TO_NODE0", .type = PERF_TYPE_RAW, .config = 0x1004001E0LL, },
 { .name = "CPUDRAM_TO_NODE1", .type = PERF_TYPE_RAW, .config = 0x1004002E0LL, },
 { .name = "CPUDRAM_TO_NODE2", .type = PERF_TYPE_RAW, .config = 0x1004004E0LL, },
 { .name = "CPUDRAM_TO_NODE3", .type = PERF_TYPE_RAW, .config = 0x1004008E0LL, },
 **/

};
static event_t *events = default_events;
static int nb_events = sizeof(default_events)/sizeof(*events);

static thdata_t ** pe_data;

static int load_balancing = 1;
static volatile int how_many_active_child = 0;

static struct bitmask *old_nodes;
static processor_ev_t * evts;

static const int die_order[] = { 1, 2, 0, 3 };
static cpu_set_t mask[4];

static int nb_observed_apps;
static char** observed_apps;

static void init_pe() {
   evts = calloc(ncpus , sizeof(processor_ev_t));

   int e,per_die,die,core;
   struct perf_event_attr *events_attr = calloc(nb_events , sizeof(*events_attr));
   assert(events_attr != NULL);
   evts->fds = calloc(nb_events , sizeof(*evts->fds));
   for(e = 0; e < nb_events; e++) {
      events_attr[e].size = sizeof(struct perf_event_attr);
      events_attr[e].type = events[e].type;
      events_attr[e].config = events[e].config;
      events_attr[e].exclude_kernel = events[e].exclude_kernel;
      events_attr[e].exclude_user = events[e].exclude_user;

      per_die = (events_attr[e].config & 0x100000000LL);

      evts->fds[e] = calloc(4 , sizeof(*(evts->fds[e])));
      for(die = 0; die < 4; die++) {
         evts->fds[e][die].nb_fd = 4;
         evts->fds[e][die].per_die = per_die;
         evts->fds[e][die].fd = calloc(evts->fds[e][die].nb_fd, sizeof(*evts->fds[e][die].fd));
         evts->fds[e][die].last_values = calloc(evts->fds[e][die].nb_fd, sizeof(*evts->fds[e][die].last_values));
         for(core = 0; core < evts->fds[e][die].nb_fd; core++) {
            int ddie = die;
            if(die == 1)
               ddie = 3;
            else if(die == 3)
               ddie = 1;
            evts->fds[e][die].fd[core] = sys_perf_counter_open(&events_attr[e], -1, core*4 + ddie, -1, 0);
            if (evts->fds[e][die].fd[core] < 0 ) {
               fprintf(stdout, "# sys_perf_counter_open failed: %s (i=%d)\n", strerror(errno), e);
               exit(-1);
            }
         }
      }
   }
}

static int filter(const struct dirent *dir) {
   if (dir->d_name[0] == '.')
      return 0;
   else
      return 1;
}

int find_all_tid(int pid, pid_t ** all_tid) {
   char name[256];
   int items;
   struct dirent **namelist = NULL;
   int ret = 0;
   int i;

   sprintf(name, "/proc/%d/task", pid);
   items = scandir(name, &namelist, filter, NULL);
   if (items <= 0)
      return -ENOENT;
   *all_tid = malloc(sizeof(pid_t) * items);
   if (!*all_tid) {
      ret = -ENOMEM;
      goto failure;
   }

   for (i = 0; i < items; i++)
      (*all_tid)[i] = atoi(namelist[i]->d_name);

   ret = items;

   failure: for (i = 0; i < items; i++)
      free(namelist[i]);
   free(namelist);

   return ret;
}

static void move_app(thdata_t * t, int new_die){
   pid_t * tids = NULL;
   int nb_tids = find_all_tid(t->tid, &tids);
   t->current_die = new_die;

   int j;
   for (j = 0; j < nb_tids; j++) {
      if (sched_setaffinity(tids[j], sizeof(mask[new_die]), &mask[new_die]) < 0) {
         printf("Error when setting affinity of pid %d\n", tids[j]);
         exit(-1);
      }


#if MIGRATE_PAGES
      struct bitmask *new_nodes = numa_bitmask_alloc(4);
      numa_bitmask_setbit(new_nodes, new_die);
      int rc = numa_migrate_pages(tids[j], old_nodes, new_nodes);

      if (rc < 0) {
         perror("numa_migrate_pages failed");
         exit(-1);
      }
      free(new_nodes);
#endif
   }

   free(tids);
}


static int compare(void const *a, void const *b) {
   thdata_t const *pa = *(thdata_t const **) a;
   thdata_t const *pb = *(thdata_t const **) b;

   return pb->avg > pa->avg;
}

unsigned long compare_time(struct timeval* start_time, struct timeval *stop_time) {
   unsigned long sec_res = stop_time->tv_sec - start_time->tv_sec;
   unsigned long usec_res = stop_time->tv_usec - start_time->tv_usec;

   return 1000000 * sec_res + usec_res;

}

static void take_a_decision() {
   //printf("\n------------------\n");
   if (load_balancing) {
      //printf("I gonna take a decision ...\n");

      int i, j;
      for (i = 0; i < nb_observed_apps; i++) {
         pe_data[i]->avg = 0;
         for (j = 0; j < how_many_time_before_decision; j++) {
#if 0
            if(pe_data[i]->stats[j] > pe_data[i]->avg) {
               pe_data[i]->avg = pe_data[i]->stats[j];
            }
#else
            pe_data[i]->avg += pe_data[i]->stats[j];
#endif
         }
         pe_data[i]->avg /= how_many_time_before_decision;

         //printf("Application %15.15s --> %.5Lf\n", pe_data[i]->app_name, pe_data[i]->avg);
      }

      //printf ("\n");
      int old_indexes[4];
      for(i = 0; i < nb_observed_apps; i++) {
         old_indexes[pe_data[i]->id] = i;
      }
      qsort(pe_data, nb_observed_apps, sizeof *pe_data, compare);

      for(i = 0; i < 2; i++) {
         if(old_indexes[pe_data[i]->id] > 1) { //The app i was on a slow die
            printf("i = %d app = %s old = %d current_die=%d\n", i, pe_data[i]->app_name, old_indexes[pe_data[i]->id], pe_data[i]->current_die);
            for(j = 2; j < 4; j++) {
               printf("j = %d app = %s old = %d current_die = %d\n", j, pe_data[j]->app_name, old_indexes[pe_data[j]->id], pe_data[j]->current_die);
               if(pe_data[j]->current_die == 1 || pe_data[j]->current_die == 2) { //The app j was on a fast die
                  thdata_t * now_fast = pe_data[i];
                  thdata_t * now_slow = pe_data[j];
                  int available_fast_die = pe_data[j]->current_die;
                  int available_slow_die = pe_data[i]->current_die;

                  //if( now_fast->avg - now_slow->avg < 0.01 ) {
                  if( now_fast->avg / now_slow->avg < 3 ) {
                     // MAPI diff between apps is too small, don't move
                     printf("Don't move because apps %s and app %s are too close (%.5Lf vs %.5Lf)\n", now_slow->app_name, now_fast->app_name,
                           now_slow->avg,
                           now_fast->avg);
                     pe_data[i] = now_slow;
                     pe_data[j] = now_fast;
                     break;
                  } else {
                     struct timeval stop;
                     gettimeofday(&stop, NULL);
                     printf("Switching applications %15.15s (now fast on %d) and %15.15s (now slow on %d) (%.5Lf vs %.5Lf) at %lums\n", 
                           now_fast->app_name, 
                           available_fast_die,
                           now_slow->app_name,
                           available_slow_die,
                           now_fast->avg,
                           now_slow->avg,
                           compare_time(&now_slow->start,&stop)/1000);

                     move_app(now_fast, available_fast_die);
                     move_app(now_slow, available_slow_die);

                     break;
                  }
               }
            }
            assert(j!=4);
         }
      }

      /*
      i = 0;
      while (i < nb_observed_apps) {
         int to_die[2];
         to_die[0] = die_order[i];
         to_die[1] = die_order[i + 1];

         //printf("%15s --> %.5Lf --> to processor %d [Move needed : %s]\n", 
         //pe_data[i]->app_name, pe_data[i]->avg, to_die,
         //(pe_data[i]->current_die != to_die) ? "yes" : "no");

         if ((pe_data[i]->current_die + pe_data[i + 1]->current_die == 3)
                  && (to_die[0] + to_die[1] == 3)
                  && (to_die[0] == pe_data[i]->current_die || to_die[0] == pe_data[i + 1]->current_die)) {
            // Change between fast<->fast or slow<->slow, don't need to move
            i += 2;
            break;
         }
         
         if( fabs(pe_data[i]->avg - pe_data[i + 1]->avg) < 0.01 ) {
            // MAPI diff between apps is too small, don't move
            printf("Don't move because apps %s and app %s conflict\n", pe_data[i]->app_name, pe_data[i+1]->app_name);
            i += 2;
            break;
         }

         int k;
         for (k = 0; k < 2; k++) {
            thdata_t * t = pe_data[i + k];
            printf("Application %15.15s maps on processor %d (%.5Lf)\n", t->app_name, to_die[k], t->avg);
            move_app(t, to_die[k]);
         }
         i += 2;

         printf("\n");
      }
      */
   }
}

static inline void do_read (int current_stat) {
   int a = 0,e;
   for (a = 0; a < nb_observed_apps; a++) {
      if (pe_data[a]->has_finished) {
         pe_data[a]->stats[current_stat] = 0;
         continue;
      }

      //printf("App %s was on die %d. Reading on that core.\n", pe_data[a]->app_name, pe_data[a]->current_die);

      uint64_t sums[2];
      for(e = 0; e < nb_events; e++) {
         uint64_t value,sum = 0;
         int current_die = pe_data[a]->current_die;
         int nb_fd = evts->fds[e][current_die].nb_fd, fd;
         for(fd = 0; fd < nb_fd; fd++) {
            assert(read(evts->fds[e][current_die].fd[fd], &value, sizeof(uint64_t)) == sizeof(uint64_t));
            if(current_stat >= 0 &&
                  (evts->fds[e][current_die].last_values[fd] > value)) {
               printf("ERROR when reading counter %d on die %d, core %d %lu %lu\n", e, current_die, (4*current_die+fd), value, evts->fds[e][current_die].last_values[fd]);
               exit(-1);
            }
            if(evts->fds[e][current_die].per_die) {
               if(value - evts->fds[e][current_die].last_values[fd] > sum)
                  sum = value - evts->fds[e][current_die].last_values[fd];
            } else {
               sum += (value - evts->fds[e][current_die].last_values[fd]);
            }
            evts->fds[e][current_die].last_values[fd] = value;
         }
         sums[e] = sum;
      }
      if(current_stat >= 0){
         if (sums[1]) {
            pe_data[a]->stats[current_stat] = (long double) sums[0] / (long double) sums[1];
         }
         else{
            pe_data[a]->stats[current_stat] = 0;
         }
      }

      /*printf("[%d] Application %15.15s : %.5Lf [%lu, %lu]\n",
      current_stat, pe_data[a]->app_name, pe_data[a]->stats[current_stat],
      sums[0], sums[1]
      );*/


   }

   //printf("\n");
}


static void watch_app() {
   int current_stat = 0;
   do_read(-1);

   while (1) {
      usleep(sleep_time);

      do_read(current_stat);
      current_stat++;
      if (current_stat == how_many_time_before_decision) {
         take_a_decision();
         current_stat = 0;
         //return;
      }
   }
}

void parse_options(int argc, char **argv) {
   int i = 1;
   for (;;) {
      if (i >= argc)
         break;
      if (!strcmp(argv[i], "-a")) {
         if (i + 1 >= argc)
            die("Missing argument for -a APPLICATION\n");

         observed_apps = realloc(observed_apps, (nb_observed_apps + 1) * sizeof(*observed_apps));
         observed_apps[nb_observed_apps] = argv[i + 1];
         nb_observed_apps++;
         i += 2;

      }
      else if (!strcmp(argv[i], "-nb")) {
         load_balancing = 0;
         i++;
      }
      else {
         die("Unknown option %s\n", argv[i]);
      }
   }
}

static void sig_chld(int signal) {
   int status, i;

   load_balancing = 0;

   for (i = 0; i < nb_observed_apps; i++) {
      int pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
      if (pid > 0) {
         int j = 0;
         thdata_t* t = NULL;
         for (j = 0; j < nb_observed_apps; j++) {
            if (pe_data[j]->tid == pid) {
               t = pe_data[j];
               break;
            }
         }

         t->has_finished = 1;

         struct timeval stop;
         gettimeofday(&stop, NULL);
         unsigned long usec = compare_time(&t->start, &stop);

         printf("App %15.15s has finished in %.3f s\n", t->app_name, (double) usec / 1000000.);
         --how_many_active_child;
      }
   }

   if (how_many_active_child == 0)
      exit(0);
}

int main(int argc, char**argv) {
   signal(SIGPIPE, sig_handler);
   signal(SIGTERM, sig_handler);
   signal(SIGINT,  sig_handler);
   signal(SIGCHLD, sig_chld);

   /** INIT Cpuset ***/
   CPU_ZERO(&mask[0]);
   CPU_ZERO(&mask[1]);
   CPU_ZERO(&mask[2]);
   CPU_ZERO(&mask[3]);

   CPU_SET(0, &mask[0]);
   CPU_SET(4, &mask[0]);
   CPU_SET(8, &mask[0]);
   CPU_SET(12, &mask[0]);

   CPU_SET(3, &mask[1]);
   CPU_SET(7, &mask[1]);
   CPU_SET(11, &mask[1]);
   CPU_SET(15, &mask[1]);

   CPU_SET(2, &mask[2]);
   CPU_SET(6, &mask[2]);
   CPU_SET(10, &mask[2]);
   CPU_SET(14, &mask[2]);

   CPU_SET(1, &mask[3]);
   CPU_SET(5, &mask[3]);
   CPU_SET(9, &mask[3]);
   CPU_SET(13, &mask[3]);

   old_nodes = numa_bitmask_alloc(4);
   numa_bitmask_setbit(old_nodes, 0);
   numa_bitmask_setbit(old_nodes, 1);
   numa_bitmask_setbit(old_nodes, 2);
   numa_bitmask_setbit(old_nodes, 3);
   /** End init cpuset **/


   parse_options(argc, argv);

   if (nb_observed_apps == 0) {
      fprintf(stderr, "Usage :  %s [-a app]\n", argv[0]);
      exit(EXIT_FAILURE);
   } else if(nb_observed_apps != 4) {
      fprintf(stderr, "Need 4 apps\n");
      exit(EXIT_FAILURE);
   }


   int i;
   pe_data = (thdata_t**) calloc(nb_observed_apps, sizeof(thdata_t*));

   for (i = 0; i < nb_observed_apps; i++) {
      printf("Forking ...\n");
      pid_t pid = fork();

      if (pid > 0) {
         pe_data[i] = calloc(1, sizeof(thdata_t));
         pe_data[i]->id = i;
         pe_data[i]->tid = pid;
         pe_data[i]->stats = (long double*) calloc(how_many_time_before_decision, sizeof(long double));
         pe_data[i]->current_die = -1;
         pe_data[i]->app_cmd = observed_apps[i];

         gettimeofday(&(pe_data[i]->start), NULL);

         char *cmd;
         cmd = strtok(observed_apps[i], " ");
         pe_data[i]->app_name = basename(cmd);

         move_app(pe_data[i], die_order[i]);
      } else if (pid == 0) {
#define MAX_ARGS_SIZE 16

         fclose(stdout);
         fclose(stderr);

         char *cmd[MAX_ARGS_SIZE];
         cmd[0] = strtok(observed_apps[i], " ");

         printf("[%d] Running : %s ", getpid(), cmd[0]);
         int i;
         for (i = 1; i < MAX_ARGS_SIZE - 1; i++) {
            cmd[i] = strtok(NULL, " ");
            if (cmd[i] == NULL) {
               break;
            }
            printf("%s ", cmd[i]);
         }

         printf("\n");
         cmd[MAX_ARGS_SIZE - 1] = NULL;

         execvp(cmd[0], &cmd[0]);
      }
      else {
         fprintf(stderr, "Error cannot fork\n");
         exit(EXIT_FAILURE);
      }
   }

   how_many_active_child = nb_observed_apps;

   if(load_balancing) {
      init_pe();
      watch_app();
   } 

   int status = 0;

   while (1)
      sleep(10);

   printf("Child status : %d\n", status);

   return 0;
}
