/* Barrelfish communication mechanism - test */

#include <stdio.h>

#include "bfishmprotect.h"

int main(void)
{
   printf("%lu\n", get_ump_message_size());

  return 0;
}
