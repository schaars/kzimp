#ifndef LAT_REQ_H_
#define LAT_REQ_H_

#include <stdio.h>
#include <stdint.h>

// initialize everything for logging latencies
void time_for_latency_init(uint64_t _basetime);

// add a new tuple
void time_for_latency_add(long n, uint64_t time);

// output the tuple we have in the FILE *F
void time_for_latency_output(FILE *F);

// destroy the allocated structures
void time_for_latency_destroy(void);

#endif
