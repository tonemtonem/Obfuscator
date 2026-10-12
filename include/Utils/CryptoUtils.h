#ifndef OBFUSCATOR_CRYPTOUTILS_H
#define OBFUSCATOR_CRYPTOUTILS_H
#include "llvm/ADT/StringRef.h"
#include <vector>
#include<cstdint>
#include <random>
#include <array>
#include <limits>


    // padding for blocks <8 bytes, for xtea encr
    std::vector<uint8_t> padToBlockSize(const llvm::StringRef Data, const size_t BlockSize = 8);
    void xteaEncrypt(uint32_t v[2], const uint32_t key[4]);
    extern "C" void xteaDecrypt(uint32_t v[2], const uint32_t key[4]);
    std::array<uint32_t, 4> generateRandomKey();

#endif //OBFUSCATOR_CRYPTOUTILS_H
