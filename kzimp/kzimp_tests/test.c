#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

void do_reader(void)
{
  int fd, i;

  fd = open("/dev/kzimp0", O_RDONLY /*| O_NONBLOCK*/);

  // this will cause a timeout at the writer, and this reader will be put offline
  sleep(10);

  // read. It will return an error.
  int r = read(fd, (void*) &i, sizeof(i));
  if (r == -1) {
    perror("read error");
        i=-1;
  }
  printf(">>> read %i\n", i);

  /*
  // wait a few seconds, so that the writer gets blocked
  sleep(5);

  // read 10 messages
  for (i = 0; i < 10; i++)
  {
    read(fd, (void*) &i, sizeof(i));
    printf(">>> read %i\n", i);
  }
  */

  sleep(2);

  close(fd);
}

void do_writer(void)
{
  int fd, i;

  fd = open("/dev/kzimp0", O_WRONLY);

  // wait a little to block the writer
  //sleep(5);

  // send 11 messages. It will block at the 10th message.
  for (i = 0; i < 11; i++)
  {
    write(fd, (void*) &i, sizeof(i));
    printf("<<< wrote %i\n", i);
  }

  sleep(2);

  close(fd);
}

int main(void)
{
  //do_reader();
  do_writer();

  return 0;
}
