#ifndef SYMBOLS_H
#define SYMBOLS_H
#include "parser-sampling.h"
#include <libelf.h>
#include <gelf.h>
#include <elf.h>
#include <fcntl.h>
#include <bfd.h>
#include <limits.h>

struct symb_arr;
/**
 * A symbol = a function.
 * We store the number of samples / samples per core associated with the function,
 * the parent (ie. the file containing the function) and the pids which called the functions.
 * @pids : int -> int[MAX_EVT], stores the samples per pid.
 * @callers_ips : symbol_ptr -> int[MAX_EVT], stores the samples per caller function.
 */
struct symb {
   long long unsigned hex;
   char *name;
   int samples[MAX_EVT];
   int core_samples[MAX_CORE][MAX_EVT];
   struct symb_arr *parent;
   GHashTable *calling_apps_name;          /* Name of the applications (ie. 'httpd') which called the function */
                                           /* str -> int[MAX_EVT], count the number of samples per application */
   GHashTable *direct_callers;             /* symbols which called the function _directly_ */
                                           /* struct symb* -> struct symb_callg_container* { number of samples (ie. number of calls by the symbol) + children = next elts of the callchain } */
   GHashTable *callers;                    /* symbols which called the symbol */
                                           /* struct symb* -> int[MAX_EVT] */
   GHashTable *called_symbols;             /* symbols called _by_ this symbol */
                                           /* struct symb* -> int[MAX_EVT] */
};

struct callchain {
   uint64_t nbr;
   uint64_t ips[0];
};


/**
 * A file descriptor. Each file contains a number of functions / symbols.
 * - Entries are often sorted by hex (ie. address of functions) in order to make searches faster.
 * - Do not try to iterate on entries and use symb_find() on the structure at the same time : the order of entries may vary.
 */
struct symb_arr {
   struct symb **entries;
   size_t size, index;
   int sorted_by_hex;
   char *name;
};

/**
 * Each file / symb_arr may not be mapped at the same location for each pid. Thus we remember the virtual address mapping for each pid.
 * Each *file corresponds to a symb_arr->name.
 */
struct mmaped_zone {
   uint64_t begin, end, off;
   char *file;
   struct mmaped_zone *next;
};

struct process {
   int pid;
   char *name;
   int maps_needs_reset;
   struct mmaped_zone *maps;
   int samples[MAX_EVT];
   int failed_maps;
};

extern struct symb unknown_symbol;
struct symb* symb_new();
void symb_print(struct symb *k);
void symb_add_kern(char *file);
void symb_add_exe(char *file);

extern struct symb_arr kern_symb_arr;
extern struct symb_arr all_syms;
struct symb_arr *symb_find_exe(char *file);
struct symb* symb_find(long long unsigned ip, struct symb_arr *arr);
struct symb* symb_get(int index, struct symb_arr *arr);
void symb_sort(int (*sort_fun)(const void *a, const void *b),  struct symb_arr *arr);

#endif
