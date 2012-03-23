#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define HALF_DUPLEX		"/tmp/halfduplex"
#define MAX_BUF_SIZE	255

int main(int argc, char *argv[])
{
    int fd;

    /* Check if an argument was specified. */

    if (argc != 2) {
        printf("Usage : %s <string to be sent to the server>n", argv[0]);
        exit (1);
    }

    /* Open the pipe for writing */
    fd = open(HALF_DUPLEX, O_WRONLY);

while (1) sleep(1);

    int e;
    while (1) {
    /* Write to the pipe */
    e = write(fd, argv[1], strlen(argv[1]));
    if (e < 0) {
       perror("");
    } else if (e == 0) {
       printf("Error: has written 0 bytes\n");
    }
    }
}

