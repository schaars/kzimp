/*
 * common.h
 *
 *  Created on: Aug 19, 2010
 *      Author: blepers
 */

#ifndef COMMON_H_
#define COMMON_H_
#define MAX_CORE 16
#define MAX_EVT 4


#define die(msg, args...) \
do {                         \
            fprintf(stderr,"(%s,%d) " msg "\n", __FUNCTION__ , __LINE__, ##args); \
            exit(-1);                 \
         } while(0)
#define thread_die(msg, args...) \
do {                         \
            fprintf(stderr,"(%s,%d) " msg "\n", __FUNCTION__ , __LINE__, ##args); \
            pthread_exit(NULL); \
         } while(0)
typedef struct _event {
   uint64_t type;
   uint64_t config;
   uint64_t exclude_kernel;
   uint64_t exclude_user;
   uint64_t sampling_period;
   const char* name;
} event_t;

struct ip_event {
        struct perf_event_header header;
        uint64_t ip;
        uint32_t pid, tid;
	//uint64_t tid_entry;
        uint64_t time;
	//uint64_t addr;
	//uint64_t id, stream_id;
	uint32_t cpu, res; 
	//uint64_t period;
        unsigned char __more_data[];
};

struct mmap_event {
        struct perf_event_header header;
        uint32_t pid, tid;
        uint64_t start;
        uint64_t len;
        uint64_t pgoff;
        char filename[256];
};

struct fork_event {
        struct perf_event_header header;
        uint32_t pid, ppid;
        uint32_t tid, ptid;
        uint64_t time;
};

struct comm_event {
   struct perf_event_header        header;

   uint32_t                             pid, tid;
   char                            comm[];
};

#define PERF_RECORD_CUSTOM_WRITE_EVENT 15
struct write_event {
        struct perf_event_header header;
        uint32_t core, event;
        uint64_t failed;
        uint64_t time;
        char data[0];
};

#define PERF_RECORD_CUSTOM_EVENT_EVENT 16
struct new_event_event {
   struct perf_event_header header;
   event_t event;
   char name[512];
};

typedef union event_union {
        struct perf_event_header        header;
        struct ip_event                 ip;
        struct mmap_event               mmap;
        struct fork_event               fork;
        struct comm_event               comm;
        struct write_event              write_event;
        struct new_event_event          new_event;
} sample_event_t;

#ifdef __x86_64__
#define rdtscll(val) {                                           \
    unsigned int __a,__d;                                        \
    asm volatile("rdtsc" : "=a" (__a), "=d" (__d));              \
    (val) = ((unsigned long)__a) | (((unsigned long)__d)<<32);   \
}
#else
   #define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif


#endif /* COMMON_H_ */
