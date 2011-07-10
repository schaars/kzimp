#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Convention (see pipe(2)):
 *    -fd[0] is used to read
 *    -fd[1] is used to write
 */

/*
 * Note that this is not a good idea to rely only on signals.
 * Indeed, if a process A sends 2 identical signals to B, then it may
 * happen that B receives only 1 of them.
 */

#define MSG_SIZE 8

void receive_a_message(int fd, char *prefix);

// signal handler. We handle the SIGIO signal
int fd_for_reading;
void handle_sigio(int sig)
{
  printf("In the handler\n");

  if (sig == SIGIO)
  {
    receive_a_message(fd_for_reading, "0");
  }
}

void send_a_message(int fd, int v, char *prefix)
{
  int msg_size = MSG_SIZE;
  char msg[msg_size];
  int sum;
  int i;

  for (i = 0; i < msg_size; i++)
    msg[i] = (char) ((float) (i + 10 * v) * 1.5);

  sum = 0;
  //printf("%s Sending message: [ ", prefix);
  for (i = 0; i < msg_size; i++)
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
  for (i = 0; i < MSG_SIZE; i++)
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
  int pipe_fd[2]; // father writes -> recv son

  if (pipe(pipe_fd) == -1)
  {
    perror("pipe");
    exit(-1);
  }

  fflush(NULL);
  sync();

  // fork
  int son_pid;
  if (!(son_pid = fork()))
  {
    // I am the son, receive messages
    int flags, ret;

    // close the unnecessary fd
    close(pipe_fd[1]);

    // set O_ASYNC flag
    flags = fcntl(pipe_fd[0], F_GETFD);
    ret = fcntl(pipe_fd[0], F_SETFL, flags | O_ASYNC);
    if (ret == -1)
    {
      perror("O_ASYNC");
    }

    flags = fcntl(pipe_fd[0], F_GETOWN);
    ret = fcntl(pipe_fd[0], F_SETOWN, flags | getpid());
    if (ret == -1)
    {
      perror("F_SETOWN");
    }

    // set signal handler
    fd_for_reading = pipe_fd[0];
    signal(SIGIO, handle_sigio);

    while (1)
    {
      sleep(1);
    }

    // close the channel
    close(pipe_fd[0]);
  }
  else
  {
    // close the unnecessary fd
    close(pipe_fd[0]);

    sleep(1);

    int i;
    for (i = 0; i < 5; i++)
    {
      send_a_message(pipe_fd[1], i, "1");
      // sleep(1); <-- signals are sent at a low rate, thus it works.
    }

    int status;
    wait(&status);

    // close the channel
    close(pipe_fd[1]);
  }

  return 0;
}
