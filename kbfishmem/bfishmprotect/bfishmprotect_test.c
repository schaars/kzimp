/* Barrelfish communication mechanism - test */

/*
 * 07/25/11. The test is working, but you have to take care of the channel size (considering the size of a struct ump_message)
 *           when you will want to use it. The kernel module must be loaded with the proper size. Cf. the microbench scripts
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfishmprotect.h"

#define NB_MSG 20
// MESSAGE_BYTES is defined at compile time. Must be a multiple of the cache line size
#define MSG_SIZE MESSAGE_BYTES

#define MPROTECTFILE "/dev/kbfishmem0"

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

void simple_test_1_process(void)
{
  int i;
  struct ump_channel sender_to_receiver, receiver_to_sender;
  char msg[MSG_SIZE];

  // open channels
  sender_to_receiver = open_channel(MPROTECTFILE, 0, 10, MSG_SIZE, 0);
  receiver_to_sender = open_channel(MPROTECTFILE, 0, 10, MSG_SIZE, 1);

  recv_msg(&receiver_to_sender, msg, MSG_SIZE);

  // send 10 messages and then receive 10 messages
  memset(msg, getpid(), MSG_SIZE);
  for (i = 0; i < 9; i++)
  {
    printf("Going to send message %i\n", i);
    send_msg(&sender_to_receiver, msg, MSG_SIZE);
  }

  for (i = 0; i < 9; i++)
  {
    printf("Going to recv message %i\n", i);
    recv_msg(&receiver_to_sender, msg, MSG_SIZE);
  }

  /*
   // send a message
   memset(msg, getpid(), MSG_SIZE);

   printf("Going to send a message:\n");
   print_msg(msg, MSG_SIZE);

   send_msg(&sender_to_receiver, msg, MSG_SIZE);

   // recv a message
   recv_msg(&receiver_to_sender, msg, MSG_SIZE);

   printf("Has received a message:\n");
   print_msg(msg, MSG_SIZE);
   */

  sleep(2);

  // close channels
  close_channel(&receiver_to_sender);
  close_channel(&sender_to_receiver);
}

void do_reader(void)
{
  int i;
  struct ump_channel receiver_to_sender;
  char msg[MSG_SIZE];

  // open channels
  receiver_to_sender = open_channel(MPROTECTFILE, 0, 10, MSG_SIZE, 1);

  // receive 10 messages
  int snd = 0;
  for (i = 0; i < NB_MSG; i++)
  {
    printf("Going to recv message %i\n", i);
    recv_msg(&receiver_to_sender, msg, MSG_SIZE);

    if (snd)
    {
      printf("Going to send a message at %i\n", i);
      send_msg(&receiver_to_sender, msg, MSG_SIZE);
    }
    snd = !snd;
  }

  sleep(2);

  // close channels
  close_channel(&receiver_to_sender);
}

void do_writer(void)
{
  int i;
  struct ump_channel sender_to_receiver;
  char msg[MSG_SIZE];

  // open channels
  // note that the receiver should already have opened the file
  sender_to_receiver = open_channel(MPROTECTFILE, 0, 10, MSG_SIZE, 0);

  // send 10 messages
  memset(msg, getpid(), MSG_SIZE);
  int snd = 0;
  for (i = 0; i < NB_MSG; i++)
  {
    printf("Going to send message %i\n", i);
    send_msg(&sender_to_receiver, msg, MSG_SIZE);

    if (snd)
    {
      printf("Going to recv a message at %i\n", i);
      recv_msg(&sender_to_receiver, msg, MSG_SIZE);
    }
    snd = !snd;
  }

  sleep(2);

  // close channels
  close_channel(&sender_to_receiver);
}

int main(void)
{
#ifdef SIMPLE_TEST
  create_channel(MPROTECTFILE, 0);
  simple_test_1_process();

#else
  int core_id;

  create_channel(MPROTECTFILE, 0);

  fflush(NULL);
  sync();

  // fork in order to create the children
  core_id = 0;
  if (!fork())
  {
    core_id = 1;
  }

  //1 process reads, the other writes
   if (core_id == 0)
   {
     do_reader();
   }
   else
   {
     sleep(2);
     do_writer();
   }
#endif

  destroy_channel(0);

  return 0;
}
