/*
 * tcp_net.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>
#include <errno.h>

#include "tcp_net.h"
#include "time.h"

int recvMsg(int s, void *buf, size_t len, uint64_t *nb_cycles)
{
  uint64_t cycle_start, cycle_stop;
  int len_tmp = 0;
  int n;

  do
  {
    rdtsc(cycle_start);
    n = recv(s, &(((char *) buf)[len_tmp]), len - len_tmp, 0);
    rdtsc(cycle_stop);
    if (n == -1)
    {
      perror("tcp_net:recv():");
      exit(-1);
    }

    *nb_cycles += cycle_stop - cycle_start;

    len_tmp = len_tmp + n;
  } while (len_tmp < len);

  return len_tmp;
}

void sendMsg(int s, void *msg, int size, uint64_t *nb_cycles)
{
  uint64_t cycle_start, cycle_stop;
  int total = 0; // how many bytes we've sent
  int bytesleft = size; // how many we have left to send
  int n = -1;

  while (total < size)
  {
    rdtsc(cycle_start);
    n = send(s, (char*) msg + total, bytesleft, 0);
    rdtsc(cycle_stop);

    if (n == -1)
    {
      perror("tcp_net:send():");
      exit(-1);
    }

    *nb_cycles += cycle_stop - cycle_start;

    total += n;
    bytesleft -= n;
  }
}
