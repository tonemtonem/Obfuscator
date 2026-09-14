#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace {
    //
    struct InstructionSubtitution : public PassInfoMixin<InstructionSubtitution> {
       PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
           for (BasicBlock &BB: F) {
               for (auto It = BB.begin(); It !=BB.end(); ++It) {
                   Instruction &I = *It++;
                    if (auto *Op = dyn_cast<BinaryOperator>(&I)) {
                        if (Op->getOpcode() == Instruction::Add) {
                            IRBuilder<> Builder(Op);
                            Value* LHS = Op->getOperand(0);
                            Value* RHS = Op->getOperand(1);
                            Value* NegRHS = Builder.CreateNeg(RHS);
                            Value* Sub = Builder.CreateSub(LHS, NegRHS);
                            Op->replaceAllUsesWith(Sub);
                            Op->eraseFromParent();
                        }
                    }
               }
           }
       }
    };
}