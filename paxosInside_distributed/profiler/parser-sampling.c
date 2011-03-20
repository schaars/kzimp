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
	   if(!strncmp(f->data, "#CPU_IN_IP_EVENTS", sizeof("#CPU_IN_IP_EVENTS") -1)) {
		int value = atoi(f->data+sizeof("#CPU_IN_IP_EVENTS"));
		if(value != CPU_IN_IP_EVENTS) {
			die("Parser compiled with CPU_IN_IP_EVENTS=%d; found CPU_IN_IP_EVENTS=%d in logs. Please recompile parser.", CPU_IN_IP_EVENTS, value);
		}
	   }

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
static void check_events(struct mmaped_file *f) {
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
   if(f->size <= 0)
      die("No events in file %s", f->name);
}

/**
 * Get the samples.
 */
uint64_t samples_first_time, samples_last_time;
uint64_t first_time, last_time;
uint64_t total_samples[MAX_EVT][MAX_CORE];
static int check_samples(struct mmaped_file *f) {
   int i;
   char *raw_data;
   int custom_event_read = 0;
   int ignore_samples = options.ignore_samples;
   sample_event_t *base_event = (sample_event_t *)&f->data[f->index];
   struct process *p;
   for (; f->index < f->size; ) {
      sample_event_t *event = (sample_event_t *)&f->data[f->index];

      switch(event->header.type) {
         case PERF_RECORD_SAMPLE:
	    if(options.ltid && event->ip.tid != options.ltid)
	        break;

	    int core = f->core;
#if CPU_IN_IP_EVENTS
	    core = event->ip.cpu;
#endif
	    assert(core < MAX_CORE);

            if(options.nb_cpus) {
		int found = 0;
		for(i = 0; !found && i < options.nb_cpus; i++) {
			if(core == options.cpus[i])
				found = 1;
		}
		if(!found)
			break;
	    }

	    if(event->ip.time < options.min_time || event->ip.time > options.max_time) {
		    break;
	    }
				
	    if(!samples_first_time || event->ip.time < samples_first_time)
	   	 samples_first_time = event->ip.time;
	    if(event->ip.time > samples_last_time)
	   	 samples_last_time = event->ip.time;
	    if(options.ignore_samples) {
	       if(first_time == 0) 
		       first_time = base_event->write_event.time;
	       last_time = base_event->write_event.time;
	    }
            if(ignore_samples) 
	       break;

            if(event->ip.time < f->old_time[f->event]) { /* Simple check : time should always go forward, otherwise this means that samples are mixed up. Should never happen. */
               fprintf(stderr, "#TIME BUG CORE %d EVT %d\n", f->event, core);
	    } else if(options.temporal_dump) {
	       if(event->ip.time - f->last_dump_time[f->event] > options.temporal_dump) {
		       printf("%d\t%llu\t%llu\n", f->event, (long long unsigned)event->ip.time, (long long unsigned)f->samples_since_last_dump[f->event]);
		       f->last_dump_time[f->event] = event->ip.time;
		       f->samples_since_last_dump[f->event] = 0;
	       }
	       f->samples_since_last_dump[f->event]++;
	    } else if(options.pid) {
		    int *value = g_hash_table_lookup(pid_hash[f->event][core], int_key(event->ip.pid));
		    if(!value) {
			    value = calloc(1,sizeof(*value));
			    g_hash_table_insert(pid_hash[f->event][core], int_key(event->ip.pid), value);
		    }
		    *value = *value+1;
	    } else if(options.tid) {
		    int *value = g_hash_table_lookup(tid_hash[f->event][core], int_key(event->ip.tid));
		    if(!value) {
			    value = calloc(1,sizeof(*value));
			    g_hash_table_insert(tid_hash[f->event][core], int_key(event->ip.tid), value);
		    }
		    *value = *value+1;

		    value = g_hash_table_lookup(pid_hash[f->event][core], int_key(event->ip.tid));
		    if(!value) {
			    value = calloc(1,sizeof(*value));
			    *value = event->ip.pid;
			    g_hash_table_insert(pid_hash[f->event][core], int_key(event->ip.tid), value);
		    } else if(*value != event->ip.pid) {
			    //fprintf(stderr, "Error: tid has two pids...\n");
		    }
	    } else if(options.ttid) {
		    p = symb_find_pid(event->ip.pid);
		    printf("%llu\t%d\t%d\t%d\t%s\n",(long long unsigned)event->ip.time, event->ip.tid, f->event, core, p?p->name:"unknown");
            } else {
               f->old_time[f->event] = event->ip.time;
               int ret = symb_add_sample(&event->ip, (options.all_events)?0:f->event, core);
	       if(ret == 4) { //ignored
		  f->ignores++;
               } else if(ret) {
		  //printf("fail %llu %d %d\n", (long long unsigned)event->ip.ip, event->ip.pid, event->ip.tid);
	          f->fails++;
	       } else {
	          f->successes++;
		  total_number_of_samples[f->event]++;
	       }
	       if(ret != 4)
		       total_samples[f->event][core]++;

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
	    /*if(options.ttid) {
		    printf("#EVT %d RDT %llu\n", f->event, (long long unsigned)event->write_event.time);
	    }*/
	    if(event->write_event.failed) {
               die("A mmap_read failed during profiling ?!\n");
	    }
            break;
         case PERF_RECORD_THROTTLE:
         case PERF_RECORD_UNTHROTTLE:
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
      printf("#Finished parsing %s (%d events skipped and %d ignored and %d success - %d%%)\n", f->name, f->fails, f->ignores, f->successes, f->fails*100/(f->fails+f->successes));
   return (f->index >= f->size);
}

static int get_lowest_time(struct mmaped_file *mmaped_files, int argc) {
   int i, ret_index;
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
int main(int argc, char **argv) {
   struct stat sb;
   int fd, i, j;

   if (argc < 2)
      die("usage: %s <files>\n", argv[0]);
   mmaped_files = calloc(1, (argc)*sizeof(*mmaped_files));

   symb_add_kern("/proc/kallsyms");

   int file_index = 0;
   options.all_events = 1;
   for(i = 1; i < argc; i++) {
      if(!strcmp(argv[i], "--ignore-samples")) {
	      options.ignore_samples = 1;
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
      } else if(!strcmp(argv[i], "--temporal")) {
	      options.temporal_dump = atol(argv[++i]);
	      continue;
      } else if(!strcmp(argv[i], "--app")) {
	      options.app = argv[++i];
	      continue;
      } else if(!strcmp(argv[i], "--pid")) {
	      options.pid = 1;
	      continue;
      } else if(!strcmp(argv[i], "--tid")) {
	      options.tid = 1;
	      continue;
      } else if(!strcmp(argv[i], "--ttid")) {
	      options.ttid = 1;
	      continue;
      } else if(!strcmp(argv[i], "--ltid")) {
	      options.ltid = atoi(argv[++i]);
	      continue;
      } else if(!strcmp(argv[i], "--c")) {
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
      }

      file_index++;

      /* Open and map argv[i] to memory */
      if((fd = open (argv[i], O_RDONLY)) == -1) {
         die("open %s: %s", argv[i], strerror(errno));
      } else if (fstat (fd, &sb) == -1) {
         die("fstat : %s", strerror(errno));
      } else if (!S_ISREG (sb.st_mode)) {
         die("%s is not a file\n", argv[i]);
      } else if((mmaped_files[file_index].data = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
         die("mmap: %s", strerror(errno));
      } else if (close (fd) == -1) {
         die("close: %s", strerror(errno));
      }
      mmaped_files[file_index].size = sb.st_size;
      mmaped_files[file_index].name = argv[i];

      /* Check that host is consistent so that we can load kernel symbols */
      check_host(&mmaped_files[file_index]);

      /* Get the events array */
      check_events(&mmaped_files[file_index]);
   }
   if(!options.max_time)
	   options.max_time = -1;
   for(i = 0; i < MAX_EVT; i++) {
      for(j = 0; j < MAX_CORE; j++) {
	      pid_hash[i][j] = g_hash_table_new(g_int_hash, g_int_equal);
	      tid_hash[i][j] = g_hash_table_new(g_int_hash, g_int_equal);
      }
   }

   int finished = 0;
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
   for(i = 0; i < 16; i++) {
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

   if(!options.ignore_samples && !options.temporal_dump && !options.pid && !options.tid && !options.ttid)
	   symb_print_top();

   return 0;
}
