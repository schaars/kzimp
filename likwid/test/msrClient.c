#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define LLU_CAST  (unsigned long long)

typedef enum {
    MSR_READ = 0,
    MSR_WRITE
} MsrAccessType;

typedef struct {
    int cpu;
    uint32_t reg;
    uint64_t data;
    MsrAccessType type;
} MsrDataRecord;


int main(int argc, char** argv)
{
 struct sockaddr_un address;
 uint32_t reg;
 int socket_fd, nbytes;
 pid_t pid=0;
 size_t address_length;
 MsrDataRecord msrData;

 socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
 if(socket_fd < 0)
 {
  printf("socket() failed\n");
  return 1;
 }

 pid = atoi(argv[1]);
 
 address.sun_family = AF_LOCAL;
 address_length = sizeof(address);
sprintf(address.sun_path, "/tmp/likwid-%d",pid);

 if(connect(socket_fd, (struct sockaddr *) &address, address_length) != 0)
 {
  printf("connect() failed\n");
  return 1;
 }

 msrData.cpu = 0;
 msrData.reg = strtoul(argv[2],NULL,16);
 msrData.data = 0x00ULL;
 msrData.type = MSR_READ;

 write(socket_fd, &msrData, sizeof(MsrDataRecord));
 read(socket_fd,  &msrData, sizeof(MsrDataRecord));

 printf("RETURN: cpu: %d reg: 0x%x data: %llu\n",
         msrData.cpu,
         msrData.reg,
         LLU_CAST msrData.data);

 close(socket_fd);

 return 0;
}
