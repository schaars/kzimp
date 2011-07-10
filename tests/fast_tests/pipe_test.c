#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#define MAX_LEN 4096
char buffer[MAX_LEN];

void le_sender_crashe()
{
   int pipefd[2];
   pid_t cpid;

   if (pipe(pipefd) == -1) {
      perror("pipe");
      exit(EXIT_FAILURE);
   }

   cpid = fork();
   if (cpid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
   }

   if (cpid == 0) {    /* Child reads from pipe */
      close(pipefd[1]);          /* Close unused write end */

      int total_read = 0;
      sleep(2);
      while (1) {
         printf(">>> Going to read %i\n", MAX_LEN);

         int r = read(pipefd[0], buffer, MAX_LEN);
         if (r > 0) {
            total_read += r;
         } else if (r == 0) {
            printf(">>> EOF reached\n");
            break;
         } else {
            perror("Read failed");
         }

         printf(">>> Has read %i\n", total_read);
      }

      printf(">>> OH NO!\n");

      close(pipefd[0]);
      exit(EXIT_SUCCESS);

   } else {            /* Parent writes argv[1] to pipe */
      close(pipefd[0]);          /* Close unused read end */
      
      bzero((void*)buffer, MAX_LEN);
      int i=0;
      while (1) {
         write(pipefd[1], buffer, MAX_LEN);
         sleep(1);
         i++;
         if (i==3) {
            printf("<<< Stop sending\n");
            exit(EXIT_FAILURE);
         }
      }

      close(pipefd[1]);          /* Reader will see EOF */
      wait(NULL);                /* Wait for child */
      exit(EXIT_SUCCESS);
   }
}

void le_receiver_crashe()
{
   int pipefd[2];
   pid_t cpid;

   if (pipe(pipefd) == -1) {
      perror("pipe");
      exit(EXIT_FAILURE);
   }

   cpid = fork();
   if (cpid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
   }

   if (cpid == 0) {    /* Child reads from pipe */
      close(pipefd[1]);          /* Close unused write end */

      int total_read = 0;
      int i=0;
      while (1) {
         printf(">>> Going to read %i\n", MAX_LEN);

         int r = read(pipefd[0], buffer, MAX_LEN);
         if (r > 0) {
            total_read += r;
         } else if (r == 0) {
            printf(">>> EOF reached\n");
            break;
         } else {
            perror("Read failed");
         }

         printf(">>> Has read %i\n", total_read);

         i++;
         if (i == 3) {
            exit(EXIT_FAILURE);
         }
      }

      close(pipefd[0]);
      exit(EXIT_SUCCESS);

   } else {            /* Parent writes argv[1] to pipe */
      close(pipefd[0]);          /* Close unused read end */
      
      bzero((void*)buffer, MAX_LEN);
      while (1) {
         printf("<<< Going to write %i\n", MAX_LEN);
         int r = write(pipefd[1], buffer, MAX_LEN);
         if (r < 0) {
            perror("Write failed");
         }

         printf("<<< Returned value: %i\n", r);
      }

      close(pipefd[1]);          /* Reader will see EOF */
      wait(NULL);                /* Wait for child */
      exit(EXIT_SUCCESS);
   }
}

int main(void) {
   le_sender_crashe();
   //le_receiver_crashe();
   
   return 0;
}
