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

#ifdef SYSCALLS_MEASUREMENT
    nb_syscalls_recv++;

    //printf("recv_size= %i\n", n);
#endif

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

#ifdef SYSCALLS_MEASUREMENT
    nb_syscalls_send++;
#endif

    *nb_cycles += cycle_stop - cycle_start;

    total += n;
    bytesleft -= n;
  }
}

int recvMsg_nonBlock(int s, void *buf, size_t len)
{
  int len_tmp = 0;
  int n;

  do
  {
    n = recv(s, &(((char *) buf)[len_tmp]), len - len_tmp, 0);

    if (n == -1)
    {
      if (errno == EWOULDBLOCK || errno == EAGAIN)
      {
        break;
      }
      else
      {
        perror("tcp_net:recv():");
        exit(-1);
      }
    }

    len_tmp = len_tmp + n;
  } while (len_tmp < len);

  return len_tmp;
}
