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
    int fd, ret_val, count, numread;
    char buf[MAX_BUF_SIZE];

    /* Create the named - pipe */
    ret_val = mkfifo(HALF_DUPLEX, 0666);

    if ((ret_val == -1) && (errno != EEXIST)) {
        perror("Error creating the named pipe");
        exit (1);
    }

    /* Open the pipe for reading */
    fd = open(HALF_DUPLEX, O_RDONLY);

    //while (1) sleep(1);

    int e;
    while (1) {
    /* Read from the pipe */
    numread = read(fd, buf, MAX_BUF_SIZE);

    if (numread < 0) {
       perror("");
       break;
    } else if (numread == 0) {
       printf("Error: has received 0 bytes\n");
       break;
    }


    buf[numread] = '0';

    printf("Half Duplex Server : Read From the pipe : %s\n", buf);

    /* Convert to the string to upper case */
    count = 0;
    while (count < numread) {
        buf[count] = toupper(buf[count]);
        count++;
    }
    
    printf("Half Duplex Server : Converted String : %s\n", buf);

    sleep(1);
    }
}

