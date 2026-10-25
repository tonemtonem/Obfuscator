#include "../include/CommandLine.h"

namespace obfuscator {

    llvm::cl::opt<int> AddSubProb(
        "add-sub-prob", llvm::cl::init(70),
        llvm::cl::desc("Probability (%) of replacing add/sub with a negate-based rewrite"));

    llvm::cl::opt<int> AddMbaProb(
        "add-mba-prob", llvm::cl::init(20),
        llvm::cl::desc("Probability (%) of replacing add/sub with an MBA rewrite (checked before add-sub-prob)"));

    llvm::cl::opt<int> ObfuscationProbability(
        "obfuscation-probability", llvm::cl::init(50),
        llvm::cl::desc("Probability (%) of applying bogus control flow to each eligible block"));

}