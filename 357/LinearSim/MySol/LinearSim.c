#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdarg.h>

#include "Report.h"

#define NUMCELL 'C'
#define STEP 'S'
#define LEFT 'L'
#define RIGHT 'R'
#define BAD_ARGS 11
#define BAD_EXIT 12
#define TOO_FEW 13
#define TOO_MANY 14
#define MIN_PIPES 3
#define MAX_PIPES 6
#define TOTAL_DES 5

void ClosePipes(int count, ...) {
   va_list ap;
   int index;
   
   va_start(ap, count);
   for (index = 0; index < count; index++) {
      close(va_arg(ap, int));
   }
   va_end(ap);
}

void WriteError(int status, int val1, int val2) {
   if (status == BAD_ARGS) {
      fprintf(stderr, "Usage: LinearSim C S L R (in any order)\n");
   }

   else if (status == BAD_EXIT) {
      fprintf(stderr, "Error: Child %d exited with %d\n", val1, val2);
   }

   else if (status == TOO_FEW) {
      fprintf(stderr, "Error: %d cells reported too few reports\n", val1);
   }

   else if (status == TOO_MANY) {
      fprintf(stderr, "Error: %d cells reported too many reports\n", val1);
   }
}

char **FormatArg(int step, int id, int out0, int out1, int out2, int in0, 
 int in1, int fixed, double value) {
   char **new, **arg = malloc(10 * sizeof(char *));
   int index, count = 1;

   *arg = malloc((TOTAL_DES) * sizeof(char));
   sprintf(*arg, "Cell");

   *(arg + count) = malloc(10 * sizeof(char));
   sprintf(*(arg + count++), "S%d", step);

   *(arg + count) = malloc(10 * sizeof(char));
   sprintf(*(arg + count++), "D%d", id);

   *(arg + count) = malloc(10 * sizeof(char));
   sprintf(*(arg + count++), "O%d", out0);

   if (out1) {
      *(arg + count) = malloc(10 * sizeof(char));
      sprintf(*(arg + count++), "O%d", out1);
   }

   if (out2) {
      *(arg + count) = malloc(10 * sizeof(char));
      sprintf(*(arg + count++), "O%d", out2);
   }

   if (in0) {
      *(arg + count) = malloc(10 * sizeof(char));
      sprintf(*(arg + count++), "I%d", in0);
      *(arg + count) = malloc(10 * sizeof(char));
      sprintf(*(arg + count++), "I%d", in1);
   }

   if (fixed) {
      *(arg + count) = malloc(10 * sizeof(char));
      sprintf(*(arg + count++), "V%lf", value);
   }

   *(arg + count++) = NULL;

   new = malloc(count * sizeof(char *));

   for (index = 0; index < count; index++) {
      *(new + index) = *(arg + index);
   }

   return new;   
}

void LessThan3(int numCell, int **pids, int numSteps, double left, 
 double right, int fd[2]) {
   int pid;
   char **args;

   if (!(pid = fork())) { 
      close(fd[0]);
      args = FormatArg(numSteps, 0, fd[1], 0, 0, 0, 0, 1, left);
      execvp(*args, args);
   }

   else if (numCell == 2) {
      **pids = pid;
      if (!(pid = fork())) {
         close(fd[0]);
         args = FormatArg(numSteps, 1, fd[1], 0, 0, 0, 0, 1, right);
         execvp(*args, args);
      }
      
      else {
         *(*pids + 1) = pid;
         close(fd[1]);
      }
   }
  
   else {
      **pids = pid;
      close(fd[1]);
   }
}

