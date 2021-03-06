/*
 * dump_log.c
 *
 *  Created on: Aug 19, 2010
 *      Author: blepers
 */

#include "parser-sampling.h"


struct options options;
struct mmaped_file {
   char *data;
   size_t size, index;
   char *name;
   int event, core;
   int fails, successes, ignores;
   uint64_t old_time[MAX_EVT];

   //Temporal dump
   uint64_t last_dump_time[MAX_EVT];
   uint64_t samples_since_last_dump[MAX_EVT];
} *mmaped_files;
GHashTable* pid_hash[MAX_EVT][MAX_CORE];
GHashTable* tid_hash[MAX_EVT][MAX_CORE];
static int total_successes, total_ignores;

static void print_pids(gpointer key, gpointer value, gpointer user_data) {
   int *pid = key;
   int *num = value;
   struct process *p;
   if(options.tid) {
      int *rpid = g_hash_table_lookup(pid_hash[0][0], int_key(*pid));
      p = symb_find_pid(*rpid);
   } else {
      p = symb_find_pid(*pid);
   }
   printf("%d: %d (%s)\n", *pid, *num, p?p->name:"unknown");
}

/**
 * Check that we are parsing the file on the same computer which created the file. Mandatory in order to have correct kernel symbols information.
 */
static void check_host(struct mmaped_file *f) {
   char host[512];
   if(!sscanf(f->data, "#Host %s", host))
      die("No host in file %s", f->name);
   else {
      char *hostname = malloc(512);
      gethostname(hostname, 512);
      if(strcmp(host, hostname))
         printf("#WARNING: HOSTS DIFFER (%s vs %s), functions names will likely be wrong\n", hostname, host);
      free(hostname);
   }

   while(*f->data!='\n') {
      f->data++;
      f->size--;
   }
   f->data++;
   f->size--;

   /* Extra information lines */
   while(*f->data=='#') {
      while(*f->data!='\n') {
         f->data++;
         f->size--;
      }
      f->data++;
      f->size--;
   }
}

/**
 * Get the events array.
 */
static sample_event_t seen_events[MAX_EVT];
static int check_events(struct mmaped_file *f) {
   int nb_evts = 0;
   size_t old;
   for (old = 0; old < f->size; ) {
      sample_event_t *event = (sample_event_t *)&f->data[old];

      if(event->header.type == PERF_RECORD_CUSTOM_EVENT_EVENT) {
         if(nb_evts>=MAX_EVT)
            die("Too many events in file %s (%d; maximum of %d authorized)\n", f->name, nb_evts, MAX_EVT);
         if(seen_events[nb_evts].new_event.name[0] == 0) {
            memcpy(&seen_events[nb_evts], event, event->header.size);
            printf("#Event %d: %s (%llx), count %llu (Exclude Kernel: %s; Exclude User: %s)\n", nb_evts, event->new_event.name, (long long unsigned)event->new_event.event.config, (long long unsigned) event->new_event.event.sampling_period, (event->new_event.event.exclude_kernel)?"yes":"no", (event->new_event.event.exclude_user)?"yes":"no");
            if(options.remote && strncmp("CPU_DRAM_NODE", event->new_event.name, sizeof("CPU_DRAM_NODE") - 1)) 
               die("Option --remote can only be set when monitoring CPU_DRAM events (seen %s)\n", event->new_event.name);
         } else {
            if(strcmp(seen_events[nb_evts].new_event.name, event->new_event.name) 
                  || event->new_event.event.config != seen_events[nb_evts].new_event.event.config
                  || event->new_event.event.sampling_period != seen_events[nb_evts].new_event.event.sampling_period
                  || event->new_event.event.exclude_kernel != seen_events[nb_evts].new_event.event.exclude_kernel
                  || event->new_event.event.exclude_user != seen_events[nb_evts].new_event.event.exclude_user) {
               die("Seen in file %s:\n#Event %d: %s (%llx), count %llu (Exclude Kernel: %s; Exclude User: %s)\nExpected:\n#Event %d: %s (%llx), count %llu (Exclude Kernel: %s; Exclude User: %s)\n", f->name, nb_evts, event->new_event.name, (long long unsigned)event->new_event.event.config, (long long unsigned) event->new_event.event.sampling_period, (event->new_event.event.exclude_kernel)?"yes":"no", (event->new_event.event.exclude_user)?"yes":"no", nb_evts, seen_events[nb_evts].new_event.name, (long long unsigned)seen_events[nb_evts].new_event.event.config, (long long unsigned) seen_events[nb_evts].new_event.event.sampling_period, (seen_events[nb_evts].new_event.event.exclude_kernel)?"yes":"no", (seen_events[nb_evts].new_event.event.exclude_user)?"yes":"no");
            }
         }
         nb_evts++;
      } else {
         break;
      }

      old +=  event->header.size;
   }
   f->size -= old;
   f->data += old;
   if(f->size <= 0) {
      if(options.die_on_empty_file)
         die("No events in file %s", f->name);
      return 1;
   } else {
      return 0;
   }
}

