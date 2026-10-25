#ifndef OBFUSCATOR_STRINGCONSTENCRYPTION_H
#define OBFUSCATOR_STRINGCONSTENCRYPTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "Utils/CryptoUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/Alignment.h"
#include <cstring>
#include <vector>
#include <array>
#include "llvm/IR/IRBuilder.h"

namespace obfuscator {

struct EncryptionInfo {
    std::array<uint32_t, 4> Key;
    size_t OrigLength;
    size_t PaddedLength;
};

static void emitXteaDecrypt(llvm::IRBuilder<> &Builder, llvm::Value *BufPtr,
                             uint64_t PaddedLength, llvm::GlobalVariable *EncGV,
                             llvm::GlobalVariable *MaskGV,
                             llvm::Instruction *InsertBefore) {
    using namespace llvm;

    // Reconstruct Key[k] = Enc[k] ^ Mask[k], once, before touching any
    // ciphertext block.
    Value *Key[4];
    for (unsigned k = 0; k < 4; ++k) {
        Value *EncPtr = Builder.CreateConstGEP2_64(EncGV->getValueType(), EncGV, 0, k, "key.enc.ptr");
        Value *MaskPtr = Builder.CreateConstGEP2_64(MaskGV->getValueType(), MaskGV, 0, k, "key.mask.ptr");
        Value *EncWord = Builder.CreateLoad(Builder.getInt32Ty(), EncPtr, "key.enc");
        Value *MaskWord = Builder.CreateLoad(Builder.getInt32Ty(), MaskPtr, "key.mask");
        Key[k] = Builder.CreateXor(EncWord, MaskWord, "key.word");
    }

    const uint64_t NumBlocks = PaddedLength / 8;
    if (NumBlocks == 0)
        return; // nothing to decrypt; padToBlockSize always pads to >=1 block

    LLVMContext &Ctx = Builder.getContext();
    Function *F = Builder.GetInsertBlock()->getParent();

    BasicBlock *Preheader = Builder.GetInsertBlock();
    BasicBlock *AfterBB = Preheader->splitBasicBlock(InsertBefore, "xtea.dec.end");
    // splitBasicBlock linked Preheader -> AfterBB with a fresh unconditional
    // branch; we replace it so Preheader enters the decrypt loop instead.
    Preheader->getTerminator()->eraseFromParent();

    BasicBlock *LoopBB = BasicBlock::Create(Ctx, "xtea.dec.loop", F, AfterBB);

    IRBuilder<> PreB(Preheader);
    PreB.CreateBr(LoopBB);

    IRBuilder<> LB(LoopBB);
    PHINode *IV = LB.CreatePHI(LB.getInt64Ty(), 2, "xtea.i");
    IV->addIncoming(LB.getInt64(0), Preheader);

    Value *ByteOff = LB.CreateMul(IV, LB.getInt64(8), "xtea.blk.off");
    Value *Ptr0 = LB.CreateGEP(LB.getInt8Ty(), BufPtr, ByteOff, "xtea.v0.ptr");
    Value *Off4 = LB.CreateAdd(ByteOff, LB.getInt64(4), "xtea.off4");
    Value *Ptr1 = LB.CreateGEP(LB.getInt8Ty(), BufPtr, Off4, "xtea.v1.ptr");


    Value *V0 = LB.CreateAlignedLoad(LB.getInt32Ty(), Ptr0, Align(1), "xtea.v0");
    Value *V1 = LB.CreateAlignedLoad(LB.getInt32Ty(), Ptr1, Align(1), "xtea.v1");

    constexpr uint32_t Delta = 0x9E3779B9u;
    uint32_t Sum = Delta * 32u;
    for (int round = 0; round < 32; ++round) {
        unsigned Idx1 = (Sum >> 11) & 3u;
        Value *Shl0 = LB.CreateShl(V0, LB.getInt32(4));
        Value *Lshr0 = LB.CreateLShr(V0, LB.getInt32(5));
        Value *T1 = LB.CreateAdd(LB.CreateXor(Shl0, Lshr0), V0);
        Value *Mix1 = LB.CreateXor(T1, LB.CreateAdd(Key[Idx1], LB.getInt32(Sum)));
        V1 = LB.CreateSub(V1, Mix1, "xtea.v1");

        Sum -= Delta;

        unsigned Idx0 = Sum & 3u;
        Value *Shl1 = LB.CreateShl(V1, LB.getInt32(4));
        Value *Lshr1 = LB.CreateLShr(V1, LB.getInt32(5));
        Value *T0 = LB.CreateAdd(LB.CreateXor(Shl1, Lshr1), V1);
        Value *Mix0 = LB.CreateXor(T0, LB.CreateAdd(Key[Idx0], LB.getInt32(Sum)));
        V0 = LB.CreateSub(V0, Mix0, "xtea.v0");
    }

    LB.CreateAlignedStore(V0, Ptr0, Align(1));
    LB.CreateAlignedStore(V1, Ptr1, Align(1));

    Value *INext = LB.CreateAdd(IV, LB.getInt64(1), "xtea.i.next");
    IV->addIncoming(INext, LoopBB);
    Value *Cond = LB.CreateICmpULT(INext, LB.getInt64(NumBlocks), "xtea.cond");
    LB.CreateCondBr(Cond, LoopBB, AfterBB);
}

struct StringConstEncryption : llvm::PassInfoMixin<StringConstEncryption> {
    static llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
        using namespace llvm;

