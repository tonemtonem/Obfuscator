#ifndef OBFUSCATOR_COMMANDLINE_H
#define OBFUSCATOR_COMMANDLINE_H
#include "llvm/Support/CommandLine.h"

// Command-line knobs shared by all passes in this plugin.
//

namespace obfuscator {

    // Probability  of replacing `add`/`sub` with a negate-based rewrite
    // (a + b -> a - (-b), a - b -> a + (-b)) in the InstructionSubstitution
    // pass.
    extern llvm::cl::opt<int> AddSubProb;

    // Probabilityof replacing `add`/`sub` with a Mixed-Boolean-Arithmetic

    extern llvm::cl::opt<int> AddMbaProb;

    // Probability of applying bogus control flow to each eligible basic
    // block in the BogusControlFlow pass.
    extern llvm::cl::opt<int> ObfuscationProbability;

}

#endif // OBFUSCATOR_COMMANDLINE_H