#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>

//#define _GNU_SOURCE
//#include <fcntl.h>
#include <sys/uio.h>

#define MAX_MSG_SIZE 10240
#define CHAN_SIZE (10+1)
#define NB_MSG 10

// round up to a page size
#define PAGE_SIZE 4096
#define ROUND_UP_PAGE_SIZE(S) (S + (PAGE_SIZE - S % PAGE_SIZE) % PAGE_SIZE)

#define KZIMP_IOCTL_SPLICE_WRITE 0x1

void do_write(int fd)
{
  int i;
  size_t msg_area_len;
  size_t max_msg_size_rounded_up;
  char* messages[CHAN_SIZE];
  unsigned long kzimp_addr_struct[3];
  int next_msg_idx = 0;

  //max_msg_size_rounded_up = ROUND_UP_PAGE_SIZE(MAX_MSG_SIZE);
  //msg_area_len = max_msg_size_rounded_up;

  for (i = 0; i < CHAN_SIZE; i++)
  {
    messages[i] = mmap(NULL, MAX_MSG_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED,
        fd, PAGE_SIZE * i);
    if (messages[i] == (void *) -1)
    {
      perror("mmap failed");
      exit(-1);
    }
  }

  // create the message
  for (i = 0; i < 10; i++)
  {
    messages[next_msg_idx][i] = 42 + i;
  }

  // send the message
  //  -arg[0]: user-space address of the message
  //  -arg[1]: offset in the big memory area
  //  -arg[2]: length
  kzimp_addr_struct[0] = (unsigned long)messages[next_msg_idx];
  kzimp_addr_struct[1] = next_msg_idx;
  kzimp_addr_struct[2] = MAX_MSG_SIZE;

  printf("uaddr=%p, offset=%lu, count=%lu\n", (char*)kzimp_addr_struct[0], kzimp_addr_struct[1], kzimp_addr_struct[2]);

  ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, kzimp_addr_struct);

  messages[next_msg_idx][0] = 0;
  printf("No segfault?!\n");

  next_msg_idx = (next_msg_idx + 1 ) % CHAN_SIZE;

  for (i = 0; i < CHAN_SIZE; i++)
  {
    munmap(messages[i], MAX_MSG_SIZE);
  }

  // old code
#if 0
  int j;
  unsigned long offset;
  unsigned long kzimp_addr_struct[3];

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
    //  -arg[0]: user-space address of the message
    //  -arg[1]: offset in the big memory area
    //  -arg[2]: length
    kzimp_addr_struct[0] = (unsigned long)(msg_area + offset);
    kzimp_addr_struct[1] = offset;
    kzimp_addr_struct[2] = MAX_MSG_SIZE;

    printf("uaddr=%p, offset=%lu, count=%lu\n", (char*)kzimp_addr_struct[0], kzimp_addr_struct[1], kzimp_addr_struct[2]);

    printf("You have 10 seconds to pmap...\n");
    sleep(10);

    ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, kzimp_addr_struct);

    printf("You have 10 seconds to pmap...\n");
    sleep(10);

    // now that we have sent the message, modify it
    // this should segfault
    *(msg_area + offset) = 32;

    offset += max_msg_size_rounded_up;
    if (offset > msg_area_len)
    {
      offset = 0;
    }
  }
#endif

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
  //fixme: do_read(fd_read);

  return 0;
}
