#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MSG_QUEUE_NAME "/posix_msg_queue_test"

#define MSG_SIZE 8

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

  mq_send(fd, msg, msg_size, 0);

  printf("\n");
  fflush(NULL);
}

void receive_a_message(int fd, int msg_max_size_in_queue, char *prefix)
{
  int msg_size;
  char msg[msg_max_size_in_queue];
  int sum;
  int i;

  // 0make it timeout
  struct timespec abs_timeout;
  struct timeval current_time;
  gettimeofday(&current_time, 0);

  unsigned long start_sec = current_time.tv_sec;
  unsigned long start_usec = current_time.tv_usec;

  // timeout is in 10 seconds
  abs_timeout.tv_sec = current_time.tv_sec + 10;
  abs_timeout.tv_nsec = current_time.tv_usec * 1000;

  msg_size
      = mq_timedreceive(fd, msg, msg_max_size_in_queue, NULL, &abs_timeout);

  printf("%s has a message of size %i\n", prefix, msg_size);

  if (msg_size > 0)
  {
    sum = 0;
    //printf("%s Message is: [ ", prefix);
    for (i = 0; i < MSG_SIZE; i++)
    {
      //printf("%i ", msg[i]);
      sum += msg[i];
    }
    //printf("] - %i\n", sum);

    printf("%s Message is: %i\n", prefix, sum);
  }
  else
  {
    gettimeofday(&current_time, 0);
    printf("%s Timeout after %lu sec %lu usec\n", prefix, current_time.tv_sec
        - start_sec, current_time.tv_usec - start_usec);
  }

  printf("\n");
  fflush(NULL);
}

int main(void)
{
  int ret;
  int msg_max_size_in_queue;
  mqd_t fdqueue;

  // create the queue
  fdqueue = mq_open(MSG_QUEUE_NAME, O_RDWR | O_CREAT, 0666, NULL);
  if (fdqueue == -1)
  {
    perror("mq_open");
  }

  // get the attributes of the (first) queue
  struct mq_attr attr;
  ret = mq_getattr(fdqueue, &attr);
  if (!ret)
  {
    msg_max_size_in_queue = attr.mq_msgsize;
    printf(
        "A queue has been created. Attributes are:\n\tflags=%li, maxmsg=%li, msgsize=%li, curmsgs=%li\n",
        attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
  }
  else
  {
    perror("mq_getattr");
  }

  fflush(NULL);
  sync();

  // fork
  if (!fork())
  {
    // I am the son

    // should timeout
    printf("[0] Waiting for a message\n");
    receive_a_message(fdqueue, msg_max_size_in_queue, "0");

    // close the queue
    mq_close(fdqueue);
  }
  else
  {
    // commented, so that the reception will timeout
    //send_a_message(fdqueue, 1, "1");

    int status;
    wait(&status);

    // delete the queue
    mq_close(fdqueue);
    mq_unlink(MSG_QUEUE_NAME);
  }

  return 0;
}
