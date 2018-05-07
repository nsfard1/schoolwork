#include <stdio.h>

int main() {
   int val, *ptr = NULL;

   scanf("%d", &val);

   switch (val) {
      case 1: // PASS output matches
         printf("Congrats, you passed test 1\n");
         break;
      case 2: // FAIL diff fail
         printf("Sorry, there was a diff in output\n");
         break;
      case 3: // FAIL timout fail
         for (;;) {}
      case 4: // FAIL runtime error
         val = *ptr;
   }

   return 0;
}
