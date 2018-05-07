#include <stdlib.h>
#include <string.h>

#include "CodeSet.h"

typedef struct CodeEntry {                               
   unsigned short count;                                 
   unsigned char data;                                   
   unsigned char *seq;                                   
   struct CodeEntry *prev;                               
} CodeEntry;                                             
                                                         
typedef struct {                                         
   CodeEntry *codes;                                     
   int index;                                            
} CodeSet;                                               
                                                         
int GetSize(CodeEntry *codeEntry) {
   int size = 0;

   while ((codeEntry = codeEntry->prev))
      size++;
                                                         
   return size;
}                                                        
                                                         
void *CreateCodeSet(int numCodes) {
   CodeSet *set = malloc(sizeof(CodeSet));

   set->codes = malloc(numCodes * sizeof(CodeEntry));
   set->index = 0;
                                                         
   return set;
}                                                        
                                                         
int NewCode(void *codeSet, char val) {
   CodeSet *set = (CodeSet *)codeSet;
   int ndx = set->index++;
                                                         
   set->codes[ndx].data = val;
   set->codes[ndx].count = 0;
   set->codes[ndx].seq = NULL;
   set->codes[ndx].prev = NULL;
   return ndx;
}                                                        
                                                         
int ExtendCode(void *codeSet, int oldCode) {
   CodeSet *set = (CodeSet *)codeSet;
   int ndx = NewCode(set, 0);
                                                         
   set->codes[ndx].prev = &(set->codes[oldCode]);
                                                         
   return ndx;
}                                                        
                                                         
void SetSuffix(void *codeSet, int code, char suffix) {
   CodeEntry *codeEntry = &(((CodeSet *)codeSet)->codes[code]);

   codeEntry->data = suffix;
                                                         
   if (codeEntry->count)
      codeEntry->seq[GetSize(codeEntry)] = suffix;
                                                         
}                                                        
                                                         
Code GetCode(void *codeSet, int code) {
   CodeEntry *codeEntry = &((CodeSet *)codeSet)->codes[code];
   Code result;                                        
   int size = GetSize(codeEntry);
   CodeEntry *temp = codeEntry;
                                                         
   result.size = ++size;
                                                         
   if (!temp->count) {
      codeEntry->seq = malloc(size);
      while (size && !temp->count) {
         codeEntry->seq[--size] = temp->data;
         temp = temp->prev;
      }                                                  
                                                         
      if (size) {
         memcpy(codeEntry->seq, temp->seq, size);
      }                                                  
   }                                                     
                                                         
   result.data = codeEntry->seq;
   codeEntry->count++;
                                                         
   return result;
}                                                        
                                                         
void FreeCode(void *codeSet, int code) {
   CodeEntry *codeEntry = &((CodeSet *)codeSet)->codes[code];
                                                         
   if (!--(codeEntry->count)) {
      free(codeEntry->seq);
      codeEntry->seq = NULL;
   }                                                     
}                                                        
                                                         
void DestroyCodeSet(void *codeSet) {
   CodeSet *set = (CodeSet *)codeSet;
 
   while (set->index--) {
      if (set->codes[set->index].seq)
         free(set->codes[set->index].seq);
   }                                                     
   free(set->codes);
   free(codeSet);
}   
