#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Util.h"
#include "Parse.h"

#define BUFFER 1000
#define MAX_FILES 32

int FillTests(Test **test, FILE *suiteFile, char **buff, int *testCount, 
 char **line) {
   char **tokens, *temp;
   int argCount;

   *line = malloc(BUFFER);
   *buff = fgets(*line, BUFFER, suiteFile);
   while (**line == 'T') {
      argCount = 0;
      *line += 2;
      *test = malloc(sizeof(Test));

      temp = strtok(*line, " \\/\n");
      (*test)->inFile = malloc(strlen(temp));
      strcpy((*test)->inFile, temp);

      temp = strtok(NULL, " \\/\n");
      (*test)->outFile = malloc(strlen(temp));
      strcpy((*test)->outFile, temp);

      temp = strtok(NULL, " \\/\n");
      (*test)->timeout = atoi(temp);

      tokens = malloc(MAX_FILES * sizeof(char *));
      while ((temp = strtok(NULL, " \\/\n"))) {
         *tokens = malloc(strlen(temp) + 1);
         strcpy(*tokens++, temp);
         argCount++;
      }
      *tokens = NULL;
      tokens -= argCount;

      (*test)->args = tokens;

      *buff = fgets(*line, BUFFER, suiteFile);
      (*testCount)++;
      test = &(*test)->next;
   }

   *test = NULL;

   return 0;
}

int Parse(Program **program, FILE *suiteFile) {
   char *line, *tempc, *buff;
   int fileCount, testCount;

   line = malloc(sizeof(char *));
   line = malloc(BUFFER);
   buff = fgets(line, BUFFER, suiteFile);

   while (buff) {
      line += 2;
      fileCount = testCount = 0;

      *program = malloc(sizeof(Program));
      (*program)->files = malloc(MAX_FILES * sizeof(char *));

      tempc = strtok(line, " \\/\n");
      (*program)->name = malloc(strlen(tempc));
      strcpy((*program)->name, tempc);

      while ((tempc = strtok(NULL, " \\/\n"))) {
         *(*program)->files = malloc(strlen(tempc) + 1);
         strcpy(*(*program)->files++, tempc);
         fileCount++;
      }

      (*program)->files -= fileCount;

      testCount = 0;
      (*program)->numFiles = fileCount;
      FillTests(&(*program)->tests, suiteFile, &buff, &testCount, &line);
      (*program)->numTests = testCount;

      program = &(*program)->next;
   }

   *program = NULL;
   
   return 0;
}
