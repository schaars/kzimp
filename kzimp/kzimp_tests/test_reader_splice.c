#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>

#include <sys/uio.h>

#define MAX_MSG_SIZE 10240
#define CHAN_SIZE 10
#define NB_MSG 10

// IOCTL commands
#define KZIMP_IOCTL_SPLICE_START_READ 0x7
#define KZIMP_IOCTL_SPLICE_FINISH_READ 0x8

void do_write(int fd)
{
  int i, j;
  char buffer[MAX_MSG_SIZE];

  for (j = 0; j < NB_MSG; j++)
  {
    // create the message
    printf(">>>Sending:");
    for (i = 0; i < 10; i++)
    {
      buffer[i] = 42 + i * j;
      printf(" %x", (int) buffer[i]);
    }
    printf("\n");

    // send the message
    write(fd, buffer, MAX_MSG_SIZE);
  }

  sleep(10);

  close(fd);
}

void do_read(int fd)
{
  int i, j;
  size_t msg_area_len;
  int idx;
  char *messages, *msg;
  char buffer[MAX_MSG_SIZE];

  // mmap
  msg_area_len = MAX_MSG_SIZE * CHAN_SIZE;
  messages = mmap(NULL, msg_area_len, PROT_READ, MAP_SHARED, fd, 0);
  if (messages == (void*) -1)
  {
    perror("mmap");
    exit(-1);
  }

  printf("mmap @ %p\n", messages);

  for (j = 0; j < NB_MSG; j++)
  {
    // get a message
    idx = ioctl(fd, KZIMP_IOCTL_SPLICE_START_READ);
    printf("idx=%i\n", idx);

    if (idx >= 0)
    {
      msg = messages + idx * MAX_MSG_SIZE;

      // read the message
      printf("<<<Reading:");
      for (i = 0; i < 10; i++)
      {
        printf(" %x", (int) msg[i]);
      }
      printf("\n");

      // finish reading
      ioctl(fd, KZIMP_IOCTL_SPLICE_FINISH_READ);
    }
    else if (idx == -1)
    {
      perror("ioctl");
      exit(-1);
    }
  }

  munmap(messages, msg_area_len);

  close(fd);
}

int main(void)
{
  int fd_read, fd_write;

  // we need a reader, otherwise the multicast mask is at 0
  fd_read = open("/dev/kzimp0", O_RDONLY);
  if (fd_read < 0)
  {
    perror("Failed to open channel for reading");
    return;
  }

  fd_write = open("/dev/kzimp0", O_WRONLY);
  if (fd_write < 0)
  {
    perror("Failed to open channel");
    return;
  }

  if (!fork())
  {
    printf("I am the writer, of pid %i\n", getpid());
    do_write(fd_write);
  }
  else
  {
    printf("I am the reader, of pid %i\n", getpid());
    do_read(fd_read);
  }

  return 0;
}
