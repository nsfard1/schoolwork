#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "Util.h"
#include "RunTest.h"

#define SR_STAT 192
#define RUN_MASK 8
#define TIME_SCALE 10

char **FillArgs(Program *prog, Test *test) {
   char **args, *temp = malloc(BUFFER);
   int index = NAME_ARG + 1;

   args = malloc(MAX_FILES * sizeof(char *));
   args[ARG0] = malloc(strlen(SAFERUN) + 1);
   strcpy(args[ARG0], SAFERUN);
   args[ARG1] = malloc(strlen(PROC_LIM) + 1);
   strcpy(args[ARG1], PROC_LIM);

   sprintf(temp, "-t%d", test->timeout);
   args[ARG2] = malloc(strlen(temp) + 1);
   strcpy(args[ARG2], temp);
   sprintf(temp, "-T%d", TIME_SCALE * test->timeout);
   args[ARG3] = malloc(strlen(temp) + 1);
   strcpy(args[ARG3], temp);

   sprintf(temp, "./%s", prog->name);
   args[NAME_ARG] = malloc(strlen(temp) + 1);
   strcpy(args[NAME_ARG], temp);

   while (*(test->args)) {
      args[index] = malloc(strlen(*(test->args)) + 1);
      strcpy(args[index++], *(test->args++));
   }

   args[index] = NULL;

   return args;
}

int RunTest(Program *prog, Test *test, TestResult *result) {
   int infd, outfd,  status, diff = 0, runtime = 0, timeout = 0, res;
   char **args;

   /* set up SafeRun arguments */
   args = FillArgs(prog, test);
   infd = open(test->inFile, O_RDONLY);
   outfd = open(TEST_OUT, O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | 
    S_IRGRP | S_IWGRP | S_IROTH);

   /* execute test with SafeRun */
   if (!fork()) {
      dup2(infd, 0);
      dup2(outfd, 1);
      dup2(outfd, 2);

      close(infd);
      close(outfd);

      execv(SAFERUN, args);
   }

   else {
      wait(&status);
      status = WEXITSTATUS(status);
   }

   /* determine SafeRun exit status */
   if (status) {
      runtime = 1;

      status -= SR_STAT;
      if (status > 0 && status % 2) {
         timeout = 1;
      }
   }

   /* diff output */
   if (!fork()) {
      dup2(infd, 0);
      dup2(outfd, 1);
      dup2(outfd, 2);

      close(infd);
      close(outfd);

      execl("/usr/bin/diff", "/usr/bin/diff",  TEST_OUT, test->outFile, NULL);
   }

   else {
      wait(&status);
      status = WEXITSTATUS(status);
      if (status) {
         diff = 1;
      }
   }

   /* fill TestResult */
   res = 0;
   res += diff;
   res += runtime << 1;
   res += timeout << 2;

   result->statuses = res;

   close(infd);
   close(outfd);

   return 0;
}
