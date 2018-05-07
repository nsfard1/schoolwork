#include "BitUnpacker.h"

void BuInit(BitUnpacker *bitUnpacker) {
   bitUnpacker->curData = 0;
   bitUnpacker->nextData = 0;
   bitUnpacker->bitsLeft = 0;
   bitUnpacker->validNext = 0;
}

void BuTakeData(BitUnpacker *bitUnpacker, UInt data) {
   bitUnpacker->nextData = data;
   bitUnpacker->validNext = 1;
}

int BuUnpack(BitUnpacker *bitUnpacker, int size, UInt *ret) {
   int result = 1;
                                                             
   if (size <= bitUnpacker->bitsLeft) {
      *ret = bitUnpacker->curData >> (bitUnpacker->bitsLeft - size)
       & ~((UINT_MASK << (size - 1)) << 1);                  
      bitUnpacker->bitsLeft -= size;
   }                                                         
                                                             
   else if (bitUnpacker->validNext) {
      if (bitUnpacker->bitsLeft) {
         *ret = (bitUnpacker->curData & ~(UINT_MASK << 
          bitUnpacker->bitsLeft)) << (size - 
          bitUnpacker->bitsLeft);                          
         bitUnpacker->bitsLeft += UINT_SIZE - size;
         *ret += bitUnpacker->nextData >> bitUnpacker->bitsLeft
          & ~(UINT_MASK << size);                            
      }                                                      
      else {
         bitUnpacker->bitsLeft = UINT_SIZE;
         *ret = bitUnpacker->nextData >> (UINT_SIZE - size)
          & ~((UINT_MASK << (size - 1)) << 1);               
         bitUnpacker->bitsLeft -= size;
      }                                                      
                                                             
      bitUnpacker->curData = bitUnpacker->nextData;
      bitUnpacker->validNext = 0;
   }                                                         
                                                             
   else
      result = 0;
                                                             
   return result;
}



