#ifndef UTIL_H
#define UTIL_H

#define MAX_FILES 32
#define BUFFER 1000
#define ARG0 0
#define ARG1 1
#define ARG2 2
#define ARG3 3
#define NAME_ARG 4
#define SAFERUN "/home/grade-cstaley/bin/SafeRun"
#define PROC_LIM "-p30"
#define TEST_OUT "test.output.temp"
#define GCC "/usr/bin/gcc"
#define O_OPT "-o"
#define ERROR_COUNT 3
#define NO_MAKE 2

typedef struct Test {
   char *inFile;
   char *outFile;
   int timeout;
   char **args;
   struct Test *next;
} Test;

typedef struct Program {
   char *name;
   int numFiles;
   int numTests;
   char **files;
   Test *tests;
   struct Program *next;
} Program;

typedef struct {
   int testNum;
   int statuses;
} TestResult;

typedef struct {
   int numFails;
   TestResult *failedResults;
} ProgResult;





#endif
