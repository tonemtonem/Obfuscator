#include "llvm/IR/IRBuilder.h"
#include "Utils/MBAhelpers.h"

namespace obfuscator {
    llvm::Value *expandMulLinear(llvm::IRBuilder<> &B, llvm::Value *X, uint64_t C) {
        llvm::Value *R = X;
        for (uint64_t i = 1; i < C; ++i)
            R = B.CreateAdd(R, X);
        return R;
    }
}