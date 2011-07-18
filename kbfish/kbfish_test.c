#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MSG_SIZE 64
#define NB_MSG 20

void do_reader(void)
{
  int fd;
  int r, i;
  pid_t pid;
  char msg[MSG_SIZE];

  // open channel, receiver side
  fd = open("/dev/kbfish0", O_RDWR | O_CREAT);
  if (fd < 0)
  {
    perror("reader open");
    exit(-1);
  }

  printf("I am the reader. I am gonna recv %i messages\n", NB_MSG);

  // recv NB_MSG messages
  for (i = 0; i < NB_MSG; i++)
  {
    r = read(fd, &pid, sizeof(pid));
    if (r < 0)
    {
      perror("Writer write");
      exit(-1);
    }
    printf("<<<[%i] Reading %i\n", i, pid);
  }

  close(fd);
}

void do_writer(void)
{
  int fd;
  int r, i;
  pid_t pid;
  char msg[MSG_SIZE];

  // open channel, sender side
  fd = open("/dev/kbfish0", O_RDWR);
  if (fd < 0)
  {
    perror("writer open");
    exit(-1);
  }

  pid = getpid();
  printf("I am the writer. I am gonna send %i %i times\n", pid, NB_MSG);

  // send NB_MSG messages
  for (i = 0; i < NB_MSG; i++)
  {
    printf(">>>[%i] Writing %i\n", i, pid);
    r = write(fd, &pid, sizeof(pid));
    if (r < 0)
    {
      perror("Writer write");
      exit(-1);
    }
  }

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
