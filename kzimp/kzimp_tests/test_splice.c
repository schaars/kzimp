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

#define PAGE_SIZE 4096
#define ROUND_UP_PAGE_SIZE(S) (S + (PAGE_SIZE - S % PAGE_SIZE) % PAGE_SIZE)

#define KZIMP_IOCTL_SPLICE_WRITE 0x1

void do_write(void)
{
  int fd;
  size_t msg_area_len;
  char *msg_area;
  char buf[MAX_MSG_SIZE];

  fd = open("/dev/kzimp0", O_WRONLY);
  if (fd < 0)
  {
     perror("Failed to open channel");
     return;
  }

  msg_area_len = (size_t)ROUND_UP_PAGE_SIZE(MAX_MSG_SIZE) * (size_t)NB_MSG;
  
  // MAP_SHARED is necessary, otherwise changes are not sent to the file but copy-on-write is used
  msg_area = mmap(NULL, msg_area_len, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

  bzero(msg_area, msg_area_len); 

  // +++ ioctl +++
  struct iovec iov;
  iov.iov_base = msg_area;
  iov.iov_len = MAX_MSG_SIZE;

  printf("Going to ioctl with cmd=%i and iov=%p. The message is @%p and of %lu bytes\n", KZIMP_IOCTL_SPLICE_WRITE, &iov, iov.iov_base, iov.iov_len);
  ioctl(fd, KZIMP_IOCTL_SPLICE_WRITE, &iov);

  munmap(msg_area, msg_area_len);

  close(fd);
}


void do_read(void)
{
  int fd;
  char buf[MAX_MSG_SIZE];

  fd = open("/dev/kzimp0", O_RDONLY);
  if (fd < 0)
  {
     perror("Failed to open channel for reading");
     return;
  }

  // TODO: read

  close(fd);
}


int main(void)
{
   do_write();
   do_read();

   return 0;
}
