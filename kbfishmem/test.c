#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


int main(void) {
   int fd;
   char *addr;

   fd = open("/dev/kbfishmem0", O_RDWR | O_CREAT);

   printf("File is open on fd=%i\n", fd);
   sleep(2);

   addr = mmap(NULL, 12288, PROT_WRITE, MAP_SHARED, fd, 8192);
   printf("Addr range of the mapping is [%p, %p]\n", addr, addr+12288-1);

   /*
   sleep(1);

   addr = mmap(NULL, 12288, PROT_WRITE, MAP_SHARED, fd, 4096);
   printf("Addr range of the mapping is [%p, %p]\n", addr, addr+12288-1);
   */

   sleep(2);

   // trigger a page fault
   addr[0] = 32;
   addr[4096] = 42;

   if (fd > 0) {
      close(fd);
   }

   return 0;
}
