#ifndef _PARSER_SAMPLE_H
#define _PARSER_SAMPLE_H

#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/perf_event.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <glib.h>
#include "common.h"
#include "symbols.h"
#include "events.h"

#define MAX_CPUS 32
extern struct options {
	int ignore_samples;
	uint64_t min_time;
	uint64_t max_time;
	int base_event;
	int nb_cpus;
	int cpus[MAX_CPUS];
	int temporal_dump;
	char *app;
	int pid, tid, ttid, ltid;
} options;

__attribute__((unused)) static gpointer int_key (int key_val) {
        int *ip = g_malloc (sizeof(int));
        *ip = key_val;
        return ip;
}
#endif
