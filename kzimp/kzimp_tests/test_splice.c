#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <sys/mman.h>

//#define _GNU_SOURCE
//#include <fcntl.h>
#include <sys/uio.h>

#define MAX_MSG_SIZE 10240
#define NB_MSG 10

#define KZIMP_IOCTL_SPLICE_WRITE 0x1

void do_write(int fd)
{
  int i, j;
  size_t msg_area_len;
  char *msg_area;
  unsigned long offset;
  struct iovec iov;

  msg_area_len = (size_t) MAX_MSG_SIZE * (size_t) NB_MSG;

  // MAP_SHARED is necessary, otherwise changes are not sent to the file but copy-on-write is used
  msg_area
      = mmap(NULL, msg_area_len, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

  bzero(msg_area, msg_area_len);

  offset = 0;
  for (j = 0; j < NB_MSG; j++)
  {
    // create the message
    for (i = 0; i < 10; i++)
    {
      *(msg_area + offset + i) = 42 + j;
    }

    // send the message
    iov.iov_base = (void*)offset;
    iov.iov_len = MAX_MSG_SIZE;
    ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, &iov);

    offset += MAX_MSG_SIZE;
    if (offset > msg_area_len)
    {
      offset = 0;
    }
  }

  munmap(msg_area, msg_area_len);

  close(fd);
}

void do_read(int fd)
{
  int i, j;
  char buf[MAX_MSG_SIZE];

  for (j = 0; j < NB_MSG; j++)
  {
    // read the message
    read(fd, buf, MAX_MSG_SIZE);

    // display the message
    printf("Reading:");
    for (i = 0; i < 10; i++)
    {
      printf(" %x", buf[i]);
    }
    printf("\n");
  }

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

  do_write(fd_write);
  do_read(fd_read);

  return 0;
}
