#include "events.h"
int total_number_of_samples[MAX_EVT];

/**
 * PID - Comm events allow us to do a pid->name hash
 */
GHashTable* processes_hash;

static struct mmaped_zone *symb_map_new();
static struct process *symb_new_process() {
   struct process *p = malloc(sizeof(*p));
   p->name = NULL;
   p->maps = symb_map_new();    /* Dummy */
   p->maps->file = "[dummy]";
   p->maps->begin = p->maps->end = 0;
   p->failed_maps = 0;
   p->maps_needs_reset = 0;
   return p;
}

__attribute__((unused)) static void print_process (gpointer key, gpointer value, gpointer user_data) {
   struct process *p = value;
   printf("%d: %s\n", p->pid, p->name);
   struct mmaped_zone *l = p->maps->next;
   while(l) {
      printf("%p - %p : %s \n", (void*)l->begin, (void*)l->end, l->file);
      l = l->next;
   }
}

void symb_add_pid(struct comm_event *evt) {
   struct process *process;
   if(!processes_hash)
      processes_hash = g_hash_table_new(g_int_hash, g_int_equal);

   process = symb_new_process();
   process->pid = evt->pid;
   process->name = strdup(evt->comm);
   g_hash_table_insert(processes_hash, int_key(evt->pid), process);
   //g_hash_table_foreach(processes_hash, print_process, NULL);
}

struct process *symb_find_pid(int pid) {
   if(!processes_hash)
         processes_hash = g_hash_table_new(g_int_hash, g_int_equal);
   return g_hash_table_lookup(processes_hash, int_key(pid));
}

void symb_add_fork(struct fork_event *evt) {
   struct process *process, *forked_process;
   if(evt->pid == evt->ppid)
      return;
   if(!processes_hash)
      processes_hash = g_hash_table_new(g_int_hash, g_int_equal);
   process = g_hash_table_lookup(processes_hash, &evt->ppid);
   if(!process)
      return;
   forked_process = symb_new_process();
   forked_process->pid = evt->pid;
   if(process->name)
      forked_process->name = strdup(process->name);
   /*TODO:
    * If the pid doesn't match with the parent pid, currently we do not duplicate
    * maps so that it does not create false mappings. Maybe there is a smarter way
    * to do that to avoid sample misses */
   /*forked_process->maps = process->maps;
   forked_process->maps_needs_reset = 1;*/
   g_hash_table_insert(processes_hash, int_key(evt->pid), forked_process);
}

/**
 * MAPS
 */

static struct mmaped_zone *symb_map_new() {
   struct mmaped_zone *m = malloc(sizeof(*m));
   m->next = NULL;
   return m;
}

void symb_add_map(struct mmap_event *evt) {
   struct process *p = symb_find_pid(evt->pid);
   if(!p) {
      fprintf(stderr, "#Cannot find process pid %d tid %d\n", evt->pid, evt->tid);
      return;
   }

   if(p->maps_needs_reset) {
	   p->maps = symb_map_new();
	   p->maps->file = "[dummy]";
	   p->maps->begin = p->maps->end = 0;
	   p->maps_needs_reset = 0;
   }

   struct mmaped_zone *l = p->maps;
   while(l->next && l->next->begin < evt->start)
      l = l->next;
   struct mmaped_zone *n = symb_map_new();
   n->begin = evt->start;
   n->end = n->begin + evt->len;
   n->off = evt->pgoff;
   n->file = strdup(evt->filename);
   n->next = l->next;
   l->next = n;
}

static struct mmaped_zone *symb_find_map(struct ip_event *evt) {
   struct process *p = symb_find_pid(evt->pid);
   if(!p) 
      return NULL;

   struct mmaped_zone *l = p->maps->next;
   while(l) {
      if(evt->ip >= l->begin && evt->ip <= l->end) {
         return l;
      }
      l = l->next;
   }
   p->failed_maps++;
   return NULL;
}

/**
 * SAMPLE
 */
static void symb_add_sample_pid(struct symb *s, struct ip_event *evt, int evt_index, int core) {
   if(!s->calling_apps_name)
      s->calling_apps_name = g_hash_table_new(g_str_hash, g_str_equal);
   struct process *p = symb_find_pid(evt->pid);
   char *name = "unknown";
   if(p) {
      name = p->name;
      p->samples[evt_index]++;
      if(!name)
              name = "unknown";
   }
   int *value = g_hash_table_lookup(s->calling_apps_name, name);
   if(!value) {
      value = calloc(MAX_EVT,sizeof(*value));
      g_hash_table_insert(s->calling_apps_name, name, value);
   }
   value[evt_index]++;
   /* TODO: Add a per app / per core / per event array also ? */
}

