#ifndef OBFUSCATOR_STRINGCONSTENCRYPTION_H
#define OBFUSCATOR_STRINGCONSTENCRYPTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "../Utils/CryptoUtils.h"
#include "llvm/IR/Constants.h"
namespace llvm {
    struct StringConstEncryption : PassInfoMixin<StringConstEncryption> {
        static PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
            bool Changed = false;
            for (auto &GlobalVariable : M.globals()) {
                if (!GlobalVariable.hasInitializer() || !GlobalVariable.isConstant()) continue;
                if (const auto* ConstDataArray = dyn_cast<ConstantDataArray*>(GlobalVariable.hasInitializer())) {
                    if (!ConstDataArray->isString()) continue;
                    StringRef OriginalString = ConstDataArray->getAsString();
                    std::vector<uint8_t> PaddedData = padToBlockSize(OriginalString);
                    std::array<uint32_t, 4> Key = generateRandomKey();
                    for (size_t i = 0; i < PaddedData.size(); i += 8) {
                        uint32_t Block[2];
                        std::memcpy(Block, &PaddedData[i], 8);
                        xteaEncrypt(Block, Key.data());
                        std::memcpy(&PaddedData[i], Block, 8);
                    }
                    std::vector<Constant*> EncryptedChars;
                    for (uint8_t Byte : PaddedData) {
                        EncryptedChars.push_back(ConstantInt::get(Type::getInt8Ty(M.getContext()), Byte));
                    }
                    Constant* NewInit = ConstantDataArray::get(M.getContext(), EncryptedChars);
                    GlobalVariable.setInitializer(NewInit);
                    GlobalVariable.setConstant(false);  // Make it mutable so we can decrypt at runtime
                }
                Changed = true;
            }
            if (!Changed)
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };
}
#endif //OBFUSCATOR_STRINGCONSTENCRYPTION_H
