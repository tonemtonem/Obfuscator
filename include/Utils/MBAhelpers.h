#ifndef OBFUSCATOR_MBAHELPERS_H
#define OBFUSCATOR_MBAHELPERS_H
#include "llvm/IR/IRBuilder.h"
namespace llvm {
    //function to change multiplication to addition
    Value *expandMulLinear(IRBuilder<> &B, Value *X, uint64_t C);
}
#endif //OBFUSCATOR_MBAHELPERS_H

