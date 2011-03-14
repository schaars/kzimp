/*
 * paxosInside.cc
 *
 *  Created on: Mar 2, 2011
 *      Author: pl
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "comm_mech/ipc_interface.h"
#include "Checkpointer.h"


int nb_nodes = 3; // this counts the number of paxos nodes
uint64_t nb_iter = 10; // number of snapshots to be taken before exiting
uint64_t periodic_chkpt = 1000; // periodic checkpointing in ms
uint64_t periodic_snapshot = 10000; // periodic snapshot taking in ms
int *associated_core; // associated_core[i] = core on which you launch node i, for all the nodes

// read the configuration file
// format :
//    nb nodes
//    core associated to that node, 1 line per node
void read_config_file(char *config)
{
  FILE *config_file = fopen(config, "r");
  if (!config_file)
  {
    fprintf(stderr, "configuration file %s not found.\n", config);
    exit(-1);
  }

  fscanf(config_file, "%i", &nb_nodes);
  fscanf(config_file, "%lu", &nb_iter);
  fscanf(config_file, "%lu", &periodic_chkpt);
  fscanf(config_file, "%lu", &periodic_snapshot);

  associated_core = (int*) malloc(sizeof(int) * nb_nodes);
  if (!associated_core)
  {
    perror("Associated core malloc has failed! ");
    exit(errno);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    fscanf(config_file, "%i", &associated_core[i]);
  }

  fclose(config_file);

  FILE *results_file = fopen(LOG_FILE, "a");
  if (!results_file)
  {
    fprintf(stderr, "Unable to open %s in append mode.\n", LOG_FILE);
    exit(-1);
  }

  printf("===== CONFIGURATION =====\n");
  fprintf(results_file, "===== CONFIGURATION =====\n");

  printf("Nb nodes: %i\n", nb_nodes);
  fprintf(results_file, "Nb paxos nodes: %i\n", nb_nodes);

  printf("Nb iterations: %lu\n", nb_iter);
  fprintf(results_file, "Nb iterations: %lu\n", nb_iter);

  printf("Checkpoint interval: %lu\n", periodic_chkpt);
  fprintf(results_file, "Checkpoint interval: %lu\n", periodic_chkpt);

  printf("Snapshot interval: %lu\n", periodic_snapshot);
  fprintf(results_file, "Snapshot interval: %lu\n", periodic_snapshot);

  printf("Core association:");
  fprintf(results_file, "Core association:");

  for (int i = 0; i < nb_nodes; i++)
  {
    printf("\tn%i -> c%i", i, associated_core[i]);
    fprintf(results_file, "\tn%i -> c%i", i, associated_core[i]);
  }

  printf("\n=========================\n\n");
  fprintf(results_file, "\n=========================\n\n");

  fclose(results_file);
}

void print_help_and_exit(char *program_name)
{
  fprintf(stderr, "Usage: %s config_file\n", program_name);
  exit(-1);
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    print_help_and_exit(argv[0]);
  }
  else
  {
    read_config_file(argv[1]);
  }

  IPC_initialize(nb_nodes);

  fflush(NULL);
  sync();

  // create them (with fork)
  int core_id = 0;
  for (int i = 1; i < nb_nodes; i++)
  {
    if (!fork())
    {
      core_id = i;
      break; // i'm a child, so I exit the loop
    }
  }

  // set affinity to 1 core
  cpu_set_t mask;

  CPU_ZERO(&mask);
  CPU_SET(associated_core[core_id], &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
    printf("[core %i] Error while calling sched_setaffinity()\n", core_id);
    perror("");
  }

  IPC_initialize_node(core_id);

  Checkpointer *c = new Checkpointer(core_id, nb_nodes, nb_iter, periodic_chkpt
      * 1000, periodic_snapshot * 1000);

  c->run();

  delete c;

  IPC_clean_node();

  // wait for my children
  if (core_id == 0)
  {
    int status;

    for (int i = 0; i < nb_nodes; i++)
    {
      wait(&status);
    }

    IPC_clean();
  }

  return 0;
}
