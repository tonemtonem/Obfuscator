#ifndef OBFUSCATOR_MBAHELPERS_H
#define OBFUSCATOR_MBAHELPERS_H
#include "llvm/IR/IRBuilder.h"
namespace obfuscator {
    // Expands X * C (for small constant C) into C-1 repeated additions.
    llvm::Value *expandMulLinear(llvm::IRBuilder<> &B, llvm::Value *X, uint64_t C);
}
#endif //OBFUSCATOR_MBAHELPERS_H

