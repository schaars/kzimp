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
#define CHAN_SIZE (1+1)
#define NB_MSG 10

// round up to a page size
#define PAGE_SIZE 4096
#define ROUND_UP_PAGE_SIZE(S) (S + (PAGE_SIZE - S % PAGE_SIZE) % PAGE_SIZE)

#define KZIMP_IOCTL_SPLICE_WRITE 0x1

void do_write(int fd)
{
  int i, j;
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

  for (j = 0; j < NB_MSG; j++)
  {
    // create the message
    printf(">>>Sending:");
    for (i = 0; i < 10; i++)
    {
      messages[next_msg_idx][i] = 42 + i*j;
      printf(" %x", messages[next_msg_idx][i]);
    }
    printf("\n");

    // send the message
    //  -arg[0]: user-space address of the message
    //  -arg[1]: offset in the big memory area
    //  -arg[2]: length
    kzimp_addr_struct[0] = (unsigned long) messages[next_msg_idx];
    kzimp_addr_struct[1] = next_msg_idx;
    kzimp_addr_struct[2] = MAX_MSG_SIZE;

    //system(
    //    "echo '==1=='; pmap $(ps -A | grep test | awk '{print $1}') | grep /dev/kzimp0");

    ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, kzimp_addr_struct);

    //system(
    //    "echo '==2=='; pmap $(ps -A | grep test | awk '{print $1}') | grep /dev/kzimp0");

    next_msg_idx = (next_msg_idx + 1) % CHAN_SIZE;
  }

  sleep(10);

  for (i = 0; i < CHAN_SIZE; i++)
  {
    munmap(messages[i], MAX_MSG_SIZE);
  }

  close(fd);
}

void do_read(int fd)
{
  int i, j;
  char buf[MAX_MSG_SIZE];

  for (j = 0; j < NB_MSG; j++)
  {
    //system(
    //    "echo '==3=='; pmap $(ps -A | grep test | awk '{print $1}') | grep /dev/kzimp0");

    // read the message
    read(fd, buf, MAX_MSG_SIZE);

    //system(
    //    "echo '==4=='; pmap $(ps -A | grep test | awk '{print $1}') | grep /dev/kzimp0");

    // display the message
    printf("<<<Reading:");
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

  if (!fork())
  {
    do_write(fd_write);
  }
  else
  {
    do_read(fd_read);
  }

  return 0;
}
