#include "profiler.h"
//#define MMAP_SIZE               (128)
#define MMAP_SIZE               (64)

#define CPU_IN_IP_EVENTS 1


// Events to look for
// 0 -> several stuff but memory access
// 1 -> memory access, L3 cache stuff
// 2 -> memory access, L1D and L2 cache stuff
#define TYPE_OF_EVENTS 0

int ncpus;
int callgraph = 0;
static event_t default_events[] = {
#if TYPE_OF_EVENTS==0
    /********** Set of counters for several stuff but memory accesses **********/
  {
    .name = "CLK_UNHALTED",
    .type = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_CPU_CYCLES,
    .sampling_period = 1000000,
    .exclude_user = 0,
  },
  {
    .name = "INSTRUCTIONS",
    .type = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_INSTRUCTIONS,
    .sampling_period = 1000000,
    .exclude_user = 0,
  },
//  {
//  .name = "UNHALTED_CPU_CLK",
//    .type = PERF_TYPE_RAW,
//    .config = 0x000000003c,
//    .sampling_period = 100000,
//    .exclude_user = 0,
//  },
  {
    .name = "CACHE_MISSES",
    .type = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_CACHE_MISSES,
    .sampling_period = 1000000,
    .exclude_user = 0,
  },
//  {
//    .name = "CONTEXT_SWITCHES",
//    .type = PERF_TYPE_SOFTWARE,
//    .config = PERF_COUNT_SW_CONTEXT_SWITCHES,
//    .sampling_period = 1000000,
//    .exclude_user = 0,
//  },

#elif TYPE_OF_EVENTS == 1
  /********** Set of counters for memory accesses, L3 cache stuff **********/
  {
  .name = "UNC_LLC_HITS.ANY",
  .type = PERF_TYPE_RAW,
  .config = 0x0000000308,
  .sampling_period = 10000,
  .exclude_user = 0,
  },
  {
  .name = "UNC_LLC_HITS.PROBE",
  .type = PERF_TYPE_RAW,
  .config = 0x0000000408,
  .sampling_period = 10000,
  .exclude_user = 0,
  },
  {
  .name = "UNC_LLC_MISS.ANY",
  .type = PERF_TYPE_RAW,
  .config = 0x0000000309,
  .sampling_period = 10000,
  .exclude_user = 0,
  },
  {
  .name = "UNC_LLC_MISS.PROBE",
  .type = PERF_TYPE_RAW,
  .config = 0x0000000409,
  .sampling_period = 10000,
  .exclude_user = 0,
  },

#elif TYPE_OF_EVENTS == 2
  /********** Set of counters for memory accesses, L1D and L2 cache stuff **********/

  // L1D_MISSES = MEM_INST_RETIRED.LOADS - MEM_LOAD_RETIRED.L1D_HIT
  {
  .name = "MEM_INST_RETIRED.LOADS",
  .type = PERF_TYPE_RAW,
  .config = 0x000000010B,
  .sampling_period = 100000,
  .exclude_user = 0,
  },
  {
  .name = "MEM_LOAD_RETIRED.L1D_HIT",
  .type = PERF_TYPE_RAW,
  .config = 0x00000001CB,
  .sampling_period = 100000,
  .exclude_user = 0,
  },

  // L2_HITS = L2_RQSTS.REFERENCES - L2_RQSTS.MISS
  {
  .name = "L2_RQSTS.MISS",
  .type = PERF_TYPE_RAW,
  .config = 0x000000AA24,
  .sampling_period = 100000,
  .exclude_user = 0,
  },
  {
  .name = "L2_RQSTS.REFERENCES",
  .type = PERF_TYPE_RAW,
  .config = 0x000000FF24,
  .sampling_period = 100000,
  .exclude_user = 0,
  },
#endif
};


static int nb_events = sizeof(default_events) / sizeof(*default_events);
static event_t *events = default_events;
pdata_t **datas;

/**
 * Taken from Ingo: create COMM events (= pids available on the system) and MMAP events (symbols associated with pids) for tasks which are already spawned in the system.
 */
