/* Barrelfish communication mechanism - test */

#include <stdio.h>
#include <unistd.h>

#include "bfishmprotect.h"

int main(void)
{
  struct ump_channel sender_to_receiver, receiver_to_sender;

  // open channels
  sender_to_receiver = open_channel("/dev/kbfishmem0", 10, 1024, 0);
  receiver_to_sender = open_channel("/dev/kbfishmem0", 10, 1024, 1);

  //todo

  // send & recv a message
  // we gonna need 2 processes for automatically sending/receiving the acks

  sleep(5);

  // close channels
  close_channel(&receiver_to_sender);
  close_channel(&sender_to_receiver);

  return 0;
}
