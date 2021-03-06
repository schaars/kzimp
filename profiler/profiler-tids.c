#include "profiler.h"
#define MMAP_SIZE               (64)

int callgraph = 0;
static event_t default_events[] = {
	/*
	{
		.name = "CLK_UNHALTED",
		.type = PERF_TYPE_RAW,
		.config = 0x00400076,
		.sampling_period = 1500000,
		.exclude_user = 0,
	},
	*/

	/*
	{
		.name = "RETIRED_INSTRUCTION",
		.type = PERF_TYPE_RAW,
		.config = 0x004000C0,
		.sampling_period = 100000,
		.exclude_user = 0,
	},*/

	{
		.name = "CPU_DRAM_NODE0",
		.type = PERF_TYPE_RAW,
		.config = 0x1004001E0,
		.sampling_period = 500,
	},
	{
		.name = "CPU_DRAM_NODE1",
		.type = PERF_TYPE_RAW,
		.config = 0x1004002E0,
		.sampling_period = 500,
	},
	{
		.name = "CPU_DRAM_NODE2",
		.type = PERF_TYPE_RAW,
		.config = 0x1004004E0,
		.sampling_period = 500,
	},
	{
		.name = "CPU_DRAM_NODE3",
		.type = PERF_TYPE_RAW,
		.config = 0x1004008E0,
		.sampling_period = 500,
	},

	/*
	{
		.name = "MEM_CPU_LOCAL",
		.type = PERF_TYPE_RAW,
		.config = 0x0040A8E9,
		.sampling_period = 550,
	},
	{
		.name = "MEM_CPU_REMOTE",
		.type = PERF_TYPE_RAW,
		.config = 0x004098E9,
		.sampling_period = 550,
	},
	*/
	/*
	{
		.name = "MEM_IO_LOCAL",
		.type = PERF_TYPE_RAW,
		.config = 0x0040A2E9,
		.sampling_period = 30,
	},
	{
		.name = "MEM_IO_REMOTE",
		.type = PERF_TYPE_RAW,
		.config = 0x004092E9,
		.sampling_period = 30,
	},*/
	/*
	{
		.name = "L3 Accesses",
		.type = PERF_TYPE_RAW,
		.config = 0x40040f7E0,
		.sampling_period = 550,
	},
	{
		.name = "L3 Misses",
		.type = PERF_TYPE_RAW,
		.config = 0x40040f7E1,
		.sampling_period = 550,
	},
	*/
	
};
#define MAX_CPUS  16
#define MAX_PIDS  2000
static pdata_t **datas;
static int nb_observed_apps;
static char** observed_apps;
static int nb_observed_pids;
static int *observed_pids;
static int nb_events = sizeof(default_events) / sizeof(*default_events);
static event_t *events = default_events;

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
	
	 if(nb_observed_apps) {
		 int i;
		 for(i = 0; i < nb_observed_apps; i++) {
			 if(!strcmp(comm_ev->comm, observed_apps[i])) {
				 printf("#Matching pid: %d (%s)\n", (int)pid, comm_ev->comm);
				 if(nb_observed_pids < MAX_PIDS) {
					 observed_pids = realloc(observed_pids, (nb_observed_pids+1)*sizeof(*observed_pids));
					 observed_pids[nb_observed_pids] = pid;
					 nb_observed_pids++;
				 } else {
					 printf("#WARN: Ignoring pid %d\n", pid);
				 }
			 }
		 }
	 } else {
		 printf("#Matching pid: %d (%s)\n", (int)pid, comm_ev->comm);
	 }
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

static void pid_synthesize_mmap_samples(pid_t pid, pid_t tid, FILE *log) {
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
         mmap_ev.tid = tid;

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

   int pid, ppid, i;
   if(nb_observed_pids == 0) {
	   char buffer[1024];
	   FILE *procs = popen("ps -A -L -o lwp= -o comm=", "r"); 
	   while(fscanf(procs, "%d %s\n", &pid, buffer) == 2) {
		   ppid = pid_synthesize_comm_event(pid, log);
		   pid_synthesize_mmap_samples(ppid, pid, log);
	   }
	   fclose(procs);
   } else {
	   for(i = 0; i < nb_observed_pids; i++) {
		   ppid = pid_synthesize_comm_event(observed_pids[i], log);
		   pid_synthesize_mmap_samples(ppid, observed_pids[i], log);
	   }
   }
}
/* End Ingo powerfullness */