#define ALIGN(x, a)             __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)   (((x)+(mask))&~(mask))
static pid_t pid_synthesize_comm_event(pid_t pid, FILE *log) {
   struct comm_event *comm_ev = malloc(512);
   char filename[512];
   char bf[512];
   FILE *fp;
   size_t size = 0;
   pid_t tgid = 0;

   snprintf(filename, sizeof(filename), "/proc/%d/status", pid);
   fp = fopen(filename, "r");
   if (fp == NULL) { /* We raced with a task exiting - just return: */
      return 0;
   }

   memset(comm_ev, 0, 512);
   while (!comm_ev->comm[0] || !comm_ev->pid) {
      if (fgets(bf, sizeof(bf), fp) == NULL)
         goto out_failure;
      if (memcmp(bf, "Name:", 5) == 0) {
         char *name = bf + 5;
         while (*name && (*name == ' ' || *name =='\t'))
            ++name;
         char *end = name;
         while(*end && *end != '\n') {
            comm_ev->comm[end-name] = *end;
            ++end;
         }
         comm_ev->comm[end-name] = '\0';
         size = end-name+1;
      }
      else if (memcmp(bf, "Tgid:", 5) == 0) {
         char *tgids = bf + 5;
         while (*tgids && *tgids == ' ')
            ++tgids;
         tgid = comm_ev->pid = atoi(tgids);
      }
   }

   comm_ev->header.type = PERF_RECORD_COMM;
   comm_ev->header.size = sizeof(comm_ev->header) + sizeof(comm_ev->pid) + sizeof(comm_ev->tid) + size;
   comm_ev->tid = pid;
   size_t written = fwrite(comm_ev, 1, comm_ev->header.size, log);
   assert(written == comm_ev->header.size);

   fclose(fp);
   return tgid;

   out_failure: fprintf(stderr, "couldn't get COMM and pgid, malformed %s\n", filename);
   exit(EXIT_FAILURE);
}

static void pid_synthesize_mmap_samples(pid_t pid, FILE *log) {
   char filename[PATH_MAX];
   FILE *fp;

   snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
   fp = fopen(filename, "r");
   if (fp == NULL) {  /* We raced with a task exiting - just return: */
      return;
   }
   while (1) {
      char bf[BUFSIZ], *pbf = bf;
      struct mmap_event mmap_ev = { .header = {.type = PERF_RECORD_MMAP}, .pgoff = 0 };
      int n;
      size_t size;
      if (fgets(bf, sizeof(bf), fp) == NULL)
         break;

      /* 00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat */
      n = hex2u64b(pbf, &mmap_ev.start);
      if (n < 0)
         continue;
      pbf += n + 1;
      n = hex2u64b(pbf, &mmap_ev.len);
      if (n < 0)
         continue;
      pbf += n + 3;
      if (*pbf == 'x') { /* vm_exec */
         char *execname = strchr(bf, '/');

         /* Catch VDSO */
         if (execname == NULL)
            execname = strstr(bf, "[vdso]");

         if (execname == NULL)
            continue;

         size = strlen(execname);
         execname[size - 1] = '\0'; /* Remove \n */
         memcpy(mmap_ev.filename, execname, size);
         size = ALIGN(size, sizeof(uint64_t));
         mmap_ev.len -= mmap_ev.start;
         mmap_ev.header.size = (sizeof(mmap_ev) - (sizeof(mmap_ev.filename)
                  - size));
         mmap_ev.pid = pid;
         mmap_ev.tid = pid;

         size_t written = fwrite(&mmap_ev, 1, mmap_ev.header.size, log);
         assert(written == mmap_ev.header.size);
      }
   }

   fclose(fp);
}

