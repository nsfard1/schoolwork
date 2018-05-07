/* Test program to test LZWExp.c methods */
/* contains a main program and a DataSink callback function */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "LZWExp.h"
#include "SmartAlloc.h"
#include "MyLib.h"

void SinkFunction(void *sinkState, UChar *data, int numBytes) {
   int ndx;

   for (ndx = 0; ndx < numBytes; ndx++) {
      printf("%c", *data++);
   }
}

int main(int argc, char **argv) {
   int status = 1, recycleCode = DEFAULT_RECYCLE_CODE;

   LZWExp *exp;
   DataSink sink = SinkFunction;
   UInt buf;

   while (*++argv) {
      if (**argv == '-' && *(*argv + 1) == 'R') {
         recycleCode = atoi(*++argv);
         break;
      }
   }
   
   exp = malloc(sizeof(LZWExp));
   LZWExpInit(exp, sink, NULL, recycleCode);

   while (scanf("%8x", &buf) != EOF) {
      if (status && LZWExpDecode(exp, buf)) {
         printf("Bad code\n");
         LZWExpDestruct(exp);
         free(exp);
         status = 0;
      }
   }

   if (status) {
      if (LZWExpStop(exp)) {
         printf("Missing EOD\n");
      }

      LZWExpDestruct(exp);

      free(exp);
   }

   if (report_space()) {
      printf("%lu\n", report_space());
   }

   return 0;
}
