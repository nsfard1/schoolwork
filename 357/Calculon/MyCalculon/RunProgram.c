#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "Util.h"
#include "RunProgram.h"

#define DIFF_MASK 1
#define RUN_MASK 2
#define TIME_MASK 4

void WriteError(Program *prog, TestResult *res) {
   int status = res->statuses;

   if (status & DIFF_MASK && status & RUN_MASK && status & TIME_MASK) {
      printf("%s test %d failed: diff failure, runtime error, timeout\n", 
       prog->name, res->testNum + 1);
   }

   else if (status & DIFF_MASK && status & RUN_MASK) {
      printf("%s test %d failed: diff failure, runtime error\n", prog->name, 
       res->testNum + 1);
   }

   else if (status & DIFF_MASK && status & TIME_MASK) {
      printf("%s test %d failed: diff failure, timeout\n", prog->name, 
       res->testNum + 1);
   }

   else if (status & RUN_MASK && status & TIME_MASK) {
      printf("%s test %d failed: runtime error, timeout\n", prog->name, 
       res->testNum + 1);
   }

   else if (status & DIFF_MASK) {
      printf("%s test %d failed: diff failure\n", prog->name, 
       res->testNum + 1);
   }

   else if (status & RUN_MASK) {
      printf("%s test %d failed: runtime error\n", prog->name, 
       res->testNum + 1);
   }

   else if (status & TIME_MASK) {
      printf("%s test %d failed: timeout\n", prog->name, res->testNum + 1);
   }
}

char **FillGCCArgs(Program *toRun) {
   char **args;
   int status;

   args = malloc(MAX_FILES * sizeof(char *));
   args[ARG0] = malloc(strlen(GCC) + 1);
   strcpy(args[ARG0], GCC);

   args[ARG1] = malloc(strlen(O_OPT) + 1);
   strcpy(args[ARG1], O_OPT);

   args[ARG2] = malloc(strlen(toRun->name) + 1);
   strcpy(args[ARG2], toRun->name);

   status = ARG3;
   while (*toRun->files) {
      if (strstr(*toRun->files, ".c")) {
         args[status] = malloc(strlen(*toRun->files) + 1);
         strcpy(args[status++], *toRun->files);
      }

      toRun->files++;
   }

   args[status] = NULL;

   return args;
}

int RunProgram(Program *toRun) {
   int testndx, errorExists, numFails = 0, noMake, status, testNum;
   struct stat buf;
   char **args;
   TestResult *tresult = malloc(toRun->numTests * sizeof(TestResult));
   Test *temp;

   /* determine if makefile exists */
   noMake = stat("./Makefile", &buf);
   if (noMake) {
      noMake = stat("./makefile", &buf);
   }

   /* if noMake, setup gcc arguments */
   if (noMake) {
      args = FillGCCArgs(toRun);
   }

   /* either make it, or gcc all necessary files */
   if (!fork()) {
      if (noMake) {
         execv(GCC, args);
      }

      else {
         execl("/usr/bin/make", "/usr/bin/make", "-s", toRun->name, NULL);
      }
   }
   
   else {
      wait(&status);
   }

   if (!noMake && WEXITSTATUS(status)) {
      printf("Failed: make %s\n", toRun->name);
      return NO_MAKE;
   }

   else if (noMake && WEXITSTATUS(status)) {
      printf("Failed: gcc %s %s %s\n", args[ARG3], args[ARG1], args[ARG2]);
      return 1;
   }

   temp = toRun->tests;
   /* Call RunTest for each test, saving the results */
   for (testNum = 0; testNum < toRun->numTests; testNum++) {
      (tresult + testNum)->testNum = testNum;
      RunTest(toRun, toRun->tests, tresult + testNum);
      toRun->tests = toRun->tests->next;
   }
   toRun->tests = temp;

   /* Fill in the ProgResult */
   testndx = 0;

   for (testNum = 0; testNum < toRun->numTests; testNum++) {
      errorExists = (tresult + testNum)->statuses;

      if (errorExists) {
         WriteError(toRun, tresult + testNum);
         numFails++;
      }
   }

   if (!numFails) {
      printf("%s %d of %d tests passed.\n", toRun->name, toRun->numTests, 
       toRun->numTests);
   }

   return noMake ? 0 : NO_MAKE;
}
