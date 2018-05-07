#include <stdio.h>

int main() {
   int c, i, j;

   for (c = 0; c < 50; c++) {
      for (i = 0; i < 61; i++) {
         for (j = 0; j < i + 1; j++) {
            printf("%c", j);
            if (j % 5 < 2)
               printf("%c", i + 27);
         }
      }
   }

/*
   for (j = 8; j < 126; j++) {
      for (i = 7; i < 126; i += 2) {
         c = 1;
         while (c) {
            printf("%c", c);
            c += i;
            if (c >= 128) {
               c %= 128;
            }
         }
      }
*/   
}