        DenseMap<GlobalVariable*, EncryptionInfo> EncryptedGlobals;
        std::vector<GlobalVariable*> ToErase;
        bool Changed = false;

        // Pass 1: find constant string globals, encrypt their bytes on the
        // host and swap each one for a non-constant global holding the
        // ciphertext.
        for (auto &GlobVariable : M.globals()) {
            if (!GlobVariable.hasInitializer() || !GlobVariable.isConstant())
                continue;
            auto *ConstDataArray = dyn_cast<ConstantDataArray>(GlobVariable.getInitializer());
            if (!ConstDataArray || !ConstDataArray->isString()) continue;

            // Only touch globals with local (internal/private) linkage.
            if (!GlobVariable.hasLocalLinkage())
                continue;

            bool AllUsersAreInstructions = true;
            for (User *U : GlobVariable.users()) {
                if (!isa<Instruction>(U)) {
                    AllUsersAreInstructions = false;
                    break;
                }
            }
            if (!AllUsersAreInstructions)
                continue;

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

        // Pass 2: for each encrypted global, publish its (split) key once,
        // then insert a decryption stub at every use site, ONCE per use.
        for (auto &[NewGV, Info] : EncryptedGlobals) {
            std::array<uint32_t, 4> Mask = generateRandomKey();
            std::array<uint32_t, 4> EncKey;
            for (int k = 0; k < 4; ++k)
                EncKey[k] = Info.Key[k] ^ Mask[k];

            Constant *EncConst = ConstantDataArray::get(M.getContext(), ArrayRef<uint32_t>(EncKey.data(), 4));
            auto *EncGV = new GlobalVariable(M, EncConst->getType(), true,
                GlobalValue::PrivateLinkage, EncConst, "xtea.key.enc");

            Constant *MaskConst = ConstantDataArray::get(M.getContext(), ArrayRef<uint32_t>(Mask.data(), 4));
            auto *MaskGV = new GlobalVariable(M, MaskConst->getType(), true,
                GlobalValue::PrivateLinkage, MaskConst, "xtea.key.mask");

            SmallVector<User*, 8> Users(NewGV->users());
            for (User *U : Users) {
                auto *UserInst = dyn_cast<Instruction>(U);
                if (!UserInst) continue; // ConstantExpr users still unhandled -- not supported by this pass
                IRBuilder<> Builder(UserInst);
                ArrayType *BufTy = ArrayType::get(Builder.getInt8Ty(), Info.PaddedLength);
                AllocaInst *Buf = Builder.CreateAlloca(BufTy, nullptr, "dec.buf");
                Builder.CreateMemCpy(Buf, Buf->getAlign(), NewGV, NewGV->getAlign(), Info.PaddedLength);
                Value *BufPtr = Buf;

                emitXteaDecrypt(Builder, BufPtr, Info.PaddedLength, EncGV, MaskGV, UserInst);

                UserInst->replaceUsesOfWith(NewGV, BufPtr);
            }
        }

        for (auto *GV : ToErase)
            GV->eraseFromParent();

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

}
#endif // OBFUSCATOR_STRINGCONSTENCRYPTION_H