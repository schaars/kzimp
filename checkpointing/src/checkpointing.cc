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

#include "Checkpointer.h"

int nb_nodes = 3; // this counts the number of paxos nodes
int *associated_core; // associated_core[i] = core on which you launch node i, for all the nodes

// read the configuration file
// format :
//    nb paxos nodes
//    nb clients
//    nb iter per client
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

  IPC_initialize(nb_nodes, nb_clients);

  fflush(NULL);
  sync();

  // create them (with fork)
  int core_id = 0;
  for (int i = 1; i < total_nb_nodes; i++)
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

  //todo
  IPC_initialize_node(core_id);

  Checkpointer c = new Checkpointer(core_id);

  c->recv();

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
