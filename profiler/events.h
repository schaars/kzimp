#ifndef EVENTS_H
#define EVENTS_H
#include "parser-sampling.h"
#include "symbols.h"

void symb_add_pid(struct comm_event *evt);
void symb_add_map(struct mmap_event *evt);
void symb_add_fork(struct fork_event *evt);
int symb_add_sample(struct ip_event *evt, int evt_index, int core);
struct process *symb_find_pid(int pid);

void symb_print_top();
extern int total_number_of_samples[MAX_EVT];
#endif
