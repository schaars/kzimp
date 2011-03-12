/*
 * Checkpoint.cc
 *
 * What is a checkpoint
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include "Checkpoint.h"

Checkpoint::Checkpoint(void)
{
  init_message(sizeof(struct checkpoint), -1, ~0);
}

Checkpoint::Checkpoint(int src, uint64_t _cn)
{
  init_message(sizeof(struct checkpoint), src, _cn);
}

void Checkpoint::init_message(int len, int src, uint64_t cn)
{
  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

  rep()->len = len;
  rep()->src = src;
  rep()->cn = cn;

  printf(
      "Creating a new Checkpoint of size %i from %i with checkpoint number %lu\n",
      rep()->len, rep()->src, rep()->cn);
}

Checkpoint::~Checkpoint(void)
{
  free(msg);
}
