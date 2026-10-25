// Registers this project's passes with LLVM's new pass manager. There are
// two independent ways they are made available:
//
//  1. By name, for explicit pipelines:
//         opt -load-pass-plugin=<this module> -passes=string-encrypt
//     (registerPipelineParsingCallback). This ONLY teaches the parser the
//     pass names; it does not place anything in any default pipeline.
//
//  2. Automatically, when the plugin is loaded into a normal clang compile:
//         clang -fpass-plugin=<this module> -O2 foo.c
//     (registerOptimizerLastEPCallback). This is what actually makes
//     `-fpass-plugin` *run* the obfuscation -- without an extension-point
//     registration, `-fpass-plugin` alone is a silent no-op, because the
//     parsing callback in (1) never inserts the passes anywhere.
//
// The passes run at OptimizerLast, i.e. after the normal optimization
// pipeline, so the optimizer cannot fold away or simplify the obfuscation
// (e.g. constant-fold a string before it is encrypted, or straighten out
// the bogus control flow) after it has been applied.
//
// This translation unit is the plugin's entry point: it is the only place
// that needs to #include the pass headers, and it is what the LLVM pass
// loader actually looks for (the `llvmGetPassPluginInfo` symbol below).
// Without a .cpp like this one, a module built only from the pass headers
// compiles into an opaque shared object that LLVM has no way to hook into.
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Compiler.h"

#include "Passes/BogusControlFlow.h"
#include "Passes/InstructionSubstitution.h"
#include "Passes/StringConstEncryption.h"

using namespace llvm;

namespace {

bool parseFunctionPassName(StringRef Name, FunctionPassManager &FPM) {
    if (Name == "bogus-cf") {
        FPM.addPass(obfuscator::BogusControlFlowPass());
        return true;
    }
    if (Name == "instr-subst") {
        FPM.addPass(obfuscator::InstructionSubstitution());
        return true;
    }
    return false;
}

bool parseModulePassName(StringRef Name, ModulePassManager &MPM) {
    if (Name == "string-encrypt") {
        MPM.addPass(obfuscator::StringConstEncryption());
        return true;
    }
    return false;
}

// Appends the full obfuscation pipeline to a module pass manager. Order:
// string encryption first (a module pass, operating on globals), then the
// per-function passes wrapped in adaptors.
void addObfuscationPipeline(ModulePassManager &MPM) {
    MPM.addPass(obfuscator::StringConstEncryption());
    MPM.addPass(createModuleToFunctionPassAdaptor(obfuscator::InstructionSubstitution()));
    MPM.addPass(createModuleToFunctionPassAdaptor(obfuscator::BogusControlFlowPass()));
}

void registerCallbacks(PassBuilder &PB) {
    // (1) Make the passes available by name for explicit `-passes=` strings.
    PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
            return parseFunctionPassName(Name, FPM);
        });
    PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
            return parseModulePassName(Name, MPM);
        });

    // (2) Run the whole pipeline automatically for `clang -fpass-plugin=...`
    //     (and `opt` runs that honor extension points). OptimizerLast fires
    //     at every optimization level, including -O0.
    // Generic trailing parameters keep this portable across LLVM versions:
    // the OptimizerLast callback is void(ModulePassManager&, OptimizationLevel)
    // up to LLVM 20, but gained a third ThinOrFullLTOPhase argument in
    // LLVM 21. `auto&&...` matches whichever signature std::function expects.
    PB.registerOptimizerLastEPCallback(
        [](ModulePassManager &MPM, auto &&...) {
            addObfuscationPipeline(MPM);
        });
}

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "Obfuscator", LLVM_VERSION_STRING, registerCallbacks};
}