struct symb_callg_container {
   int values[MAX_EVT];
   GHashTable*  children;
};

static uint64_t __context = PERF_CONTEXT_MAX;
struct symb* ip_callchain_to_symb(struct ip_event *evt, uint64_t _ip) {
   struct symb* caller_s = NULL;
   struct mmaped_zone *l;
   struct symb_arr *f;
   struct ip_event ip;      ip.pid = evt->pid;      ip.ip = _ip;
   if (ip.ip >= PERF_CONTEXT_MAX) {
      __context = ip.ip;
      return NULL;
   }


   switch (__context) {
      case PERF_CONTEXT_HV:
         return NULL;
      case PERF_CONTEXT_KERNEL:
         caller_s = symb_find(_ip, &kern_symb_arr);
         break;
      default:
         l = symb_find_map(&ip);
         if (l) {
            f = symb_find_exe(l->file);
            if (!f)
               die("Cannot find file %s in maps", l->file);
            caller_s = symb_find(ip.ip - l->begin + l->off, f);
         }
         break;
   }
   if(!caller_s)
      caller_s = &unknown_symbol;
   return caller_s;
}

void symb_add_sample_caller(struct symb *s, struct ip_event *evt, int evt_index, int core) {
	uint64_t i;
	struct callchain *ch = (struct callchain *) (evt->__more_data);
   __context = PERF_CONTEXT_MAX;
	if(!s->direct_callers)
	   s->direct_callers = g_hash_table_new(g_direct_hash, g_direct_equal);
	if(!s->callers)
	   s->callers = g_hash_table_new(g_direct_hash, g_direct_equal);

	GHashTable* to_insert = s->direct_callers;
	for (i = 0; i < ch->nbr; i++) {
      struct symb *caller_s = ip_callchain_to_symb(evt, ch->ips[i]);
		if(!caller_s) 
			continue;

		struct symb_callg_container *value = g_hash_table_lookup(to_insert, caller_s);
		if(!value) {
			value = calloc(sizeof(*value), 1);
			value->children = g_hash_table_new(g_direct_hash, g_direct_equal);
			g_hash_table_insert(to_insert, caller_s, value);
		}
		value->values[evt_index]++;
		to_insert = value->children;

		if(!caller_s->called_symbols)
		   caller_s->called_symbols = g_hash_table_new(g_direct_hash, g_direct_equal);
		int *samples = g_hash_table_lookup(caller_s->called_symbols, s);
		if(!samples) {
		   samples = calloc(MAX_EVT, sizeof(*samples));
		   g_hash_table_insert(caller_s->called_symbols, s, samples);
		}
		samples[0]++;

		samples = g_hash_table_lookup(s->callers, caller_s);
		if(!samples) {
		   samples = calloc(MAX_EVT, sizeof(*samples));
		   g_hash_table_insert(s->callers, caller_s, samples);
		}
		samples[0]++;
	}
}

int check_callee(struct ip_event *evt) {
   int ret_value = !options.callee;
	uint64_t i;
	struct callchain *ch = (struct callchain *) (evt->__more_data);
   __context = PERF_CONTEXT_MAX;

	for (i = 0; i < ch->nbr; i++) {
      struct symb *caller_s = ip_callchain_to_symb(evt, ch->ips[i]);
		if(!caller_s) {
			//die("#Null pointer caller_s in <%s>: %s:%i\n", __FILE__, __FUNCTION__, __LINE__);
         continue;
		} else {
         char *name;
         {
            list_foreach(options.callee, name) {
               if(!strcmp(name, caller_s->name)) {
                  ret_value = 1;
                  break;
               }
            }
         }
         {
            list_foreach(options.non_callee, name) {
               if(!strcmp(name, caller_s->name)) {
                  return 0;
               }
            }
         }
      }
	}
   return ret_value;
}

int _user, _kernel;
int symb_add_sample(struct ip_event *evt, int evt_index, int core) {
   assert(core < MAX_CORE);
   assert(evt_index < MAX_EVT);

   if(evt_index != options.base_event)
      return 0;

   struct process* p;
   if (!(p = symb_find_pid(evt->pid))) {
      return 1;
   }
   if(options.app && strcmp(options.app, p->name)) {
	   return 4;
   }

   if((options.callee || options.non_callee) && !check_callee(evt))
      return 5;

   //printf("%s\n", p->name);
   struct symb *s;
   if ((evt->header.misc & PERF_RECORD_MISC_CPUMODE_MASK)
            == PERF_RECORD_MISC_KERNEL) {
      s = symb_find(evt->ip, &kern_symb_arr);
      _kernel++;
   } else {
      _user++;
      struct mmaped_zone *l = symb_find_map(evt);
      if (!l) {
         return 2;
      }
      struct symb_arr *f = symb_find_exe(l->file);
      if (!f)
         die("Cannot find file %s in maps", l->file);
      s = symb_find(evt->ip - l->begin + l->off, f);
   }
   if(s) {
      s->samples[evt_index]++;
      s->core_samples[core][evt_index]++;
      symb_add_sample_pid(s, evt, evt_index, core);
      if(evt->header.size > sizeof(*evt)) /* There is a callchain */
	      symb_add_sample_caller(s, evt, evt_index, core);
   } else {
      //printf("Symb not found %p \n", (void*)evt->ip);
      return 3;
   }
   return 0;
}

