#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "Report.h"

#define NUMSTEP 'S'
#define OUTFDS 'O'
#define INFDS 'I'
#define FIXED 'V'
#define ID 'D'
#define EXIT_FIXED 5
#define EXIT_ZERO 7
#define FDMAX 13
#define MAX_OUT 3

typedef struct {
   double *value;
   int step;
   int *id;
   int *numSteps;
   int inFDs[2];
   int outFDs[MAX_OUT];
   int outCount;
   int inCount;
} Cell;

int FillCell(Cell *cell, char *s) {
   int result = EXIT_SUCCESS;
   
   if (*s == NUMSTEP && !cell->numSteps) {
      cell->numSteps = malloc(sizeof(int));
      s++;
      *(cell->numSteps) = atoi(s);
   }

   else if (*s == OUTFDS && cell->outCount < MAX_OUT) {
      s++;
      if (atoi(s) < FDMAX && atoi(s) > 2) {
         cell->outFDs[cell->outCount++] = atoi(s);
      }
      else
         result = EXIT_FAILURE;
   }

   else if (*s == INFDS && cell->inCount < 2) {
      s++;
      if (atoi(s) < FDMAX && atoi(s) > 2) {
         cell->inFDs[cell->inCount++] = atoi(s);
      }
      else
         result = EXIT_FAILURE;
   }

   else if (*s == FIXED && !cell->value) {
      cell->value = malloc(sizeof(int));
      s++;
      *(cell->value) = atof(s);
   }

   else if (*s == ID && !cell->id) {
      cell->id = malloc(sizeof(int));
      s++;
      *(cell->id) = atoi(s);
   }
   return result;
}

void WriteOut(Cell *cell) {
   int index;
   Report report = {*(cell->id), cell->step++, *(cell->value)};

   for (index = 0; index < cell->outCount; index++) {
      write(cell->outFDs[index], &report, sizeof(Report));
   }
}

int CheckCell(Cell *cell) {
   int result = EXIT_FAILURE;
   int fixed = 0, index;

   if (cell->value || !cell->inCount) {
      fixed = 1;
   }

   if (!cell->value) {
      cell->value = malloc(sizeof(double));
      *(cell->value) = 0;
   }

   if (cell->id && cell->numSteps) {
      result = EXIT_SUCCESS;
      WriteOut(cell);
   }

   if (*(cell->numSteps) == 0) {
      return EXIT_ZERO;
   }

   if (fixed) {
      for (index = 1; index < *(cell->numSteps) + 1; index++) {
         WriteOut(cell);
      }
      result = EXIT_FIXED;
   }
   return result;
}

int CalculateAvg(Cell *cell) {
   Report rep;
   int status = EXIT_SUCCESS;
   double res = 0;

   if (read(cell->inFDs[0], &rep, sizeof(Report)) != sizeof(Report))
      status = EXIT_FAILURE;

   if (rep.step == cell->step - 1) {
      res += rep.value;
   }
   else
      status = EXIT_FAILURE;

   if (read(cell->inFDs[1], &rep, sizeof(Report)) != sizeof(Report)) 
      status = EXIT_FAILURE;

   if (rep.step == cell->step - 1) {
      res += rep.value;
   }
   else
      status = EXIT_FAILURE;

   res /= 2;
   *(cell->value) = res;
   return status;
}

int main(int argc, char **argv) {
   int temp, index, status, exitStatus = EXIT_SUCCESS;
   Cell *cell = malloc(sizeof(Cell));

   cell->outCount = cell->inCount = cell->step = 0;

   while (*++argv) {
      status = FillCell(cell, *argv);
      if (status == EXIT_FAILURE) 
         return status;
   }

   status = CheckCell(cell);
   if (status == EXIT_FAILURE) 
      return status;

   if (status == EXIT_ZERO) 
      return EXIT_SUCCESS;

   if (status != EXIT_FIXED) {
      for (index = 1; index < *(cell->numSteps); index++) {
         status = CalculateAvg(cell);
         if (status == EXIT_FAILURE)
            return status;

         WriteOut(cell);
      }
  
      if (CalculateAvg(cell) == EXIT_FAILURE) {
         return EXIT_FAILURE;
      }

      temp = cell->outFDs[0];
      if (cell->outFDs + 2) {
         if (cell->outFDs[1] < cell->outFDs[0] && 
          cell->outFDs[1] < cell->outFDs[2]) {
            cell->outFDs[0] = cell->outFDs[1];
            cell->outFDs[1] = temp;
         }
         else if (cell->outFDs[2] < cell->outFDs[0] && cell->outFDs[2]) {
            cell->outFDs[0] = cell->outFDs[2];
            cell->outFDs[2] = temp;
         }
      }

      else if (cell->outFDs[1] < cell->outFDs[0]) {
         cell->outFDs[0] = cell->outFDs[1];
         cell->outFDs[1] = temp;
      }

      cell->outCount = 1;
      WriteOut(cell);
   }

   if (cell->inFDs) {
      close(cell->inFDs[0]);
      close(cell->inFDs[1]);
   }

   close(cell->outFDs[2]);

   close(cell->outFDs[1]);
   close(cell->outFDs[0]);

   free(cell->value);
   free(cell->id);
   free(cell->numSteps);
   free(cell);

   return exitStatus;
}