/**
 * Get the samples.
 */
uint64_t samples_first_time, samples_last_time;
uint64_t first_time, last_time;
uint64_t total_samples[MAX_EVT][MAX_CORE];

static struct ip_event* read_sample(sample_event_t *evt) {
   /*struct ip_event *e = malloc(sizeof(*e));
     e->ip = evt->ip.ip;
     return e;*/
   //printf("EVT %d %d\n", evt->header.type, PERF_RECORD_SAMPLE);
   //fflush(stdout);
   return &evt->ip;
}

static int check_samples(struct mmaped_file *f) {
   int i;
   char *raw_data;
   int custom_event_read = 0, type;
   int ignore_samples = options.ignore_samples;
   sample_event_t *base_event = (sample_event_t *)&f->data[f->index];
   struct process *p;

   struct ip_event *evt;
   for (; f->index < f->size; ) {
      sample_event_t *event = (sample_event_t *)&f->data[f->index];

      switch(event->header.type) {
         case PERF_RECORD_SAMPLE:
            evt = read_sample(event);
            type = 0;


            if(options.kernel && (evt->header.misc & PERF_RECORD_MISC_CPUMODE_MASK) != PERF_RECORD_MISC_KERNEL)
               break;
            if(options.user && (evt->header.misc & PERF_RECORD_MISC_CPUMODE_MASK) != PERF_RECORD_MISC_USER)
               break;

            /* Filter by TID */
            if(options.ltid) {
               if(!(type & PERF_SAMPLE_TID))
                  die("Cannot filter on TID an event without TID");
               else if(evt->tid != options.ltid)
                  break;
            }

            /* Filter by CPU */
            int core = f->core;
            if(type & PERF_SAMPLE_CPU)
               core = evt->cpu;
            if(options.nb_cpus) {
               int found = 0;
               for(i = 0; !found && i < options.nb_cpus; i++) {
                  if(core == options.cpus[i])
                     found = 1;
               }
               if(!found)
                  break;
            }

            /* Remote */
            if(options.remote) {
               int die = core %4;
               if(die == 1)
                  die = 3;
               else if(die == 3)
                  die = 1;
               if(f->event == die) //Eliminate local evts. Supposes that EVT0 = to die 0 etc.
                  break;
            }


            /* Time checks */
            if(type & PERF_SAMPLE_TIME) {
               if(evt->time < options.min_time || evt->time > options.max_time) 
                  break;
               if(!samples_first_time || evt->time < samples_first_time)
                  samples_first_time = evt->time;
               if(evt->time > samples_last_time)
                  samples_last_time = evt->time;
               if(options.ignore_samples) {
                  if(first_time == 0) 
                     first_time = base_event->write_event.time;
                  last_time = base_event->write_event.time;
               }
               if(ignore_samples) 
                  break;
               if(evt->time < f->old_time[f->event]) /* Simple check : time should always go forward, otherwise this means that samples are mixed up. Should never happen. */
                  fprintf(stderr, "#TIME BUG CORE %d EVT %d\n", f->event, core);
            } else {
               if(options.ignore_samples)
                  die("Option ignore sample has no meaning when PERF_SAMPLE_TIME is not set.");
            }


            if(options.ttid) {
               if(!(type & PERF_SAMPLE_TIME))
                  die("Option --ttid requires profiling with PERF_SAMPLE_TIME");
               if(!(type & PERF_SAMPLE_TID))
                  die("Option --ttid requires profiling with PERF_SAMPLE_TID");

               p = symb_find_pid(evt->pid);
               printf("%llu\t%d\t%d\t%d\t%s\n",(long long unsigned)evt->time, evt->tid, f->event, core, p?p->name:"unknown");
            } else {
               if(type & PERF_SAMPLE_TIME)
                  f->old_time[f->event] = evt->time;
               int ret = symb_add_sample(&event->ip, (options.all_events)?0:f->event, core);
               if(ret == 4) { //ignored
                  f->ignores++;
               } else if(ret == 5) {  // ret == 5: ignored samples because of option.callee; we still want to include it in the total
                  total_ignores++;
                  f->ignores++;
                  total_number_of_samples[(options.all_events)?0:f->event]++;
               } else if(ret) { 
                  //printf("fail %llu %d %d\n", (long long unsigned)event->ip.ip, event->ip.pid, event->ip.tid);
                  f->fails++;
               } else {
                  total_successes++;
                  f->successes++;
                  total_number_of_samples[(options.all_events)?0:f->event]++;
               }
               if((options.app && ret != 4 && ret != 1)
                     || (!options.app))
                  total_samples[(options.all_events)?0:f->event][core]++;

               if(first_time == 0) 
                  first_time = base_event->write_event.time;
               last_time = base_event->write_event.time;
            }
            break;
         case PERF_RECORD_MMAP: /* A file has just been mmaped in virtual memory -> used to know where the symbols of shared libs are loaded */
            /*if(options.ltid && event->mmap.tid == options.ltid)
              printf("MMAP EVT for tid %d %s %llu %llu %d\n", options.ltid, event->mmap.filename, (long long unsigned)event->mmap.start, (long long unsigned)event->mmap.len, event->mmap.pid);*/

            symb_add_exe(event->mmap.filename);
            symb_add_map(&event->mmap);
            break;
         case PERF_RECORD_FORK: /* fork(): we add the pid to the list of known pids */
            /*if(options.ltid && event->fork.tid == options.ltid)
              printf("FORK EVT for tid %d\n", options.ltid);*/
            symb_add_fork(&event->fork);
            break;
         case PERF_RECORD_EXIT: /* exit(): useless for now? */
            break;
         case PERF_RECORD_COMM: /* comm = exec(): used to do the link between pids and process names */
            /*if(options.ltid && event->comm.tid == options.ltid)
              printf("COMM EVT for pid %d tid %d\n", event->comm.pid, event->comm.tid);*/
            symb_add_pid(&event->comm);
            break;
         case PERF_RECORD_LOST: /* Bad... */
            fprintf(stderr, "#Event losts CORE %d EVT %d\n", f->event, f->core);
            break;
         case PERF_RECORD_CUSTOM_WRITE_EVENT: /* Not standard: one of our events. Used to tell the separation between two write() / dumps of the mmaped data */
            if(custom_event_read)
               return 0;
            custom_event_read = 1;
            f->core = event->write_event.core;
            f->event = event->write_event.event;
            if(f->event >= MAX_EVT)
               die("Got event %d which is > %d", f->event, MAX_EVT);
            if(event->write_event.failed) 
               die("A mmap_read failed during profiling ?!\n");
            break;
         case PERF_RECORD_THROTTLE:
         case PERF_RECORD_UNTHROTTLE:
            if(options.dieonfail)
               die("Event %d has been %s => results will be false\n", f->event, event->header.type==PERF_RECORD_THROTTLE?"throttled":"unthrottled");
            else
               fprintf(stderr, "Event %d has been %s => results will be false\n", f->event, event->header.type==PERF_RECORD_THROTTLE?"throttled":"unthrottled");
            break;
         default: /* Very baadd... */
            raw_data = (char*)event;
            if(!strncmp(raw_data, "###END HEADER", sizeof("###END HEADER") -1)) {
               printf("%s", raw_data + sizeof("###END HEADER"));
               f->index = f->size;
               break;
            }
            die("%d\t%d\tUNKNOWN HEADER [WARNING THIS PROBABLY MEANS THAT WE ARE LOSING LOTS AND LOTS OF SAMPLES AND THE WHOLE PROFILER IS COMPLETLY CONFUSED]\n", f->core, event->header.type);
            break;
      }

      f->index +=  event->header.size;
   }
   if(f->index >= f->size && f->successes)
      printf("#Finished parsing %s (%d failed events and %d ignored and %d succeeded - %d%%)\n", f->name, f->fails, f->ignores, f->successes, f->fails*100/(f->fails+f->successes));
   return (f->index >= f->size);
}