/**
 * TOP
 */
int symb_sort_by_base_event(const void *a, const void *b) {
   return (*(struct symb **)a)->samples[options.base_event] < (*(struct symb **)b)->samples[options.base_event];
}

struct symb_ch_sort_t{
   struct symb* s;
   char *name;
   uint64_t val;
};
struct symb_ch_sort_tmp {
   struct symb_ch_sort_t *data;
   int index;
   int sum;
};
__attribute__((unused)) static void top_foreach (gpointer key, gpointer value, gpointer user_data) {
   struct symb_ch_sort_tmp *tmp = user_data;
   int *val = value;
   tmp->data[tmp->index].val = val[options.base_event];
   tmp->data[tmp->index].name = strdup(key);
   tmp->sum += tmp->data[tmp->index].val;
   tmp->index++;
}
__attribute__((unused)) static void top_foreach_no_dup (gpointer key, gpointer value, gpointer user_data) {
   struct symb_ch_sort_tmp *tmp = user_data;
   int *val = value;
   tmp->data[tmp->index].val = val[options.base_event];
   tmp->data[tmp->index].s = key;
   tmp->sum += tmp->data[tmp->index].val;
   tmp->index++;
}
__attribute__((unused)) static void caller_top_foreach (gpointer key, gpointer value, gpointer user_data) {
   struct symb_ch_sort_tmp *tmp = user_data;
   struct symb* caller = (struct symb*)key;
   struct symb_callg_container *val = value;
   tmp->data[tmp->index].val = val->values[options.base_event];
   if (key==NULL) {
     //fprintf(stderr, "#Unknown symbol\n");
     tmp->data[tmp->index].name = strdup("unknown");
     tmp->data[tmp->index].s = NULL;
   } else {
     tmp->data[tmp->index].name = strdup(caller->name);
     tmp->data[tmp->index].s = caller;
   }
   tmp->sum += tmp->data[tmp->index].val;
   tmp->index++;
}
int symb_sort_by_sampl(const void *a, const void *b) {
   return ((struct symb_ch_sort_t *)a)->val < ((struct symb_ch_sort_t *)b)->val;
}

__attribute__((unused)) static void top_process (gpointer key, gpointer value, gpointer user_data) {
   struct process *p = value;
   struct process *old = *(struct process **)user_data;
   if(p->failed_maps > old->failed_maps)
      *(struct process **)user_data = p;
}

#define PADDING_CALLCHAIN 20
void _symb_print_callchain(struct symb *top_symbol, GHashTable* children, int max_depth,int depth) {
   if (depth > 0) {
      int j;
      struct symb_ch_sort_t* chdata = calloc(g_hash_table_size(children)*sizeof(*chdata), 1);
      struct symb_ch_sort_tmp chtmp;
      chtmp.data = chdata;
      chtmp.index = 0;
      chtmp.sum = 0;
      g_hash_table_foreach(children, caller_top_foreach, &chtmp);
      qsort(chtmp.data, chtmp.index, sizeof(struct symb_ch_sort_t), symb_sort_by_sampl);
      if(chtmp.index > 5)
         chtmp.index = 5;
      for(j = 0; j < chtmp.index; j++) {
         printf("%*.*s", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "");
         int k;
         for (k=0;k<max_depth-depth;k++) printf("|           ");
         double percent = (double)((double)chtmp.data[j].val/(double)chtmp.sum)*100.0;
         if(percent == 100.0)
            printf("|-- %4.0f.%% --", percent);
         else if(percent >= 10.0)
            printf("|-- %5.1f%% --", percent);
         else
            printf("|-- %5.2f%% --", percent);

         if(chtmp.data[j].s!=NULL){
           int *samples = g_hash_table_lookup(chtmp.data[j].s->called_symbols, top_symbol);
           if(samples)
             printf(" %s [%d samples / %d]\n", chtmp.data[j].name, (int)chtmp.data[j].val, samples[options.base_event]);
           else
             printf(" %s [%d samples]\n", chtmp.data[j].name, (int)chtmp.data[j].val);
         } else {
           //fprintf(stderr, "#Null pointer in <%s>: %s:%i\n", __FILE__, __FUNCTION__, __LINE__);
           printf(" %s [%d samples]\n", chtmp.data[j].name, (int)chtmp.data[j].val);
         }

         struct symb_callg_container *val = g_hash_table_lookup(children, chtmp.data[j].s);
         _symb_print_callchain(top_symbol, val->children, max_depth, depth-1);
      }
      /*
      if(chtmp.index > 5)
         chtmp.index = 5;
      for(j = 0; j < chtmp.index; j++) {
         if(top_symbol)
		    printf("%s -> %s;\n", top_symbol->name, chtmp.data[j].name);
         struct symb_callg_container *val = g_hash_table_lookup(children, chtmp.data[j].s);
         _symb_print_callchain(chtmp.data[j].s, val->children, max_depth, depth-1);
      }
      */
      free(chdata);
   }
}

