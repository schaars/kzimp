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

#define NB_MSG_TO_RECV 5

#define MSG_QUEUE_NAME "/posix_msg_queue_test"

#define MSG_SIZE 8

// handler of incoming messages
int nb_msg_received = 0;

void send_a_message(int fd, int v, char *prefix)
{
  int msg_size = MSG_SIZE;
  char msg[msg_size];
  int sum;
  int i;

  for (i = 0; i < msg_size - 1; i++)
    msg[i] = (char) ((float) (i + 10 * v) * 1.5);

  sum = 0;
  //printf("%s Sending message: [ ", prefix);
  for (i = 0; i < msg_size - 1; i++)
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

void receive_a_message(union sigval sv)
{
  struct mq_attr attr;
  char *msg;
  int i;
  mqd_t mqdes = *((mqd_t *) sv.sival_ptr);

  /* Determine max. msg size; allocate buffer to receive msg */
  mq_getattr(mqdes, &attr);
  msg = (char*) malloc(attr.mq_msgsize);

  mq_receive(mqdes, msg, attr.mq_msgsize, NULL);

  int sum = 0;
  //printf("%s Message is: [ ", prefix);
  for (i = 0; i < MSG_SIZE; i++)
  {
    //printf("%i ", msg[i]);
    sum += msg[i];
  }
  //printf("] - %i\n", sum);

  printf("%s Message is: %i\n", "[0]", sum);

  free(msg);
  nb_msg_received++;
}

int main(void)
{
  int ret;
  int msg_max_size_in_queue;
  mqd_t fdqueue;

  /*
   // decomment this code when you want to delete the queue
   mq_unlink(MSG_QUEUE_NAME);
   return 0;
   */

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

    struct sigevent not;
    not.sigev_notify = SIGEV_THREAD;
    not.sigev_notify_function = receive_a_message;
    not.sigev_notify_attributes = NULL;
    not.sigev_value.sival_ptr = &fdqueue; /* Arg. to thread func. */

    while (nb_msg_received < NB_MSG_TO_RECV)
    {
      printf("[0] Current status: received %i messages over %i\n",
          nb_msg_received, NB_MSG_TO_RECV);
      mq_notify(fdqueue, &not);
      sleep(5);
    }

    // close the queue
    mq_close(fdqueue);
  }
  else
  {
    sleep(5);

    int i;
    for (i = 0; i < NB_MSG_TO_RECV; i++)
    {
      send_a_message(fdqueue, i, "1");
      sleep(1);
    }

    int status;
    wait(&status);

    // delete the queue
    mq_close(fdqueue);
    mq_unlink(MSG_QUEUE_NAME);
  }

  return 0;
}
