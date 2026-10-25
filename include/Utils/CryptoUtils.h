#ifndef OBFUSCATOR_CRYPTOUTILS_H
#define OBFUSCATOR_CRYPTOUTILS_H
#include "llvm/ADT/StringRef.h"
#include <vector>
#include <cstdint>
#include <random>
#include <array>
#include <limits>

namespace obfuscator {

    // Padding for blocks <8 bytes, for XTEA encryptio.
    std::vector<uint8_t> padToBlockSize(llvm::StringRef Data, size_t BlockSize = 8);

    // Host-side-only. Used while the pass is running, to encrypt the string
    // literal's bytes before they are written into the binary as a global
    // constant. There is no IR equivalent of this function and none is
    // needed: encryption happens once, at compile time, on the host.
    void xteaEncrypt(uint32_t v[2], const uint32_t key[4]);

    std::array<uint32_t, 4> generateRandomKey();

}

#endif // OBFUSCATOR_CRYPTOUTILS_H