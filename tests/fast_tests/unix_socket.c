#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#define UNIX_SOCKET_FILE_NAME "/tmp/unix_file"
#define MSG_LEN 4096

int sock;
struct sockaddr_un address;

void init_socket(void) {
  // create socket
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);

  // fill the addresses
  bzero((char *) &address, sizeof(address));
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, sizeof(char) * 108, "%s", UNIX_SOCKET_FILE_NAME);
}


void do_client(void) {
   ssize_t total_size;
   char msg[MSG_LEN];

   total_size = 0;
   bzero(msg, MSG_LEN);

   while (1) {
      printf(">>> Going to send %i\n", MSG_LEN);
      
      // send message
      ssize_t s = sendto(sock, msg, MSG_LEN, 0, (struct sockaddr*) &address, sizeof(address));
      if (s < 0) {
         perror("SEND ERROR!");
         exit(-1);
      }

      printf("<<< Has sent %u so far\n", (unsigned int)total_size);

      exit(-1);
   }
}


void do_server(void) {
   char msg[MSG_LEN];

   // bind socket
   bind(sock, (struct sockaddr *) &address, sizeof(address));
   printf("Server is ready...\n");

   while (1) {
      recvfrom(sock, (char*) msg, MSG_LEN, 0, 0, 0);

      // do nothing
      // simulate the crash of the receiver by pressing ctrl+c
      //sleep(1);
   }
}

int main(void) {
   init_socket();

#if defined(CLIENT)
   do_client();
#elif defined(SERVER)
   do_server();
#endif

   close(sock);

   return 0;
}

