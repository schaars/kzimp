/* Barrelfish communication mechanism - test */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfishmprotect.h"

void print_msg(char *msg, int len)
{
  int i;
  int *msg_as_int = (int*) msg;

  printf("[ ");
  for (i = 0; i < len / sizeof(int); i++)
  {
    printf(" %x", msg_as_int[i]);
  }
  printf(" ]\n");
}

int main(void)
{
  struct ump_channel sender_to_receiver, receiver_to_sender;
  char msg[64];

  // open channels
  sender_to_receiver = open_channel("/dev/kbfishmem0", 10, 64, 0);
  receiver_to_sender = open_channel("/dev/kbfishmem0", 10, 64, 1);

  // send a message
  memset(msg, getpid(), 64);

  printf("Going to send a message:\n");
  print_msg(msg, 64);

  send_msg(&sender_to_receiver, msg, 64);

  // recv a message
  recv_msg(&receiver_to_sender, msg, 64);

  printf("Has received a message:\n");
  print_msg(msg, 64);

  sleep(2);

  // close channels
  close_channel(&receiver_to_sender);
  close_channel(&sender_to_receiver);

  return 0;
}