void symb_print_callchain(struct symb * sym, int depth) {
   printf("%*.*sCallchain:\n", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "");
   if(sym->direct_callers) {
	   _symb_print_callchain(sym, sym->direct_callers, depth, depth);
	   //_symb_print_callchain(NULL, sym->direct_callers, depth, depth);
	   printf("\n");
   }
}

void symb_print_apps(struct symb *sym, int num) {
   printf("%*.*sApp repartition:\n", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "");
   if(sym->calling_apps_name) {
	   struct symb_ch_sort_t* data = calloc(g_hash_table_size(sym->calling_apps_name)*sizeof(*data), 1);
	   struct symb_ch_sort_tmp tmp;
	   tmp.data = data;
	   tmp.index = 0;
	   tmp.sum = 0;
	   g_hash_table_foreach(sym->calling_apps_name, top_foreach, &tmp);
	   qsort(tmp.data, tmp.index, sizeof(*data), symb_sort_by_sampl);
	   int j;
	   if(tmp.index > num)
		   tmp.index = num;
	   for(j = 0; j < tmp.index; j++) {
		   printf("%*.*s|-- %5.2f%% --  %s\n", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "", (double)((double)tmp.data[j].val/(double)tmp.sum)*100.0, tmp.data[j].name);
	   }
   }
}

void symb_print_top_func(struct symb *sym, int num) {
   printf("%*.*sTop functions:\n", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "");
   if(sym->callers) {
	   struct symb_ch_sort_t* data = calloc(g_hash_table_size(sym->callers)*sizeof(*data), 1);
	   struct symb_ch_sort_tmp tmp;
	   tmp.data = data;
	   tmp.index = 0;
	   tmp.sum = 0;
	   g_hash_table_foreach(sym->callers, top_foreach_no_dup, &tmp);
	   qsort(tmp.data, tmp.index, sizeof(*data), symb_sort_by_sampl);
	   int j;
	   if(tmp.index > num)
		   tmp.index = num;
	   for(j = 0; j < tmp.index; j++) {
		   printf("%*.*s|-- %5.2f%% --  %s\n", PADDING_CALLCHAIN, PADDING_CALLCHAIN, "", (double)((double)tmp.data[j].val/(double)sym->samples[0])*100.0, tmp.data[j].s->name);
	   }
   }
}

void symb_print_top() {
   printf("#User: %d Kernel: %d (%.2f%%)\n", _user, _kernel, 100.*((float)_kernel)/((float)_user+_kernel));
   symb_sort(symb_sort_by_base_event, &all_syms);
   int i;
   for(i = 0; i < 100; i++) {
      struct symb *sym = symb_get(i, &all_syms);
      if(sym->samples[options.base_event] == 0) {
	      printf("#WARNING: printing only %d function of the top 100 (no other function with at least 1 sample)\n", i);
	      break;
      }
      printf("%8d  %5.2f%%  %20.20s   %s\n", sym->samples[options.base_event], 100.0*((double) sym->samples[options.base_event])/ ((double)(total_number_of_samples[options.base_event])), sym->parent->name, sym->name);

      /* Show which app called the symbol */
      symb_print_apps(sym,5);

      /* Show top indirect callers */
      if(sym->callers)
         symb_print_top_func(sym,50);

      /* Callchain computation */
      if(1 && sym->direct_callers)
	      symb_print_callchain(sym, 50);
   }
   struct process *p = symb_new_process();
   g_hash_table_foreach(processes_hash, top_process, &p);
   printf("#Top misses samples in %s (%d samples)\n", p->name, p->failed_maps);
}