static void synthesize_all(FILE *log) {
   /* Write the header */
   struct write_event new_evt;
   new_evt.header.type = PERF_RECORD_CUSTOM_WRITE_EVENT;
   new_evt.header.size = sizeof(new_evt);
   new_evt.core = 0;
   new_evt.event = 0;
   new_evt.failed = 0;
   new_evt.time = 0; /* We want this to be the first event, really */
   size_t written = fwrite(&new_evt, 1, sizeof(new_evt), log);
   assert(written == sizeof(new_evt));

   DIR *proc = opendir("/proc");
   struct dirent dirent, *next;
   while (!readdir_r(proc, &dirent, &next) && next) {
      char *end;
      pid_t pid;

      pid = strtol(dirent.d_name, &end, 10);
      if (*end) /* only interested in proper numerical dirents */
         continue;

      pid_synthesize_comm_event(pid, log);
      pid_synthesize_mmap_samples(pid, log);
   }
   closedir(proc);
}
/* End Ingo powerfullness */



static uint64_t mmap_read(int core, int event_idx, void *base, uint64_t old_index, uint64_t *old_time, FILE *fd_dump) {
   int mask_mmap = MMAP_SIZE * PAGE_SIZE - 1;
   struct perf_event_mmap_page *header = base; /* Header of the mmaped area */
   unsigned char *data = base + PAGE_SIZE; /* Actual sample area */
   uint64_t head = header->data_head; /* Last index of valid data */
   uint64_t old = old_index; /* Index where we stopped last time */
   int diff = head - old;
   unsigned char *buf;
   size_t written;
   long unsigned int total = 0;

   /* Write the header */
   struct write_event new_evt;
   new_evt.header.type = PERF_RECORD_CUSTOM_WRITE_EVENT;
   new_evt.header.size = sizeof(new_evt);
   new_evt.core = core;
   new_evt.event = event_idx;
   new_evt.failed = (diff < 0);
   rdtscll(new_evt.time);
   written = fwrite(&new_evt, 1, sizeof(new_evt), fd_dump);
   assert(written == sizeof(new_evt));

   /* If we failed to keep up with data rate, ignore the whole map. */
   if (diff < 0) {
      old = head;
      goto end_mmap;
   }

   /* Else, dump everything to disk */
   int size = head - old;
   if ((old & mask_mmap) + size != (head & mask_mmap)) {
      buf = &data[old & mask_mmap];
      size = mask_mmap + 1 - (old & mask_mmap);
      old += size;

      total += size;
      written = fwrite(buf, 1, size, fd_dump);
      assert(written == size);
   }

   buf = &data[old & mask_mmap];
   size = head - old;
   old += size;

   total += size;
   written = fwrite(buf, 1, size, fd_dump);
   assert(written == size);

   end_mmap:;
//   printf("[%d] Total written: %d\n", core, (int) total);
   /* Indicate to poll that we have read all the data so that we are not constantly waked up. */
   header->data_tail = old; /* WARNING : DO NOT REMOVE THAT LINE ! */
   return old;
}

