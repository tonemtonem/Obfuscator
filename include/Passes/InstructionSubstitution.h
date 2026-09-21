#ifndef OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H
#define OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include <random>
#include "../Utils/MBAhelpers.h"

namespace llvm {
    //structure for modifying instructions in the code
    struct InstructionSubtitution : public PassInfoMixin<InstructionSubtitution> {
        PreservedAnalyses static run(Function &F, FunctionAnalysisManager &) {
            bool changed = false;
            std::mt19937 RNG(std::random_device{}());
            std::uniform_int_distribution<int> Dist(1, 100);
            for (BasicBlock &BB: F) {
                for (auto It = BB.begin(); It !=BB.end();) {
                    Instruction &I = *It++;
                    if (auto *Op = dyn_cast<BinaryOperator>(&I)) {
                        constexpr int ADD_SUB_PROB = 70;   // 70% for add -> neg+sub and for sub -> neg-add
                        constexpr int ADD_MBA_PROB = 20;  // 20% for add -> MBA and for sub -> MBA
                        switch (Op -> getOpcode()) {
                            case Instruction::Add:{
                                // this instruction does change a+b to a-(-b) or a + b to (a ^ b) + 2*(a&b)
                                int Roll = Dist(RNG); //calculate the probability
                                IRBuilder<> Builder(Op);
                                if (Roll < ADD_MBA_PROB) {
                                    Value* LHS = Op->getOperand(0);
                                    Value* RHS = Op->getOperand(1);
                                    Value* And = Builder.CreateAnd(LHS, RHS);
                                    Value* Mul = Builder.CreateMul(And, ConstantInt::get(And->getType(), 2));
                                    Value* Xor = Builder.CreateXor(LHS, RHS);
                                    Value* MBA = Builder.CreateAdd(Xor, Mul);
                                    changed = true;
                                    Op->replaceAllUsesWith(MBA);
                                    Op->eraseFromParent();
                                }
                                else if (Roll <= ADD_SUB_PROB) {
                                    Value* LHS = Op->getOperand(0);
                                    Value* RHS = Op->getOperand(1);
                                    Value* NegRHS = Builder.CreateNeg(RHS);
                                    Value* Sub = Builder.CreateSub(LHS, NegRHS);
                                    changed = true;
                                    Op->replaceAllUsesWith(Sub);
                                    Op->eraseFromParent();
                                }
                                break;
                            }
                            case Instruction::Sub: {
                                //this instruction does change a-b to a+(-b)
                                int Roll = Dist(RNG); //calculate the probability
                                if (Roll < ADD_MBA_PROB) {
                                    IRBuilder<> Builder(Op);
                                    Value* LHS = Op->getOperand(0);
                                    Value* RHS = Op->getOperand(1);
                                    Value* Xor = Builder.CreateXor(LHS, RHS);
                                    Value* Not = Builder.CreateNot(LHS);
                                    Value* And = Builder.CreateAnd(Not, RHS);
                                    Value* Mul = Builder.CreateMul( And, ConstantInt::get(And -> getType(), 2));
                                    Value* MBA = Builder.CreateSub(Xor, Mul);
                                    changed = true;
                                    Op->replaceAllUsesWith(MBA);
                                    Op->eraseFromParent();
                                }
                                else if (Roll <= ADD_SUB_PROB) {
                                    IRBuilder<> Builder(Op);
                                    Value* LHS = Op->getOperand(0);
                                    Value* RHS = Op->getOperand(1);
                                    Value* NegRHS = Builder.CreateNeg(RHS);
                                    Value* Add = Builder.CreateAdd(LHS, NegRHS);
                                    changed = true;
                                    Op->replaceAllUsesWith(Add);
                                    Op->eraseFromParent();
                                }
                                break;
                            }
                            case Instruction::Mul: {
                                // for small(<5) constants this change multiplication to addition
                                Value* LHS = Op->getOperand(0);
                                Value* RHS = Op->getOperand(1);
                                if (isa<ConstantInt>(LHS)&& !isa<ConstantInt>(RHS)) std::swap(LHS, RHS); // we need constant in RHS
                                auto *Const = dyn_cast<ConstantInt>(RHS);
                                if (!Const)continue;
                                const APInt &value= Const -> getValue();
                                if (value.ult(2) || value.ugt(5)) continue;
                                uint64_t C = value.getZExtValue();
                                if (auto *CL = dyn_cast<ConstantInt>(LHS)) {
                                    auto *Prod = ConstantInt::get(Op->getType(), CL->getValue() * value);
                                    changed = true;
                                    Op->replaceAllUsesWith(Prod);
                                    Op->eraseFromParent();
                                    continue;
                                }
                                IRBuilder<> Builder(Op);
                                Value* Res = expandMulLinear(Builder, LHS, C);
                                changed = true;
                                Op->replaceAllUsesWith(Res);
                                Op->eraseFromParent();
                                break;
                            }
                            case Instruction::Xor: {
                                // changes a^b to (a|b) - (a&b)
                                IRBuilder<> Builder(Op);
                                Value* LHS = Op->getOperand(0);
                                Value* RHS = Op->getOperand(1);
                                Value* Or = Builder.CreateOr(LHS, RHS);
                                Value* And = Builder.CreateAnd(LHS, RHS);
                                Value* Sub = Builder.CreateSub(Or, And);
                                changed = true;
                                Op->replaceAllUsesWith(Sub);
                                Op->eraseFromParent();
                                break;
                            }
                            case Instruction::Or: {
                                //changes a | b to (a&b) + (a^b)
                                IRBuilder <> Builder(Op);
                                Value* LHS = Op->getOperand(0);
                                Value* RHS = Op->getOperand(1);
                                Value* And = Builder.CreateAnd(LHS, RHS);
                                Value* Xor = Builder.CreateXor(LHS, RHS);
                                Value* Add = Builder.CreateAdd(And, Xor);
                                changed = true;
                                Op -> replaceAllUsesWith(Add);
                                Op -> eraseFromParent();
                                break;
                            }
                            default: continue;
                        }
                    }
                }
            }
            if (!changed) return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };
}
#endif //OBFUSCATOR_INSTRUCTIONSUBSTITUTION_H
