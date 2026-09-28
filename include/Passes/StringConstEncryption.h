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
#include "llvm/IR/IRBuilder.h"
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
                if (!GlobVariable.hasInitializer() || !GlobVariable.isConstant())
                continue;
                auto *ConstDataArray = dyn_cast<ConstantDataArray>(GlobVariable.getInitializer());
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
                Constant *NewInit = ConstantDataArray::get(M.getContext(), PaddedData);
                auto *NewGV = new GlobalVariable(M, NewInit->getType(), /*isConstant=*/false,
                GlobVariable.getLinkage(), NewInit, GlobVariable.getName() + ".encrypted");
                NewGV->setAlignment(GlobVariable.getAlign());
                EncryptedGlobals[NewGV] = {Key, OriginalString.size(), PaddedData.size()};
                GlobVariable.replaceAllUsesWith(NewGV);
                ToErase.push_back(&GlobVariable);
            }

    // Pass 2: insert decryption stubs, ONCE per encrypted global
            for (auto &[NewGV, Info] : EncryptedGlobals) {
                Constant *KeyConst = ConstantDataArray::get(M.getContext(), ArrayRef<uint32_t>(Info.Key.data(), 4));
                auto *KeyGV = new GlobalVariable(M, KeyConst->getType(), true,
                GlobalValue::PrivateLinkage, KeyConst, "xtea.key");
                SmallVector<User*, 8> Users(NewGV->users());
                for (User *U : Users) {
                    auto *UserInst = dyn_cast<Instruction>(U);
                    if (!UserInst) continue; // ConstantExpr users still unhandled — see note above
                    IRBuilder<> Builder(UserInst);
                    ArrayType *BufTy = ArrayType::get(Builder.getInt8Ty(), Info.PaddedLength);
                    AllocaInst *Buf = Builder.CreateAlloca(BufTy, nullptr, "dec.buf");
                    Builder.CreateMemCpy(Buf, Buf->getAlign(), NewGV, NewGV->getAlign(), Info.PaddedLength);
                    Value *BufPtr = Buf; // already a pointer, no bitcast needed with opaque ptrs
                    Value *KeyPtr = KeyGV;
                    Builder.CreateCall(XteaDecryptFn, {BufPtr, KeyPtr});
                    UserInst->replaceUsesOfWith(NewGV, BufPtr);
                }
            }
        for (auto *GV : ToErase)
            GV->eraseFromParent();

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
    };
}
#endif //OBFUSCATOR_STRINGCONSTENCRYPTION_H