pthread_barrier_t barrier;
static void* thread_loop(void *pdata) {
   int i;
   pdata_t *data = (pdata_t*) pdata;
   int core = data->core;
   FILE *fd_dump = data->log;
   if (data->core == 0)
      synthesize_all(fd_dump);

   /* Taskset on correct CPU */
   cpu_set_t mask;
   CPU_ZERO(&mask);
   CPU_SET(data->core, &mask);
   if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
      fprintf(stdout, "#[%d] Error: cannot set affinity ! (%s)\n", data->core, strerror(errno));
      return NULL;
   }

   /* Wait for 0 to finish the synthesize_all job */
   pthread_barrier_wait(&barrier);


   /* Malloc the arrays which will hold the descriptors data */
   struct perf_event_attr *events_attr = calloc(data->nb_events
            * sizeof(*events_attr), 1);
   void **mmaped_zones = calloc(data->nb_events, sizeof(*mmaped_zones));
   uint64_t *old_index = calloc(data->nb_events, sizeof(*old_index));
   uint64_t *old_times = calloc(data->nb_events, sizeof(*old_times));
   struct pollfd *event_array = calloc(data->nb_events + 1, sizeof(*event_array));
   data->fd = malloc(data->nb_events * sizeof(*data->fd));

   /* Init the perf events */
   for (i = 0; i < data->nb_events; i++) {
      /* Init the event attribute and open the fd. Event is disabled by default. */
      events_attr[i].size = sizeof(struct perf_event_attr);
      events_attr[i].type = data->events[i].type;
      events_attr[i].config = data->events[i].config;
      events_attr[i].exclude_kernel = data->events[i].exclude_kernel;
      events_attr[i].exclude_user = data->events[i].exclude_user;
      events_attr[i].read_format = PERF_FORMAT_TOTAL_TIME_ENABLED
               | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
      events_attr[i].sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID
 	       | PERF_SAMPLE_TIME;
#if CPU_IN_IP_EVENTS
      events_attr[i].sample_type |= PERF_SAMPLE_CPU;
#endif
      if(callgraph)
	      events_attr[i].sample_type |= PERF_SAMPLE_CALLCHAIN;
      events_attr[i].freq = 0;
      events_attr[i].sample_period = data->events[i].sampling_period;
      events_attr[i].task = 0;
      events_attr[i].mmap = !i;
      events_attr[i].comm = !i;
      events_attr[i].disabled = 1;
      data->fd[i] = sys_perf_counter_open(&events_attr[i], -1, core, -1, 0);

      if (data->fd[i] < 0) {
         if (errno == EPERM || errno == EACCES)
            thread_die("#[%d] Permission denied (are you root?)", data->core);
         thread_die("#[%d] Failed to open counter %d", core, i);
      }

      /* Read the fd to make sure it is correctly initialized */
      struct {
         uint64_t count;
         uint64_t time_enabled;
         uint64_t time_running;
         uint64_t id;
      } read_data;
      if (read(data->fd[i], &read_data, sizeof(read_data)) == -1)
         thread_die("#[%d] Unable to read perf file descriptor (%s)\n", core, strerror(errno));
      fcntl(data->fd[i], F_SETFL, O_NONBLOCK);

      /* Initialize the mmap where the samples will be dumped */
      mmaped_zones[i] = mmap(NULL, (MMAP_SIZE + 1) * PAGE_SIZE, PROT_READ
               | PROT_WRITE, MAP_SHARED, data->fd[i], 0);
      if (mmaped_zones[i] == MAP_FAILED)
         thread_die("#Mmap %d failed", i);

      event_array[i].events = POLLIN;
      event_array[i].revents = 0;
      event_array[i].fd = data->fd[i];

      ioctl(data->fd[i], PERF_EVENT_IOC_ENABLE);
      old_index[i] = 0;
   }
   event_array[data->nb_events].events = POLLIN;
   event_array[data->nb_events].revents = 0;
   event_array[data->nb_events].fd = data->pipe[0];

   /* Read the mmap areas when there is data available */
   for (;;) {
      for (i = 0; i < data->nb_events; i++) {
         old_index[i]
                  = mmap_read(core, i, mmaped_zones[i], old_index[i], &old_times[i], fd_dump);
      }
      if(event_array[data->nb_events].revents)
         break;
      int n = poll(event_array, data->nb_events+1, -1);
      if (n <= 0 && errno != EINTR)
         thread_die("#[%d] Poll failed ?! (%s)\n", core, strerror(errno));
   }

   fclose(fd_dump);
   return NULL;
}

void parse_options(int argc, char **argv) {
   int i = 1;
   event_t *evts = NULL;
   int nb_evts = 0;
   for (;;) {
      if (i >= argc)
         break;
      if (!strcmp(argv[i], "-e")) {
         if (i + 4 >= argc)
            die("Missing argument for -e NAME COUNTER KERNEL USER\n");
         evts = realloc(evts, (nb_evts + 1) * sizeof(*evts));
         evts[nb_evts].name = strdup(argv[i + 1]);
         evts[nb_evts].type = PERF_TYPE_RAW;
         evts[nb_evts].config = hex2u64(argv[i + 2]);
         evts[nb_evts].exclude_kernel = atoi(argv[i + 3]);
         evts[nb_evts].exclude_user = atoi(argv[i + 4]);
         nb_evts++;

         i += 5;
      } else if(!strcmp(argv[i], "-c")) {
         if (i + 1 >= argc)
            die("Missing argument for -c NB_CORES\n");
         ncpus = atoi(argv[i + 1]);
         i += 2;
      } else if(!strcmp(argv[i], "--callgraph") || !strcmp(argv[i], "-cg")) {
         callgraph = 1;
	 i++;
      } else if(!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
         printf("Usage: %s [-e name_of_event value_of_raw_counter excluse_kernel exclude_user]* [-c NB_CORES] [--callgraph]\n", argv[0]);
      } else {
         die("Unknown option %s\n", argv[i]);
      }
   }

   if (nb_evts != 0) {
      nb_events = nb_evts;
      events = evts;
   }
}