void Equals3(int **pids, int numSteps, double left, double right, int fd0[2], 
 int fd1[2], int fd2[2]) {
   char **args;
   int pid;

   if (!(pid = fork())) { 
      ClosePipes(MIN_PIPES + 1, fd0[0], fd1[0], fd2[0], fd2[1]);
      args = FormatArg(numSteps, 0, fd0[1], fd1[1], 0, 0, 0, 1, left);
      execvp(*args, args);
   }

   else {
      **pids = pid;
      if (!(pid = fork())) {
         ClosePipes(MIN_PIPES, fd0[0], fd1[1], fd2[1]);
         args = FormatArg(numSteps, 1, fd0[1], 0, 0, fd1[0], fd2[0], 0, 0);
         execvp(*args, args);
      }

      else {
         *(*pids + 1) = pid;
         if (!(pid = fork())) {
            ClosePipes(MIN_PIPES + 1, fd0[0], fd1[0], fd1[1], fd2[0]);
            args = FormatArg(numSteps, 2, fd0[1], fd2[1], 0, 0, 0, 1, right);
            execvp(*args, args);
         }
      
         else { 
            *(*pids + 2) = pid;
            ClosePipes(TOTAL_DES, fd0[1], fd1[0], fd1[1], fd2[0], fd2[1]);
         }
      }
   }
}

void AddFirst2(int **pids, int numSteps, double left, int fd0[2], int fd1[2], 
 int fd2[2], int fd3[2]) {
   int pid;
   char **args;

   if (!(pid = fork())) {
      ClosePipes(MAX_PIPES, fd0[0], fd1[0], fd2[0], fd2[1], fd3[0], fd3[1]);
      args = FormatArg(numSteps, 0, fd0[1], fd1[1], 0, 0, 0, 1, left);
      execvp(*args, args);
   }

   else {
      **pids = pid;
      if (!(pid = fork())) {
         ClosePipes(MIN_PIPES + 1, fd0[0], fd1[1], fd2[1], fd3[0]);
         args = FormatArg(numSteps, 1, fd0[1], fd3[1], 0, fd1[0], fd2[0], 0, 
          0);
         execvp(*args, args);
      }

      else {
         *(*pids + 1) = pid;
         ClosePipes(2, fd1[0], fd1[1]);
      }
   }
}

void AddCells(int numCells, int **pids, int numSteps, int fds[TOTAL_DES][2], 
 int *lastIn, int *lastOut) {
   int temp, prevIn = 2, prevOut = 2 + 1, curIn = 1, curOut = 2 + 2, pid, 
    id = 2;
   char **args;

   while (numCells-- > 2) {
      pipe(fds[curIn]);
      pipe(fds[curOut]);

      ClosePipes(2, fds[prevIn][0], fds[prevOut][1]);

      if (!(pid = fork())) {
         ClosePipes(MIN_PIPES, fds[0][0], fds[curIn][1], fds[curOut][0]);
         args = FormatArg(numSteps, id, fds[0][1], fds[prevIn][1], 
          fds[curOut][1], fds[prevOut][0], fds[curIn][0], 0, 0);
         execvp(*args, args);
      }

      else {
         *(*pids + id++) = pid;
         ClosePipes(2, fds[prevIn][1], fds[prevOut][0]);
      }

      temp = prevIn;
      prevIn = curIn;
      curIn = temp;
      
      temp = prevOut;
      prevOut = curOut;
      curOut = temp;
   }

   *lastIn = prevIn;
   *lastOut = prevOut;
}

void AddLast2(int **pids, int prevIn, int prevOut, int numSteps, int id, 
 double right, int fds[TOTAL_DES][2]) {
   char **args;
   int pid, lastPipe = prevIn == 1 ? 2 : 1;

   pipe(fds[lastPipe]);
   ClosePipes(2, fds[prevIn][0], fds[prevOut][1]);
   if (!(pid = fork())) {
      ClosePipes(2, fds[0][0], fds[lastPipe][1]);
      args = FormatArg(numSteps, id, fds[0][1], fds[prevIn][1], 0, 
       fds[prevOut][0], fds[lastPipe][0], 0, 0);
      execvp(*args, args);
   }

   else {
      *(*pids + id++) = pid;
      if (!(pid = fork())) {
         ClosePipes(MIN_PIPES + 1, fds[0][0], fds[prevIn][1], fds[prevOut][0], 
          fds[lastPipe][0]);
         args = FormatArg(numSteps, id, fds[0][1], fds[lastPipe][1], 0, 0, 0, 
          1, right);
         execvp(*args, args);
      }

      else {
         *(*pids + id) = pid;
         ClosePipes(TOTAL_DES, fds[0][1], fds[prevIn][1], fds[prevOut][0], 
          fds[lastPipe][0], fds[lastPipe][1]);
      }
   }
}

