#include "llvm/IR/IRBuilder.h"
#include "../../include/Utils/MBAhelpers.h"

namespace llvm {
    //function to change multiplication to addition
    Value *expandMulLinear(IRBuilder<> &B, Value *X, uint64_t C) {
        Value *R = X;
        for (uint64_t i = 1; i < C; ++i)
            R = B.CreateAdd(R, X);
        return R;
    }
}