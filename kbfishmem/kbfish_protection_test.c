#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

// Round up S to a multiple of the page size
#define PAGE_SIZE 4096
#define ROUND_UP_SIZE(S) (S + (PAGE_SIZE - S % PAGE_SIZE) % PAGE_SIZE)

#define CHANNEL_SIZE 10
#define MAX_MSG_SIZE 1024
#define AREA_SIZE ROUND_UP_SIZE(CHANNEL_SIZE * MAX_MSG_SIZE)

#define SENDER_TO_RECEIVER_OFFSET 4096
#define RECEIVER_TO_SENDER_OFFSET 8192

void do_reader(void)
{
  int fd, i;
  char *read_area, *write_area;
  char c;

  fd = open("/dev/kbfishmem0", O_RDWR | O_CREAT);
  if (fd < 0)
  {
    perror("Reader opening /dev/kbfishmem0");
    exit(-1);
  }

  read_area = mmap(NULL, AREA_SIZE, PROT_READ, MAP_SHARED, fd,
      SENDER_TO_RECEIVER_OFFSET);
  if (!read_area)
  {
    perror("Reader read_area mmap");
    exit(-1);
  }
  printf("Reader read_area @ %p mmapped\n", read_area);

  write_area = mmap(NULL, AREA_SIZE, PROT_WRITE, MAP_SHARED, fd,
      RECEIVER_TO_SENDER_OFFSET);
  if (!write_area)
  {
    perror("Reader write_area mmap");
    exit(-1);
  }
  printf("Reader write_area @ %p mmapped\n", write_area);

  sleep(2);

  // read read_area
  c = read_area[0];
  for (i = 1; i < AREA_SIZE; i++)
  {
    if (c != read_area[i])
    {
      printf("Reader: c=%i != read_area[%i]=%i\n", c, i, read_area[i]);
      break;
    }
  }
  printf("Reader has read %i\n", c);

  printf("Reader going to write %i in its write_area\n", getpid());
  memset(write_area, getpid(), AREA_SIZE);

  sleep(2);

  // read in write area
  // it does not segfault on an x86-64 because PROT_WRITE implies PROT_READ
  c = write_area[AREA_SIZE / 2];

  sleep(2);

  printf("Reader is closing the file\n");
  munmap(read_area, AREA_SIZE);
  munmap(write_area, AREA_SIZE);
  close(fd);
}

void do_writer(void)
{
  int fd, i;
  char *read_area, *write_area;
  char c;

  fd = open("/dev/kbfishmem0", O_RDWR);
  if (fd < 0)
  {
    perror("Writer opening /dev/kbfishmem0");
    exit(-1);
  }

  read_area = mmap(NULL, AREA_SIZE, PROT_READ, MAP_SHARED, fd,
      RECEIVER_TO_SENDER_OFFSET);
  if (!read_area)
  {
    perror("Writer read_area mmap");
    exit(-1);
  }
  printf("Writer read_area @ %p mmapped\n", read_area);

  write_area = mmap(NULL, AREA_SIZE, PROT_WRITE, MAP_SHARED, fd,
      SENDER_TO_RECEIVER_OFFSET);
  if (!write_area)
  {
    perror("Writer write_area mmap");
    exit(-1);
  }
  printf("Writer write_area @ %p mmapped\n", write_area);

  /****************************************************************************/
  // this read does not segfault :(
  printf("--- %i in 1\n", getpid());

  // read in write area -> segfault
  c = write_area[AREA_SIZE / 2];

  printf("--- %i in 2\n", getpid());

  sleep(1);
  /****************************************************************************/

  printf("Writer going to write %i in its write_area\n", getpid());
  memset(write_area, getpid(), AREA_SIZE);

  sleep(2);

  sleep(2);

  // read read_area
  c = read_area[0];
  for (i = 1; i < AREA_SIZE; i++)
  {
    if (c != read_area[i])
    {
      printf("Writer: c=%i != read_area[%i]=%i\n", c, i, read_area[i]);
      break;
    }
  }
  printf("Writer has read %i\n", c);

  sleep(2);

  // write in read area -> segfault
  //read_area[AREA_SIZE / 2] = 42;

  // read in write area
  // it does not segfault on an x86-64 because PROT_WRITE implies PROT_READ
  c = write_area[AREA_SIZE / 2];
  printf("Writer has read %i\n", c);

  sleep(2);

  printf("Writer is closing the file\n");
  munmap(read_area, AREA_SIZE);
  munmap(write_area, AREA_SIZE);
  close(fd);
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
