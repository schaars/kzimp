#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>

/*
 * Convention (see pipe(2)):
 *    -fd[0] is used to read
 *    -fd[1] is used to write
 */

/*
 * We use the vmsplice() method, which maps user memory into a pipe.
 */

/*
 * There is a strange bug on the last received message (and only on the last received one):
 * Normally, the following is displayed:
 0 Sending message: 450
 1 has a message of size 8
 1 Message is: 450

 * Sometimes there is a problem:
 0 Sending message: 450
 1 has a message of size 8
 1 Message is: -81

 * The content (and sometimes the size) of the received message are not the expected ones.
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

  struct iovec iov;

  // writing the size of the message
  iov.iov_base = &msg_size;
  iov.iov_len = sizeof(msg_size);
  vmsplice(fd, &iov, 1, 0);
  //write(fd, &msg_size, sizeof(msg_size));

  // writing the content
  iov.iov_base = &msg;
  iov.iov_len = MSG_SIZE;
  vmsplice(fd, &iov, 1, 0);

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
  int i;
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

    for (i = 0; i < 1; i++)
    {
      receive_a_message(pipe_fd1[0], "0");
      send_a_message(pipe_fd2[1], i * 2, "0");
    }

    sleep(5);

    // close the channel
    close(pipe_fd1[0]);
    close(pipe_fd2[1]);
  }
  else
  {
    // close the unnecessary fd
    close(pipe_fd1[0]);
    close(pipe_fd2[1]);

    for (i = 0; i < 1; i++)
    {
      send_a_message(pipe_fd1[1], i * 2 + 1, "1");
      receive_a_message(pipe_fd2[0], "1");
    }

    // close the channel
    close(pipe_fd1[1]);
    close(pipe_fd2[0]);
  }

  return 0;
}
