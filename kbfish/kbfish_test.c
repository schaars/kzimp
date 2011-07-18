#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>

#define MSG_SIZE 64
#define NB_MSG 20
#define NB_CHANNELS 10

void do_reader(void)
{
  int i, v, n;
  int ret;
  int fd[NB_CHANNELS];
  char msg[MSG_SIZE];
  char chaname[50];
  fd_set set_read;
  struct timeval tv;

  // open all the channels, receiver side
  for (i = 0; i < NB_CHANNELS; i++)
  {
    snprintf(chaname, 50, "/dev/kbfish%i", i);
    fd[i] = open(chaname, O_RDWR | O_CREAT);
    if (fd[i] < 0)
    {
      perror("Reader open");
      printf(">>> Error while opening channel %i\n", i);
    }
  }

  printf("I am the reader. I am gonna recv %i messages on %i channels\n",
      NB_MSG, NB_CHANNELS);

  // while not has received NB_MSG_TO_SEND
  n = 0;
  while (n < NB_MSG)
  {
    // select on them
    FD_ZERO(&set_read);

    tv.tv_sec = 1; // timeout in seconds
    tv.tv_usec = 0;

    int max = 0;
    for (i = 0; i < NB_CHANNELS; i++)
    {
      FD_SET(fd[i], &set_read);
      max = ((fd[i] > max) ? fd[i] : max);
    }

    /*
    for (i = 0; i < NB_CHANNELS; i++)
    {
      // read the message and print it + the channel from which it comes
      ret = read(fd[i], (void*) &v, sizeof(v));
      if (ret == -1)
      {
        perror("read error");
      }
      else if (ret > 0)
      {
        printf(">>> %i: %i from %i\n", n, v, i);
        n++;
      }
    }
    */

    //select
    //printf(">>> %i: select\n", n);
    ret = select(max + 1, &set_read, NULL, NULL, &tv);
    if (ret)
    {
      for (i = 0; i < NB_CHANNELS; i++)
      {
        if (FD_ISSET(fd[i], &set_read))
        {
          // read the message and print it + the channel from which it comes
          ret = read(fd[i], (void*) &v, sizeof(v));
          if (ret == -1)
          {
            perror("read error");
          }
          else if (ret > 0)
          {
            printf(">>> %i: %i from %i\n", n, v, i);
            n++;
          }
          else
          {
            printf(">>> nothing to read\n");
          }
        }
      }
    }
  }

  // close
  for (i = 0; i < NB_CHANNELS; i++)
  {
    close(fd[i]);
  }
}

void do_writer(void)
{
  int i, v, n;
  int ret;
  int fd[NB_CHANNELS];
  char msg[MSG_SIZE];
  char chaname[50];

  srand(time(NULL));

  // open all the channels in read-only
  for (i = 0; i < NB_CHANNELS; i++)
  {
    snprintf(chaname, 50, "/dev/kbfish%i", i);
    fd[i] = open(chaname, O_RDWR);
    if (fd[i] < 0)
    {
      perror("Writer open");
      printf("<<< Error while opening channel %i\n", i);
    }
  }

  // while not has received NB_MSG_TO_SEND
  for (n = 0; n < NB_MSG; n++)
  {
    // use random channel
    i = rand() % NB_CHANNELS;

    // write the message
    v = n * i;
    printf("<<< %i: %i to %i\n", n, v, i);
    ret = write(fd[i], (void*) &v, sizeof(v));
    if (ret < 0)
    {
      perror("write error");
    }

    // sleep 1
    //sleep(1);
  }

  // close
  for (i = 0; i < NB_CHANNELS; i++)
  {
    close(fd[i]);
  }
}

int main(void)
{
#ifdef READER
  do_reader();
#endif

#ifdef WRITER
  do_writer();
#endif

  return 0;
}
