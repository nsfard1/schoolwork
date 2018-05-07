#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "Util.h"
#include "Parse.h"
#include "RunProgram.h"

#define MAX_FILES 32

void RemoveAll(Program **program, char **dirName) { 
   int index;
   Test *temp;

   remove("./test.output.temp");

   for (index = 0; index < (*program)->numFiles; index++) {
      remove(*((*program)->files + index));
   }
  
   temp = (*program)->tests; 
   while (temp) {
      remove(temp->inFile);
      remove(temp->outFile);
      temp = temp->next;
   }

   remove((*program)->name);

   remove("Makefile");

   chdir("..");
   remove(*dirName);
}

int CopyFiles(Program **program, char **dirName) {
   struct stat buf;
   int index;
   char fileName[MAX_FILES];

   for (index = 0; index < (*program)->numFiles; index++) {
      sprintf(fileName, "./%s", *((*program)->files + index));
      if (stat(fileName, &buf)) {
         printf("Could not find required test file '%s'\n", fileName + 2);
         chdir(*dirName);
         RemoveAll(program, dirName);
         return 1;
      }
      if (!fork())
         execl("/bin/cp", "cp", *((*program)->files + index), *dirName, NULL);
      else 
         wait(NULL);
   }

   return 0;
}

int CopyTests(Program **program, char **dirName) {
   Test *temp;
   struct stat buf;
   char fileName[MAX_FILES];

   temp = (*program)->tests;
   while (temp) {
      sprintf(fileName, "./%s", temp->inFile);
      if (stat(fileName, &buf)) {
         printf("Could not find required test file '%s'\n", fileName + 2);
         chdir(*dirName);
         RemoveAll(program, dirName);
         return 1;
      }

      if (!fork()) 
         execl("/bin/cp", "cp", fileName, *dirName, NULL);
      else
         wait(NULL);

      sprintf(fileName, "./%s", temp->outFile);
      if (stat(fileName, &buf)) {
         printf("Could not find required test file '%s'\n", fileName + 2);
         chdir(*dirName);
         RemoveAll(program, dirName);
         return 1;
      }

      if (!fork()) 
         execl("/bin/cp", "cp", fileName, *dirName, NULL);
      else
         wait(NULL);
      temp = temp->next;
   }
   return 0;
}

int main(int argc, char **argv) {
   FILE *suiteFile = fopen(argv[1], "r");
   Program **program = malloc(sizeof(Program *));
   char *dirName, fileName[MAX_FILES];
   int status, make = 0;
   struct stat buf;

   dirName = malloc(MAX_FILES);
   if (stat(argv[1], &buf)) {
      printf("Could not find required test file '%s'\n", argv[1]);
      return 1;
   }

   sprintf(dirName, ".%d", getpid());
   Parse(program, suiteFile);

   while (*program) {
      mkdir(dirName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if (CopyFiles(program, &dirName)) 
         return 1;

      if (!stat("Makefile", &buf)) {
         make = 1;
         if (!fork())
            execl("/bin/cp", "cp", "Makefile", dirName, NULL);
         else 
            wait(&status);
      } 

      if (CopyTests(program, &dirName))
         return 1;
         
      sprintf(fileName, "./%s", dirName);
      chdir(fileName);
      //RunProgram(*program);


      if (RunProgram(*program) != NO_MAKE) {
         (*program)->files -= (*program)->numFiles;
      }

      RemoveAll(program, &dirName);
      program = &((*program)->next);
   }

   fclose(suiteFile);
   return 0;
}
