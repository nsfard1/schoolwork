#include <unistd.h>
#include <stdio.h>

#include "header.h"


int main(int argc, char ** argv)
{
   int i;
   ssize_t size;
   char buffer[BUFFER_SIZE];

   printf("Hello, world!\n");
   for (i = 0; i < argc; ++ i)
      printf("%d: %s\n", i, argv[i]);

   printf("Writing input contents:\n");
   fflush(stdout);
   while ((size = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0)
      write(STDOUT_FILENO, buffer, size);

   return 0;
}