void ExecCells(int FDS[TOTAL_DES][2], int **pids, int numCells, int numSteps, 
 double left, double right) {
   int id, prevIn, prevOut;

   id = 1;

   if (numCells < MIN_PIPES) {
      LessThan3(numCells, pids, numSteps, left, right, FDS[0]);
   }         

   else {
      pipe(FDS[1]);
      pipe(FDS[2]);
      if (numCells == MIN_PIPES) {
         Equals3(pids, numSteps, left, right, FDS[0], FDS[1], FDS[2]);
      }

      else {
         pipe(FDS[MIN_PIPES]);
         AddFirst2(pids, numSteps, left, FDS[0], FDS[1], FDS[2], FDS[MIN_PIPES]);
         numCells -= 2;
         prevIn = 2;
         prevOut = MIN_PIPES;
         AddCells(numCells, pids, numSteps, FDS, &prevIn, &prevOut);
         AddLast2(pids, prevIn, prevOut, numSteps, numCells, right, FDS);
      }
   }
}

int FillArgs(char **argv, int *cell, int *step, double *left, double *right) {
   int validC, validS, validL, validR, status = EXIT_SUCCESS;
   
   validC = validS = validL = validR = 0;

   while (*++argv) {
      if (**argv == NUMCELL && !validC) {
         (*argv)++;
         *cell = atoi(*argv);
         validC = 1;
      }

      else if (**argv == STEP && !validS) {
         (*argv)++;
         *step = atoi(*argv);
         validS = 1;
      }

      else if (**argv == LEFT && !validL) {
         (*argv)++;
         *left = atof(*argv);
         validL = 1;
      }

      else if (**argv == RIGHT && !validR) {
         (*argv)++;
         *right = atof(*argv);
         validR = 1;
      }
   }

   if (*cell < 1 || !validS || (*cell == 1 && validR)) {
      WriteError(BAD_ARGS, 0, 0);
      status = EXIT_FAILURE;
   }

   return status;
}

int main(int argc, char **argv) {
   int FDS[TOTAL_DES][2], *numReports, *pids, pstat, status = EXIT_SUCCESS;
   int numCell = 0, numStep = 0, index, fewCount = 0, manyCount = 0;
   double left = 0, right = 0;
   Report report;

   pipe(FDS[0]);
   if (FillArgs(argv, &numCell, &numStep, &left, &right) == EXIT_FAILURE) {
      return EXIT_FAILURE;
   }

   pids = malloc(numCell * sizeof(int));

   numReports = malloc(numCell * sizeof(int));
   for (index = 0; index < numCell; index++) {
      *(numReports + index) = 0;
   }
   
   ExecCells(FDS, &pids,  numCell, numStep, left, right);

   while (read(FDS[0][0], &report, sizeof(Report))) {
      printf("Result from %d, step %d: %.3lf\n", report.id, report.step, report.value);
      *(numReports + report.id) += 1;
   }

   for (index = 0; index < numCell; index++) {
      waitpid(*(pids + index), &pstat, 0);
      pstat = WEXITSTATUS(pstat);
      if (pstat != EXIT_SUCCESS) {
         WriteError(BAD_EXIT, index, pstat);
         status = EXIT_FAILURE;
      }
   }

   close(FDS[0][0]);

   for (index = 0; index < numCell; index++) {
      if (*(numReports + index) < numStep + 1) {
         fewCount++;
      }

      else if (*(numReports + index) > numStep + 1) {
         manyCount++;
      }
   }

   if (fewCount) {
      WriteError(TOO_FEW, fewCount, 0);
      status = EXIT_FAILURE;
   }

   if (manyCount) {
      WriteError(TOO_MANY, manyCount, 0);
      status = EXIT_FAILURE;
   }

   free(pids);
   free(numReports);

   return status;
}
