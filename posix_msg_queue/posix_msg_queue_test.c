#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MSG_QUEUE_NAME1 "/posix_msg_queue_test1"
#define MSG_QUEUE_NAME2 "/posix_msg_queue_test2"

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

  msg_size = mq_receive(fd, msg, msg_max_size_in_queue, NULL);

  printf("%s has a message of size %i\n", prefix, msg_size);

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
  int ret;
  int msg_max_size_in_queue;
  mqd_t fdqueue1, fdqueue2;

  // create the 2 queues
  fdqueue1 = mq_open(MSG_QUEUE_NAME1, O_RDWR | O_CREAT, 0666, NULL);
  fdqueue2 = mq_open(MSG_QUEUE_NAME2, O_RDWR | O_CREAT, 0666, NULL);
  if (fdqueue1 == -1 || fdqueue2 == -1)
  {
    perror("mq_open");
  }

  // get the attributes of the (first) queue
  struct mq_attr attr;
  ret = mq_getattr(fdqueue1, &attr);
  if (!ret)
  {
    msg_max_size_in_queue = attr.mq_msgsize;
    printf(
        "2 queues have been created. Attributes are:\n\tflags=%li, maxmsg=%li, msgsize=%li, curmsgs=%li\n",
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

    receive_a_message(fdqueue1, msg_max_size_in_queue, "0");
    send_a_message(fdqueue2, 0, "0");

    // close the queue
    mq_close(fdqueue1);
    mq_close(fdqueue2);
  }
  else
  {
    send_a_message(fdqueue1, 1, "1");
    receive_a_message(fdqueue2, msg_max_size_in_queue, "1");

    int status;
    wait(&status);

    // delete the queue
    mq_close(fdqueue1);
    mq_close(fdqueue2);
    mq_unlink(MSG_QUEUE_NAME1);
    mq_unlink(MSG_QUEUE_NAME2);
  }

  return 0;
}
