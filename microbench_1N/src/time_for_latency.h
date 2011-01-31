#ifndef LAT_REQ_H_
#define LAT_REQ_H_

#include <stdio.h>
#include <stdint.h>

// initialize everything for logging latencies
// n is used to know the size of the structure, if it has to be known at this point
void time_for_latency_init(uint64_t _basetime, long n);

// add a new tuple
void time_for_latency_add(long n, uint64_t time);

// output the tuple we have in the file filename
void time_for_latency_output(char *filename);

// destroy the allocated structures
void time_for_latency_destroy(void);

#endif
