#ifndef OBFUSCATOR_STRINGCONSTENCRYPTION_H
#define OBFUSCATOR_STRINGCONSTENCRYPTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
namespace llvm {
    struct StringConstEncryption : PassInfoMixin<StringConstEncryption> {
        static PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
            bool Changed = false;
            for (auto &GlobalVariable : M.globals()) {


                Changed = true;
            }
            if (!Changed)
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };
}
#endif //OBFUSCATOR_STRINGCONSTENCRYPTION_H
