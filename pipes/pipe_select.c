/*
 * pipe_select.c
 * How to use select with pipes.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>

#define MSG_SIZE 8

int my_pipe[2];
int core_id;

void do_producer(void)
{
  char msg[MSG_SIZE];

  // the producer never reads from the pipe
  close(my_pipe[0]);

  sleep(5);

  // send a message
  memset(msg, 3, MSG_SIZE);

  printf("Producer is sending a message...\n");
  write(my_pipe[1], msg, MSG_SIZE);

  sleep(5);

  close(my_pipe[1]);
}

void do_consumer(void)
{
  char msg[MSG_SIZE];
  fd_set file_descriptors; // set of file descriptors to listen to
  struct timeval listen_time; //max time to wait for something readable in the file descriptors

  // the consumer never writes to the pipe
  close(my_pipe[1]);

  while (1)
  {
    FD_ZERO(&file_descriptors);
    FD_SET(my_pipe[0], &file_descriptors);

    listen_time.tv_sec = 1;
    listen_time.tv_usec = 0;

    select(my_pipe[0] + 1, &file_descriptors, NULL, NULL, &listen_time);

    if (FD_ISSET(my_pipe[0], &file_descriptors))
    {
      int size = read(my_pipe[0], msg, MSG_SIZE);
      printf("Consumer has received a message of size %i: %i\n", size, msg[0]);
      break;
    }
    else
    {
      printf("Consumer does not have any message yet :(\n");
    }
  }

  close(my_pipe[0]);
}

int main(void)
{
  pipe(my_pipe);

  fflush(NULL);
  sync();

  // fork in order to create the child
  core_id = 0;
  if (!fork())
  {
    core_id = 1;
  }

  if (core_id == 0)
  {
    do_producer();
  }
  else
  {
    do_consumer();
  }
  return 0;
}
