#ifndef OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H
#define OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include <random>
#include "Utils/MBAhelpers.h"
#include "CommandLine.h"

namespace obfuscator {
    // Rewrites arithmetic/bitwise instructions into equivalent.
    struct InstructionSubtitution : public llvm::PassInfoMixin<InstructionSubtitution> {
        llvm::PreservedAnalyses static run(llvm::Function &F, llvm::FunctionAnalysisManager &) {
            bool changed = false;
            std::mt19937 RNG(std::random_device{}());
            std::uniform_int_distribution<int> Dist(1, 100);

            // AddMbaProb is checked first (roll < AddMbaProb), AddSubProb
            // second (roll <= AddSubProb): the two are nested thresholds on
            // the same roll, not independent probabilities, so keep
            // AddMbaProb <= AddSubProb when configuring them.
            const int AddMbaProbVal = AddMbaProb.getValue();
            const int AddSubProbVal = AddSubProb.getValue();

            for (llvm::BasicBlock &BB: F) {
                for (auto It = BB.begin(); It != BB.end();) {
                    llvm::Instruction &I = *It++;
                    if (auto *Op = dyn_cast<llvm::BinaryOperator>(&I)) {
                        if (!Op->getType()->isIntegerTy())
                            continue;
                        switch (Op->getOpcode()) {
                            case llvm::Instruction::Add: {
                                // a+b -> a-(-b), or a+b -> (a^b) + 2*(a&b)
                                int Roll = Dist(RNG);
                                llvm::IRBuilder<> Builder(Op);
                                if (Roll < AddMbaProbVal) {
                                    llvm::Value* LHS = Op->getOperand(0);
                                    llvm::Value* RHS = Op->getOperand(1);
                                    llvm::Value* And = Builder.CreateAnd(LHS, RHS);
                                    llvm::Value* Mul = Builder.CreateMul(And, llvm::ConstantInt::get(And->getType(), 2));
                                    llvm::Value* Xor = Builder.CreateXor(LHS, RHS);
                                    llvm::Value* MBA = Builder.CreateAdd(Xor, Mul);
                                    changed = true;
                                    Op->replaceAllUsesWith(MBA);
                                    Op->eraseFromParent();
                                }
                                else if (Roll <= AddSubProbVal) {
                                    llvm::Value* LHS = Op->getOperand(0);
                                    llvm::Value* RHS = Op->getOperand(1);
                                    llvm::Value* NegRHS = Builder.CreateNeg(RHS);
                                    llvm::Value* Sub = Builder.CreateSub(LHS, NegRHS);
                                    changed = true;
                                    Op->replaceAllUsesWith(Sub);
                                    Op->eraseFromParent();
                                }
                                break;
                            }
                            case llvm::Instruction::Sub: {
                                // a-b -> a+(-b)
                                int Roll = Dist(RNG);
                                if (Roll < AddMbaProbVal) {
                                    llvm::IRBuilder<> Builder(Op);
                                    llvm::Value* LHS = Op->getOperand(0);
                                    llvm::Value* RHS = Op->getOperand(1);
                                    llvm::Value* Xor = Builder.CreateXor(LHS, RHS);
                                    llvm::Value* Not = Builder.CreateNot(LHS);
                                    llvm::Value* And = Builder.CreateAnd(Not, RHS);
                                    llvm::Value* Mul = Builder.CreateMul(And, llvm::ConstantInt::get(And->getType(), 2));
                                    llvm::Value* MBA = Builder.CreateSub(Xor, Mul);
                                    changed = true;
                                    Op->replaceAllUsesWith(MBA);
                                    Op->eraseFromParent();
                                }
                                else if (Roll <= AddSubProbVal) {
                                    llvm::IRBuilder<> Builder(Op);
                                    llvm::Value* LHS = Op->getOperand(0);
                                    llvm::Value* RHS = Op->getOperand(1);
                                    llvm::Value* NegRHS = Builder.CreateNeg(RHS);
                                    llvm::Value* Add = Builder.CreateAdd(LHS, NegRHS);
                                    changed = true;
                                    Op->replaceAllUsesWith(Add);
                                    Op->eraseFromParent();
                                }
                                break;
                            }
                            case llvm::Instruction::Mul: {
                                // for small (<5) constants, turn multiplication into addition
                                llvm::Value* LHS = Op->getOperand(0);
                                llvm::Value* RHS = Op->getOperand(1);
                                if (isa<llvm::ConstantInt>(LHS) && !isa<llvm::ConstantInt>(RHS)) std::swap(LHS, RHS);
                                auto *Const = dyn_cast<llvm::ConstantInt>(RHS);
                                if (!Const) continue;
                                const llvm::APInt &value = Const->getValue();
                                if (value.ult(2) || value.ugt(5)) continue;
                                uint64_t C = value.getZExtValue();
                                if (auto *CL = dyn_cast<llvm::ConstantInt>(LHS)) {
                                    auto *Prod = llvm::ConstantInt::get(Op->getType(), CL->getValue() * value);
                                    changed = true;
                                    Op->replaceAllUsesWith(Prod);
                                    Op->eraseFromParent();
                                    continue;
                                }
                                llvm::IRBuilder<> Builder(Op);
                                llvm::Value* Res = expandMulLinear(Builder, LHS, C);
                                changed = true;
                                Op->replaceAllUsesWith(Res);
                                Op->eraseFromParent();
                                break;
                            }
                            case llvm::Instruction::Xor: {
                                // a^b -> (a|b) - (a&b)
                                llvm::IRBuilder<> Builder(Op);
                                llvm::Value* LHS = Op->getOperand(0);
                                llvm::Value* RHS = Op->getOperand(1);
                                llvm::Value* Or = Builder.CreateOr(LHS, RHS);
                                llvm::Value* And = Builder.CreateAnd(LHS, RHS);
                                llvm::Value* Sub = Builder.CreateSub(Or, And);
                                changed = true;
                                Op->replaceAllUsesWith(Sub);
                                Op->eraseFromParent();
                                break;
                            }
                            case llvm::Instruction::Or: {
                                // a|b -> (a&b) + (a^b)
                                llvm::IRBuilder<> Builder(Op);
                                llvm::Value* LHS = Op->getOperand(0);
                                llvm::Value* RHS = Op->getOperand(1);
                                llvm::Value* And = Builder.CreateAnd(LHS, RHS);
                                llvm::Value* Xor = Builder.CreateXor(LHS, RHS);
                                llvm::Value* Add = Builder.CreateAdd(And, Xor);
                                changed = true;
                                Op->replaceAllUsesWith(Add);
                                Op->eraseFromParent();
                                break;
                            }
                            default: continue;
                        }
                    }
                }
            }
            if (!changed) return llvm::PreservedAnalyses::all();
            return llvm::PreservedAnalyses::none();
        }

        static bool isRequired() { return true; }
    };
}
#endif // OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H