static uint64_t mmap_read(int core, int event_idx, void *base, uint64_t old_index, uint64_t *old_time, FILE *fd_dump, int tid) {
   int mask_mmap = MMAP_SIZE * PAGE_SIZE - 1;
   struct perf_event_mmap_page *header = base; /* Header of the mmaped area */
   unsigned char *data = base + PAGE_SIZE; /* Actual sample area */
   uint64_t head = header->data_head; /* Last index of valid data */
   uint64_t old = old_index; /* Index where we stopped last time */
   int diff = head - old;
   unsigned char *buf;
   size_t written;

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
   long unsigned int total = 0;
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
   printf("[%d ev %d core %d] Total written: %d\n", tid, event_idx, core, (int) total);
   /* Indicate to poll that we have read all the data so that we are not constantly waked up. */
   header->data_tail = old; /* WARNING : DO NOT REMOVE THAT LINE ! */
   return old;
}

pthread_barrier_t barrier;
static void* thread_loop(void *pdata) {
   int i,j;
   pdata_t *data = (pdata_t*) pdata;
   FILE *fd_dump = data->log;
   int tid = data->tid;
   int nb_cpus = 1;

   /* Malloc the arrays which will hold the descriptors data */
   struct perf_event_attr *events_attr = calloc(data->nb_events, sizeof(*events_attr));
   void **mmaped_zones = calloc(nb_cpus*data->nb_events, sizeof(*mmaped_zones));
   uint64_t *old_index = calloc(nb_cpus*data->nb_events, sizeof(*old_index));
   uint64_t *old_times = calloc(nb_cpus*data->nb_events, sizeof(*old_times));
   struct pollfd *event_array = calloc(nb_cpus*data->nb_events + 1, sizeof(*event_array));
   data->fd = malloc(nb_cpus * data->nb_events * sizeof(*data->fd));

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
   }

   //for (j = 0; j < nb_cpus; j++) {
   j = 0; {
	   int group_fd = -1;
	   for (i = 0; i < data->nb_events; i++) {
		   int fd_index = j*data->nb_events + i;
		   data->fd[fd_index] = sys_perf_counter_open(&events_attr[i], tid, -1, group_fd, 0);
		   /*if(group_fd == -1)
			   group_fd = data->fd[fd_index];*/

		   if (data->fd[fd_index] < 0) {
			   if (errno == EPERM || errno == EACCES)
				   thread_die("#[%d] Permission denied (are you root?)", tid);
			   thread_die("#[%d] Failed to open counter %d", tid, fd_index);
		   }

		   /* Read the fd to make sure it is correctly initidalized */
		   struct {
			   uint64_t count;
			   uint64_t tidme_enabled;
			   uint64_t tidme_running;
			   uint64_t i;
		   } read_data;
		   if (read(data->fd[fd_index], &read_data, sizeof(read_data)) == -1)
			   thread_die("#[%d] Unable to read perf file descriptor (%s)\n", tid, strerror(errno));
		   fcntl(data->fd[fd_index], F_SETFL, O_NONBLOCK);

		   /* Initidalize the mmap where the samples will be dumped */
		   mmaped_zones[fd_index] = mmap(NULL, (MMAP_SIZE + 1) * PAGE_SIZE, PROT_READ
				   | PROT_WRITE, MAP_SHARED, data->fd[fd_index], 0);
		   if (mmaped_zones[fd_index] == MAP_FAILED)
			   thread_die("#[%d] Mmap %d failed", tid, fd_index);

		   event_array[fd_index].events = POLLIN;
		   event_array[fd_index].revents = 0;
		   event_array[fd_index].fd = data->fd[fd_index];

		   ioctl(data->fd[fd_index], PERF_EVENT_IOC_ENABLE);
		   old_index[fd_index] = 0;
	   }
   }
   event_array[data->nb_events*nb_cpus].events = POLLIN;
   event_array[data->nb_events*nb_cpus].revents = 0;
   event_array[data->nb_events*nb_cpus].fd = data->pipe[0];

   /* Read the mmap areas when there is data available */
   for (;;) {
      j = 0; { //for (j = 0; j < nb_cpus; j++) {
	   	   for (i = 0; i < data->nb_events; i++) {
	   		   int fd_index = j*data->nb_events + i;
	   		   old_index[fd_index] = mmap_read(j, i, mmaped_zones[fd_index], old_index[fd_index], &old_times[fd_index], fd_dump, tid); //20=fake CPU
	   	   }
      }
      if(event_array[data->nb_events*nb_cpus].revents)
         break;
      int n = poll(event_array, data->nb_events*nb_cpus+1, -1);
      if (n <= 0 && errno != EINTR)
         thread_die("#[%d] Poll failed ?! (%s)\n", tid, strerror(errno));
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
      } else if(!strcmp(argv[i], "--callgraph") || !strcmp(argv[i], "-cg")) {
         callgraph = 1;
	 i++;
      } else if(!strcmp(argv[i], "--app")) {
         observed_apps = realloc(observed_apps, (nb_observed_apps+1)*sizeof(*observed_apps));
	 observed_apps[nb_observed_apps] = argv[i+1];
	 nb_observed_apps++;
	 i+=2;
      } else if(!strcmp(argv[i], "--tid")) {
         observed_pids = realloc(observed_pids, (nb_observed_pids+1)*sizeof(*observed_pids));
	 observed_pids[nb_observed_pids] = atoi(argv[i+1]);
	 nb_observed_pids++;
	 i+=2;
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
   if(nb_observed_apps == 0 && nb_observed_pids == 0) {
      die("Please specify at least an --app or a --tid to monitor...\n");
   }
}