static int get_lowest_time(struct mmaped_file *mmaped_files, int argc) {
   int i, ret_index = -1;
   uint64_t lower = -1;

   for (i = 1; i < argc; i++) {
      if(mmaped_files[i].index >= mmaped_files[i].size)
         continue;
      sample_event_t *event = (sample_event_t *)&mmaped_files[i].data[mmaped_files[i].index];
      if(event->header.type != PERF_RECORD_CUSTOM_WRITE_EVENT)
         die("%s: unwanted header; expected mmap boundary and got %d", mmaped_files[i].name, event->header.type);
      if(event->write_event.time < lower) {
         ret_index = i;
         lower = event->write_event.time;
      }
   }
   if(lower == -1)
      die("Bug: no index found");

   //sample_event_t *event = (sample_event_t *)&mmaped_files[ret_index].data[mmaped_files[ret_index].index];

   return ret_index;
}

/**
 * Main
 */
void usage(char *mess) {
   if(mess)
      fprintf(stderr, "\033[31m%s\033[0m\n\n", mess);

   fprintf(stderr, "Usage : ./parser-sampling [options] files\n\n");
   fprintf(stderr, "Options:\n");
   fprintf(stderr, "\t-k                     Consider only samples from the kernel\n");
   fprintf(stderr, "\t-u                     Consider only samples from the user\n");
   fprintf(stderr, "\t-c <c>                 Consider only samples from core <c>; may be specified multiple times\n");
   fprintf(stderr, "\t--app <a>              Consider only samples from application <a>\n");
   fprintf(stderr, "\t--base-event <num>     Consider only samples from event number <num>\n");
   fprintf(stderr, "\t                       Event numbers are printed at the begining of the file. E.g., \"#Event 0: CPU_DRAM_NODE0...\"\n");
   fprintf(stderr, "\t                       THIS OPTION IS MANDATORY TO UNDERSTAND RESULTS!  Without this options samples coming from all events are summed and percentages\n");
   fprintf(stderr, "\t                       are not relevant! (E.g., if a function ticks 20 time in CLK and 30 times in INSTR, it will have 50 samples which is meaningless)\n");
   fprintf(stderr, "\t--min <t>, --max <t>   Consider only samples between min and max time\n");
   fprintf(stderr, "\t                       Time values may be found on the line \"#SAMPLES - Total duration of the bench <duration> (<min> -> <max>)\" of the dump\n");
   fprintf(stderr, "\t--remote               Show only samples touching remote memory (only valid if file contains CPU_DRAM_NODE[0..3] as events, in that order)\n");
   fprintf(stderr, "\t--callee <f>           Show only samples which come directly or indirectly from function <f> (i.e., <f> is in the callchain)\n");
   fprintf(stderr, "\t--non-callee <f>       Show only samples which DO NOT come directly or indirectly from function <f> (i.e., <f> is NOT in the callchain)\n");
   fprintf(stderr, "\t                       E.g., '--callee vfs_stat --non-callee sys_open --non-callee sys_close' shows samples coming from vfs_stat but not from open or close\n");
   fprintf(stderr, "\t--ignore-samples       Don't parse samples, only show informations on the file\n");
   fprintf(stderr, "\t--die-on-empty         Die if a file does not contain any sample\n");
   fprintf(stderr, "\t--force                Try to do the job, even if it sounds silly\n");
   exit(0);
}

