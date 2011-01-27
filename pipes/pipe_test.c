#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * Convention (see pipe(2)):
 *    -fd[0] is used to read
 *    -fd[1] is used to write
 */

#define MSG_SIZE 8

void send_a_message(int fd, int v, char *prefix)
{
  int msg_size = MSG_SIZE;
  char msg[MSG_SIZE];
  int sum;
  int i;

  for (i = 0; i < MSG_SIZE - 1; i++)
    msg[i] = (char) ((float) (i + 10 * v) * 1.5);

  sum = 0;
  //printf("%s Sending message: [ ", prefix);
  for (i = 0; i < MSG_SIZE - 1; i++)
  {
    //printf("%i ", msg[i]);
    sum += msg[i];
  }
  //printf("] - %i\n", sum);

  printf("%s Sending message: %i\n", prefix, sum);

  // writing the size of the message
  write(fd, &msg_size, sizeof(int));

  // writing the content
  write(fd, msg, MSG_SIZE);

  printf("\n");
  fflush(NULL);
}

void receive_a_message(int fd, char *prefix)
{
  int msg_size;
  char msg[MSG_SIZE];
  int sum;
  int i;

  read(fd, &msg_size, sizeof(int));

  printf("%s has a message of size %i\n", prefix, msg_size);

  read(fd, msg, msg_size);

  sum = 0;
  //printf("%s Message is: [ ", prefix);
  for (i = 0; i < MSG_SIZE - 1; i++)
  {
    //printf("%i ", msg[i]);
    sum += msg[i];
  }
  //printf("] - %i\n", sum);

  printf("%s Message is: %i\n", prefix, sum);

  printf("\n");
  fflush(NULL);
}

int main(void)
{
  int pipe_fd1[2]; // father writes -> recv son
  int pipe_fd2[2]; // son writes    -> recv father

  if (pipe(pipe_fd1) == -1 || pipe(pipe_fd2) == -1)
  {
    perror("pipe");
    exit(-1);
  }

  fflush(NULL);
  sync();

  // fork
  if (!fork())
  {
    // I am the son, receive messages

    // close the unnecessary fd
    close(pipe_fd1[1]);
    close(pipe_fd2[0]);

    receive_a_message(pipe_fd1[0], "0");
    send_a_message(pipe_fd2[1], 0, "0");

    // close the channel
    close(pipe_fd1[0]);
    close(pipe_fd2[1]);
  }
  else
  {
    // close the unnecessary fd
    close(pipe_fd1[0]);
    close(pipe_fd2[1]);

    send_a_message(pipe_fd1[1], 1, "1");
    receive_a_message(pipe_fd2[0], "1");

    // close the channel
    close(pipe_fd1[1]);
    close(pipe_fd2[0]);
  }

  return 0;
}