static void _sig_handler(int signal) {
   int i;
   for (i = 0; i < nb_observed_pids; i++) {
      int r = write(datas[i]->pipe[1], " ", 1);

      if (r == -1) {
         fprintf(stderr, "Error %i: %s\n", errno, strerror(errno));
      }
   }
   printf("#signal caught: %d\n", signal);
}

int main(int argc, char**argv) {
   int i, j, r, written;
   uint64_t main_start,main_stop;

   signal(SIGPIPE, _sig_handler);
   signal(SIGTERM, _sig_handler);
   signal(SIGINT, _sig_handler);
   parse_options(argc, argv);

   char *name = malloc(512), *name_sentence = NULL, *option_sentence = NULL, *rdtscll_log;
   gethostname(name, 512);
   r = asprintf(&name_sentence, "#Host %s\n", name);
   if (r == -1) {
      fprintf(stderr, "Error in asprintf\n");
   }

   printf("%s%s\n", name_sentence, option_sentence);
   for (i = 0; i < nb_events; i++) {
      printf("#Event %d: %s (%llx), count %llu (Exclude Kernel: %s; Exclude User: %s)\n", i, events[i].name, (long long unsigned) events[i].config, (long long unsigned) events[i].sampling_period, (events[i].exclude_kernel) ? "yes" : "no", (events[i].exclude_user) ? "yes" : "no");
   }

   char *main_dump = "/home/b218/prof_dump/perf.data.0";
   FILE *log = fopen(main_dump , "w");
   if(!log)
	   die("Cannot allocate main log (%s)\n", strerror(errno));
   fwrite(name_sentence, 1, strlen(name_sentence), log);
   fwrite(option_sentence, 1, strlen(option_sentence), log);
   synthesize_all(log);
   if(nb_observed_pids == 0) {
      die("#FATAL: found not pid for monitored apps");
   }
   rdtscll(main_start);
   

   pthread_t threads[nb_observed_pids];
   datas = malloc(nb_observed_pids*sizeof(*datas));
   for (i = 0; i < nb_observed_pids; i++) {
      pdata_t *data = malloc(sizeof(*data));
      datas[i] = data;
      data->tid = observed_pids[i];
      data->nb_events = nb_events;
      data->events = events;

      char *file = NULL;
      r = asprintf(&file, "/home/b218/prof_dump/perf.data.%d", observed_pids[i]);
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

      if (i != nb_observed_pids - 1) {
         pthread_create(&threads[i], NULL, thread_loop, data);
      }
      else {
         thread_loop(data);
      }
   }

   for (i = 0; i < nb_observed_pids - 1; i++) {
      pthread_join(threads[i], NULL);
   }

   rdtscll(main_stop);
   fwrite("###END HEADER\n", 1, strlen("###END HEADER\n"), log);

   r = asprintf(&rdtscll_log, "#MAIN_LENGTH start %llu stop %llu length %llu [freq %llu]\n", (long long unsigned) main_start, (long long unsigned) main_stop, (long long unsigned) (main_stop - main_start), (long long unsigned)2493513984);
   if (r == -1) {
      fprintf(stderr, "Error in asprintf\n");
   }

   fwrite(rdtscll_log, 1, strlen(rdtscll_log), log);
   fclose(log);

   return 0;
}
