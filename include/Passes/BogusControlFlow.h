#ifndef OBFUSCATOR_BOGUSCONTROLFLOW_H
#define OBFUSCATOR_BOGUSCONTROLFLOW_H
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <random>
#include <unordered_map>
#include "CommandLine.h"

namespace obfuscator {

// true if block can safely be "doubled"
static bool isEligible(llvm::BasicBlock &BB) {
    if (BB.hasAddressTaken())
        return false;
    if (BB.isLandingPad())
        return false;

    llvm::Instruction *Term = BB.getTerminator();
    if (!Term)
        return false;

    // CallBrInst has multiple successors tied to inline-asm operands;
    // splitting/cloning it safely is out of scope, so skip it.
    if (isa<llvm::CallBrInst>(Term))
        return false;

    // musttail-call-then-ret sequences must not be touched: splitting the
    // return into a separate block (and cloning it) would break the
    // musttail contract.
    if (isa<llvm::ReturnInst>(Term) && BB.size() >= 2) {
        auto It = std::prev(BB.end(), 2);
        if (auto *CI = dyn_cast<llvm::CallInst>(&*It))
            if (CI->isMustTailCall())
                return false;
    }

    return true;
}

// Returns (and lazily creates) this module's opaque-predicate seed global.
// Cached per-Module rather than in a function-local static, since a single
// process may run this pass over multiple Modules (e.g. multi-TU / LTO /
// parallel compilation), and a function-local static would leak a
// GlobalVariable* from one Module into another.
static llvm::GlobalVariable *getOrCreateSeedGlobal(llvm::Module &M) {
    static std::unordered_map<llvm::Module *, llvm::GlobalVariable *> Cache;

    auto It = Cache.find(&M);
    if (It != Cache.end())
        return It->second;

    auto *Ty = llvm::Type::getInt32Ty(M.getContext());
    auto *Opaque = new llvm::GlobalVariable(
        M, Ty, /*isConstant=*/false, llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(Ty, 0x9E3779B9), "__bcf_seed");
    Opaque->setAlignment(llvm::Align(4));

    Cache[&M] = Opaque;
    return Opaque;
}

// Makes a predicate that is opaque to the optimizer, but always evaluates
// to true: (X * (X + 1)) % 2 == 0, where X is a global variable that is
// incremented on each call.
static llvm::Value *buildOpaquePredicate(llvm::IRBuilder<> &B, llvm::Module &M) {
    llvm::GlobalVariable *Opaque = getOrCreateSeedGlobal(M);
    auto *I32 = B.getInt32Ty();

    llvm::Value *X = B.CreateLoad(I32, Opaque, /*isVolatile=*/true, "x");
    llvm::Value *XPlus1 = B.CreateAdd(X, B.getInt32(1));
    llvm::Value *Prod = B.CreateMul(X, XPlus1);
    llvm::Value *Bit = B.CreateAnd(Prod, B.getInt32(1));

    // Persist the incremented value so the global actually evolves across
    // calls, rather than staying a constant the optimizer (or a reverse
    // engineer) can trivially fold away.
    B.CreateStore(XPlus1, Opaque, /*isVolatile=*/true);

    return B.CreateICmpEQ(Bit, B.getInt32(0), "opaque.cond");
}

static void applyBogusControlFlow(llvm::BasicBlock &BB, std::mt19937 &RNG) {
    llvm::Function &F = *BB.getParent();
    llvm::Module &M = *F.getParent();

    // 1. Split the terminator into its own block: BB gets an unconditional
    //    br to Tail, and Tail contains only the original terminator.
    llvm::Instruction *Term = BB.getTerminator();
    llvm::BasicBlock *Tail = BB.splitBasicBlock(Term, BB.getName() + ".bcf.tail");

    // 2. Clone Tail -- this gives an alternate (bogus) branch that never
    //    actually executes, but looks valid.
    //    Tail consists only of the terminator, using values defined higher
    //    up the dominator tree (in BB or earlier), so
    //    remapInstructionsInBlocks is sufficient; no PHIs are needed here.
    llvm::ValueToValueMapTy VMap;
    llvm::BasicBlock *AltTail = llvm::CloneBasicBlock(Tail, VMap, ".bcf.alt", &F);
    llvm::remapInstructionsInBlocks({AltTail}, VMap);

    // 3. PHI nodes in Tail's successors must get an additional incoming
    //    path from AltTail with the same value as from Tail.
    for (llvm::BasicBlock *Succ : successors(Tail)) {
        for (llvm::PHINode &PN : Succ->phis()) {
            llvm::Value *IncomingFromTail = PN.getIncomingValueForBlock(Tail);
            PN.addIncoming(IncomingFromTail, AltTail);
        }
    }

    // 4. Replace the unconditional BB -> Tail branch with a conditional
    //    branch on the opaque predicate: BB -> Tail | AltTail.
    llvm::Instruction *OldBr = BB.getTerminator();
    llvm::IRBuilder<> B(OldBr);
    llvm::Value *Cond = buildOpaquePredicate(B, M);

    // Shuffle which branch is declared "true", so as not to give an
    // analyzer a constant pattern to key on.
    bool Swap = (RNG() & 1) != 0;
    llvm::BasicBlock *TrueDst = Swap ? AltTail : Tail;
    llvm::BasicBlock *FalseDst = Swap ? Tail : AltTail;
    llvm::Value *FinalCond = Swap ? B.CreateNot(Cond) : Cond;

    B.CreateCondBr(FinalCond, TrueDst, FalseDst);
    OldBr->eraseFromParent();
}

struct BogusControlFlowPass : llvm::PassInfoMixin<BogusControlFlowPass> {
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &) {
        if (F.isDeclaration())
            return llvm::PreservedAnalyses::all();

        std::mt19937 RNG(std::hash<std::string>{}(F.getName().str()));
        std::uniform_int_distribution<int> Dist(1, 100);
        const int ProbVal = ObfuscationProbability.getValue();

        // Collect the block list up front -- new blocks added along the way
        // should not themselves be processed.
        llvm::SmallVector<llvm::BasicBlock *, 32> Targets;
        for (llvm::BasicBlock &BB : F)
            if (isEligible(BB) && Dist(RNG) <= ProbVal)
                Targets.push_back(&BB);

        for (llvm::BasicBlock *BB : Targets)
            applyBogusControlFlow(*BB, RNG);

#ifndef NDEBUG
        if (llvm::verifyFunction(F, &llvm::errs()))
            llvm::report_fatal_error("BogusControlFlow broke the IR of function " + F.getName());
#endif

        return Targets.empty() ? llvm::PreservedAnalyses::all()
                                : llvm::PreservedAnalyses::none();
    }

    static bool isRequired() { return true; }
};

} // namespace obfuscator

#endif // OBFUSCATOR_BOGUSCONTROLFLOW_H