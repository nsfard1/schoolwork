#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "RunProgram.h"

int main() {
   Program prog;
   Test test1, test2, test3, test4;
   TestResult tr[4];

   prog.name = "test";
   prog.numFiles = 1;
   prog.files = malloc(sizeof(char *));
   *prog.files = malloc(strlen("testProg.c") + 1);
   strcpy(*prog.files, "testProg.c");
   prog.numTests = 4;
   prog.tests = &test1;
   prog.next = NULL;

   test1.inFile = "test1.in";
   test1.outFile = "test1.out";
   test1.timeout = 1000;
   test1.args = malloc(MAX_FILES * sizeof(char *));
   test1.next = &test2;

   test2.inFile = "test2.in";
   test2.outFile = "test1.out";
   test2.timeout = 1000;
   test2.args = malloc(MAX_FILES * sizeof(char *));
   test2.next = &test3;

   test3.inFile = "test3.in";
   test3.outFile = "test3.out";
   test3.timeout = 10;
   test3.args = malloc(MAX_FILES * sizeof(char *));
   test3.next = &test4;

   test4.inFile = "test4.in";
   test4.outFile = "test4.out";
   test4.timeout = 1000;
   test4.args = malloc(MAX_FILES * sizeof(char *));
   test4.next = NULL;

   RunProgram(&prog);

   printf("Exited RunProgram\n");
}
