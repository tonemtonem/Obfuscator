#ifndef OBFUSCATOR_STRINGCONSTENCRYPTION_H
#define OBFUSCATOR_STRINGCONSTENCRYPTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "../Utils/CryptoUtils.h"
#include "llvm/IR/Constants.h"
#include <cstring>
#include <vector>
#include <array>

struct EncryptionInfo {
    std::array<uint32_t, 4> Key;
    size_t OrigLength;
    size_t PaddedLength;
};
namespace llvm {
    struct StringConstEncryption : PassInfoMixin<StringConstEncryption> {
        static PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
            DenseMap<GlobalVariable*, EncryptionInfo> EncryptedGlobals;
            std::vector<GlobalVariable*> ToErase;
            bool Changed = false;
            for (auto &GlobVariable : M.globals()) {
                if (!GlobVariable.hasInitializer() || !GlobVariable.isConstant()) continue;
                auto* ConstDataArray = dyn_cast<ConstantDataArray>(GlobVariable.getInitializer());
                if (!ConstDataArray || !ConstDataArray->isString()) continue;
                Changed = true;
                std::array<uint32_t, 4> Key = generateRandomKey();
                StringRef OriginalString = ConstDataArray->getAsString();
                std::vector<uint8_t> PaddedData = padToBlockSize(OriginalString);
                for (size_t i = 0; i < PaddedData.size(); i += 8) {
                    uint32_t Block[2];
                    std::memcpy(Block, &PaddedData[i], 8);
                    xteaEncrypt(Block, Key.data());
                    std::memcpy(&PaddedData[i], Block, 8);
                }
                Constant* NewInit = ConstantDataArray::get(M.getContext(), PaddedData);
                GlobalVariable *NewGV = new GlobalVariable(
                    M, NewInit ->getType(), false,
                    GlobVariable.getLinkage(), NewInit,
                    GlobVariable.getName() + ".encrypted"
                    );
                EncryptedGlobals[NewGV] = {Key, OriginalString.size(), PaddedData.size()};
                NewGV->setAlignment(GlobVariable.getAlign());
                GlobVariable.replaceAllUsesWith(NewGV);
                ToErase.push_back(&GlobVariable);
            }
            for (auto* Gv : ToErase) {
                Gv->eraseFromParent();
            }
            if (!Changed)
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };
}
#endif //OBFUSCATOR_STRINGCONSTENCRYPTION_H
