/* LZW Exansion methods */

#include "LZWExp.h"
#include "CodeSet.h"

#define EOD 256
#define INIT_NUM_BITS 9

void ResetDict(LZWExp *exp) {
   int ndx;

   DestroyCodeSet(exp->dict);

   exp->dict = CreateCodeSet(exp->recycleCode);
   for (ndx = 0; ndx < EOD; ndx++) {
      NewCode(exp->dict, ndx);
   }
   exp->lastCode = NewCode(exp->dict, 0);

   exp->numBits = INIT_NUM_BITS;
   exp->maxCode = ((EOD - 1) << 1) + 1;
}

void LZWExpInit(LZWExp *exp, DataSink sink, void *sinkState, 
 int recycleCode) {
   int ndx;

   exp->sink = sink;
   exp->sinkState = sinkState;
   exp->recycleCode = recycleCode;

   exp->dict = CreateCodeSet(recycleCode);
   for (ndx = 0; ndx < EOD; ndx++) {
      NewCode(exp->dict, ndx);
   }
   exp->lastCode = NewCode(exp->dict, 0);

   exp->numBits = INIT_NUM_BITS;
   exp->maxCode = ((EOD - 1) << 1) + 1;

   exp->EODSeen = FALSE;
   BuInit(&exp->bitUnpacker);
}

int LZWExpDecode(LZWExp *exp, UInt bits) {
   UInt ret;
   Code code;

   if (exp->EODSeen) {
      return BAD_CODE;
   }

   BuTakeData(&exp->bitUnpacker, bits);
   
   while (BuUnpack(&exp->bitUnpacker, exp->numBits, &ret)) {
      if ((exp->EODSeen && ret) || ret > exp->lastCode) {
         return BAD_CODE;
      }

      if (ret == EOD) {
         exp->EODSeen = TRUE;
         exp->sink(exp->sinkState, NULL, 0);
      }

      else if (!exp->EODSeen) {
         if (exp->lastCode == exp->maxCode) {
            exp->maxCode <<= 1;
            exp->maxCode++;
            exp->numBits++;
         }

         code = GetCode(exp->dict, ret);

         if (exp->lastCode != EOD) 
            SetSuffix(exp->dict, exp->lastCode, *code.data);

         exp->sink(exp->sinkState, code.data, code.size);

         if (exp->lastCode < exp->recycleCode - 1) {
            FreeCode(exp->dict, ret);
            exp->lastCode = ExtendCode(exp->dict, ret);
         }

         else
            ResetDict(exp);
      }
   }

   if (exp->EODSeen && exp->bitUnpacker.bitsLeft) {
      BuUnpack(&exp->bitUnpacker, exp->bitUnpacker.bitsLeft, &ret);

      if (ret)
         return BAD_CODE;
   }

   return 0;
}

int LZWExpStop(LZWExp *exp) {
   if (!exp->EODSeen) {
      return MISSING_EOD;
   }

   return 0;
}

void LZWExpDestruct(LZWExp *exp) {
   DestroyCodeSet(exp->dict);
}
