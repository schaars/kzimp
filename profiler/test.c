#include <poll.h>

int main(int argc, char **argv) {
   static struct pollfd event_array[] = {
            { .events = POLLIN, .fd = 0 },
   };
   while(1) {
      poll(event_array, 1, 0);
   }
}
