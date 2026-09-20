#ifndef OBFUSCATOR_MBAHELPERS_H
#define OBFUSCATOR_MBAHELPERS_H
#include "llvm/IR/IRBuilder.h"
namespace llvm {
    //function to change multiplication to addition
    inline Value *expandMulLinear(IRBuilder<> &B, Value *X, uint64_t C) {
        Value *R = X;
        for (uint64_t i = 1; i < C; ++i)
            R = B.CreateAdd(R, X);
        return R;
    }
}
#endif //OBFUSCATOR_MBAHELPERS_H