int main(int argc, char **argv) {
   struct stat sb;
   int fd, i, j;
   int finished = 0; /* Nb file processed */

   if (argc < 2)
      usage(NULL);
   mmaped_files = calloc(1, (argc)*sizeof(*mmaped_files));

   symb_add_kern("/proc/kallsyms");

   int file_index = 0;
   options.all_events = 1;
   options.dieonfail = 1;
   for(i = 1; i < argc; i++) {
      if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
         usage(NULL);

      if(!strcmp(argv[i], "--ignore-samples")) {
         options.ignore_samples = 1;
         continue;
      } else if(!strcmp(argv[i], "-k")) {
         options.kernel = 1;
         continue;
      } else if(!strcmp(argv[i], "-u")) {
         options.user = 1;
         continue;
      } else if(!strcmp(argv[i], "--force")) {
         options.dieonfail = 0; /* Don't fuck with me, do it */
         continue;
      } else if(!strcmp(argv[i], "--min")) {
         options.min_time = atol(argv[++i]);
         continue;
      } else if(!strcmp(argv[i], "--max")) {
         options.max_time = atol(argv[++i]);
         continue;
      } else if(!strcmp(argv[i], "--base-event")) {
         options.base_event = atol(argv[++i]);
         options.all_events = 0;
         continue;
      } else if(!strcmp(argv[i], "--app")) {
         options.app = argv[++i];
         continue;
      } else if(!strcmp(argv[i], "--ttid")) {
         options.ttid = 1;
         continue;
      } else if(!strcmp(argv[i], "--ltid")) {
         options.ltid = atoi(argv[++i]);
         continue;
      } else if(!strcmp(argv[i], "-c")) {
         i++;
         do {
            if(*argv[i] == ',')
               argv[i] = (argv[i]+1);
            options.cpus[options.nb_cpus] = atol(argv[i]);
            options.nb_cpus++;
            while(*argv[i] && *argv[i] != ',')
               argv[i] = argv[i]+1;
         } while(*argv[i]);
         continue;
      } else if(!strcmp(argv[i], "--remote")) {
         options.remote = 1;
         continue;
      } else if(!strcmp(argv[i], "--callee")) {
         options.callee = list_add(options.callee, argv[++i]);
         continue;
      } else if(!strcmp(argv[i], "--non-callee")) {
         options.non_callee = list_add(options.non_callee, argv[++i]);
         continue;
      } else if(!strcmp(argv[i], "--die-on-empty")) {
         options.die_on_empty_file = 1;
         continue;
      }

      file_index++;

      /* Open and map argv[i] to memory */
      if((fd = open (argv[i], O_RDONLY)) == -1) {
         die("open %s: %s\n", argv[i], strerror(errno));
      } else if (fstat (fd, &sb) == -1) {
         die("fstat : %s\n", strerror(errno));
      } else if (!S_ISREG (sb.st_mode)) {
         die("%s is not a file\n", argv[i]);
      } else if((mmaped_files[file_index].data = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         die("mmap: %s\n", strerror(errno));
      } else if (close (fd) == -1) {
         die("close: %s\n", strerror(errno));
      }
      mmaped_files[file_index].size = sb.st_size;
      mmaped_files[file_index].name = argv[i];

      /* Check that host is consistent so that we can load kernel symbols */
      check_host(&mmaped_files[file_index]);

      /* Get the events array */
      finished += check_events(&mmaped_files[file_index]);
   }
   if(!file_index)
      usage("Please specify at least one file");

   if(!options.max_time)
      options.max_time = -1;
   for(i = 0; i < MAX_EVT; i++) {
      for(j = 0; j < MAX_CORE; j++) {
         pid_hash[i][j] = g_hash_table_new(g_int_hash, g_int_equal);
         tid_hash[i][j] = g_hash_table_new(g_int_hash, g_int_equal);
      }
   }

   while(finished < file_index) {
      i = get_lowest_time(mmaped_files, argc);
      /* Get the samples */
      finished += check_samples(&mmaped_files[i]);
   }
   printf("#RDT - Total duration of the bench %llu (%llu -> %llu)\n", (long long unsigned)(last_time - first_time), (long long unsigned)first_time, (long long unsigned)last_time);
   printf("#SAMPLES - Total duration of the bench %llu (%llu -> %llu)\n", (long long unsigned)(samples_last_time - samples_first_time), (long long unsigned)samples_first_time, (long long unsigned)samples_last_time);
   if(options.all_events)
      printf("#TOTAL SAMPLES OF EVT %s ", "ALL_EVTS");
   else
      printf("#TOTAL SAMPLES OF EVT %d ", options.base_event);
   for(i = 0; i < MAX_CORE; i++) {
      printf("%d ", (int)total_samples[options.base_event][i]);
   }
   printf("\n");

   if(options.pid || options.tid) {
      for(i = 0; i < MAX_EVT; i++) {
         printf("evt %d\n", i);
         for(j = 0; j < MAX_CORE; j++) {
            printf("core %d\n", j);
            if(options.pid)
               g_hash_table_foreach(pid_hash[i][j], print_pids, NULL);
            else
               g_hash_table_foreach(tid_hash[i][j], print_pids, NULL);
         }
      }
   }

   if(options.callee)
      printf("#%% of samples having callee %s: %.2f%% (%d / %d)\n", (char*)options.callee->data, 100.*((float)total_successes)/((float)total_successes+total_ignores), total_successes, total_successes + total_ignores);
   if(options.non_callee)
      printf("#%% of samples not having callee %s: %.2f%% (%d / %d)\n", (char*)options.non_callee->data, 100.*((float)total_successes)/((float)total_successes+total_ignores), total_successes, total_successes + total_ignores);

   if(!options.ignore_samples && !options.temporal_dump && !options.pid && !options.tid && !options.ttid)
      symb_print_top();

   return 0;
}
