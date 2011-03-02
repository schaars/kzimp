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

#ifdef INET_SYSCALLS_MEASUREMENT
uint64_t nb_syscalls_send;
uint64_t nb_syscalls_recv;
#endif

int recvMsg(int s, void *buf, size_t len, uint64_t *nb_cycles)
{
#ifdef COMPUTE_CYCLES
  uint64_t cycle_start, cycle_stop;
#endif

  int len_tmp = 0;
  int n;

  do
  {
#ifdef COMPUTE_CYCLES
    rdtsc(cycle_start);
#endif

    n = recv(s, &(((char *) buf)[len_tmp]), len - len_tmp, 0);

#ifdef COMPUTE_CYCLES
    rdtsc(cycle_stop);
    *nb_cycles += cycle_stop - cycle_start;
#endif

    if (n == -1)
    {
      perror("tcp_net:recv():");
      exit(-1);
    }

#ifdef INET_SYSCALLS_MEASUREMENT
    nb_syscalls_recv++;
#endif

    len_tmp = len_tmp + n;
  } while (len_tmp < len);

  return len_tmp;
}

void sendMsg(int s, void *msg, int size, uint64_t *nb_cycles)
{
#ifdef COMPUTE_CYCLES
  uint64_t cycle_start, cycle_stop;
#endif

  int total = 0; // how many bytes we've sent
  int bytesleft = size; // how many we have left to send
  int n = -1;

  while (total < size)
  {
#ifdef COMPUTE_CYCLES
    rdtsc(cycle_start);
#endif

    n = send(s, (char*) msg + total, bytesleft, 0);

#ifdef COMPUTE_CYCLES
    rdtsc(cycle_stop);
    *nb_cycles += cycle_stop - cycle_start;
#endif

    if (n == -1)
    {
      perror("tcp_net:send():");
      exit(-1);
    }

#ifdef INET_SYSCALLS_MEASUREMENT
    nb_syscalls_send++;
#endif

    total += n;
    bytesleft -= n;
  }
}
