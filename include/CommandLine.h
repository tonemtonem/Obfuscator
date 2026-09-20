
#ifndef OBFUSCATOR_COMMANDLINE_H
#define OBFUSCATOR_COMMANDLINE_H
#include "llvm/Support/CommandLine.h"
//to configure the specified probability of replacing the `add` instruction in CLI
static cl::opt<int> AddSubProb(
    "add-sub-prob",
    cl::init(70),
    cl::desc("Probability (%) of replacing add with sub obfuscation"));

static cl::opt<int> AddMbaProb(
    "add-mba-prob",
    cl::init(20),
    cl::desc("Probability (%) of replacing add with MBA obfuscation"));
#endif //OBFUSCATOR_COMMANDLINE_H