static void _sig_handler(int signal) {
   int i;
   for (i = 0; i < ncpus; i++) {
      int r = write(datas[i]->pipe[1], " ", 1);

      if (r == -1) {
         fprintf(stderr, "Error %i: %s\n", errno, strerror(errno));
      }
   }
   printf("#signal caught: %d\n", signal);
}

int main(int argc, char**argv) {
   int i, j, r, written;

   signal(SIGPIPE, _sig_handler);
   signal(SIGTERM, _sig_handler);
   signal(SIGINT, _sig_handler);
   parse_options(argc, argv);

   //ncpus = get_nprocs();
   ncpus = MAX_CORE;

   r = pthread_barrier_init(&barrier, NULL, ncpus);
   if (r != 0) {
      fprintf(stderr, "Error in pthread_barrier_init: %i\n", r);
   }

   char *name = malloc(512), *name_sentence = NULL, *option_sentence = NULL;
   gethostname(name, 512);
   r = asprintf(&name_sentence, "#Host %s\n", name);
   r = asprintf(&option_sentence, "#CPU_IN_IP_EVENTS %d\n", CPU_IN_IP_EVENTS);
   if (r == -1) {
      fprintf(stderr, "Error in asprintf\n");
   }
  

 printf("%s%s", name_sentence, option_sentence);
   for (i = 0; i < nb_events; i++) {
      printf("#Event %d: %s (%llx), count %llu (Exclude Kernel: %s; Exclude User: %s)\n", i, events[i].name, (long long unsigned) events[i].config, (long long unsigned) events[i].sampling_period, (events[i].exclude_kernel) ? "yes" : "no", (events[i].exclude_user) ? "yes" : "no");
   }

   pthread_t threads[ncpus];
   datas = malloc(ncpus*sizeof(*datas));
   for (i = 0; i < ncpus; i++) {
      pdata_t *data = malloc(sizeof(*data));
      datas[i] = data;
      data->core = i;
      data->nb_events = nb_events;
      data->events = events;

      char *file = NULL;
      r = asprintf(&file, "/tmp/perf.data.%d", i);
      if (r == -1) {
         fprintf(stderr, "Error in asprintf\n");
      }

      data->log = fopen(file, "w");
      if (!data->log)
         die("fopen %s failed %s", file, strerror(errno));

      r = pipe(data->pipe);
      if (r == -1) {
         fprintf(stderr, "Error %i: %s\n", errno, strerror(errno));
      }

      fwrite(name_sentence, 1, strlen(name_sentence), data->log);
      fwrite(option_sentence, 1, strlen(option_sentence), data->log);
      for (j = 0; j < nb_events; j++) {
         struct new_event_event new_evt;
         new_evt.header.type = PERF_RECORD_CUSTOM_EVENT_EVENT;
         new_evt.header.size = sizeof(new_evt);
         new_evt.event = events[j];
         strcpy(new_evt.name, new_evt.event.name);
         written = fwrite(&new_evt, 1, sizeof(new_evt), data->log);
         assert(written == sizeof(new_evt));
      }

      if (i != ncpus - 1) {
         pthread_create(&threads[i], NULL, thread_loop, data);
      }
      else {
         thread_loop(data);
      }
   }

   for (i = 0; i < ncpus - 1; i++) {
      pthread_join(threads[i], NULL);
   }

   return 0;
}
