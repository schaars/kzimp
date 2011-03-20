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
	int ignore_samples; 		/* Do not try to resolve symbols. Only print stats about files (#samples, run length, etc.) */
	uint64_t min_time;		/* Do not resolve symbols appearing before x cycles */
	uint64_t max_time;		/*                                  after           */
	int all_events;			/* Show all events (default 1); cannot be changed manually */
	int base_event;			/* Show only event #base_event */
	int nb_cpus;			/* Nb --c XX options passed */
	int cpus[MAX_CPUS];		/* List of --c values */
	int temporal_dump;		/* Show a temporal dump which can be converted to a miniprof output and gnuplotted for even more fun */
	char *app;			/* Show only samples of application 'app' */
	int pid, tid, ttid, ltid;	/* pid & tid are legacy and useless 
					   ttid = show a raw output of the perf.data.*; see .c for the printf
					   ltid = show only samples of tif 'ltid' */
} options;

__attribute__((unused)) static gpointer int_key (int key_val) {
        int *ip = g_malloc (sizeof(int));
        *ip = key_val;
        return ip;
}
#endif
