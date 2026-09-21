#ifndef OBFUSCATOR_CRYPTOUTILS_H
#define OBFUSCATOR_CRYPTOUTILS_H
#include "llvm/ADT/StringRef.h"
#include <vector>
#include<cstdint>
#include <random>
#include <array>
#include <limits>


    // padding for blocks <8 bytes, for XTEA encryption
    inline std::vector<uint8_t> padToBlockSize(const StringRef Data, const size_t BlockSize = 8){
        std::vector<uint8_t> Out(Data.begin(), Data.end());
        size_t PadLen = BlockSize - (Out.size() % BlockSize);
        if (PadLen == 0) PadLen = BlockSize;
        for (size_t i = 0; i < PadLen; i++)
        Out.push_back(static_cast<uint8_t>(PadLen));
        return Out;
    }
    inline void xteaEncrypt(uint32_t v[2], const uint32_t key[4]) {
        uint32_t v0 = v[0], v1 = v[1];
        uint32_t sum = 0;
        uint32_t delta = 0x9E3779B9;
        for (uint32_t i = 0; i < 32; i++) {
            v0 += ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + key[sum & 3]);
            sum += delta;
            v1 += ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + key[(sum >> 11) & 3]);
        }
        v[0] = v0;
        v[1] = v1;
    }
    inline void xteaDecrypt(uint32_t v[2], const uint32_t key[4]) {
        uint32_t v0 = v[0], v1 = v[1];
        uint32_t delta = 0x9E3779B9;
        uint32_t sum = delta * 32;
        for (uint32_t i = 0; i < 32; i++) {
            v1 -= ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + key[(sum >> 11) & 3]);
            sum -= delta;
            v0 -= ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + key[sum & 3]);
        }
        v[0] = v0;
        v[1] = v1;
    }
    inline std::array<uint32_t, 4> generateRandomKey() {
        static std::mt19937 RNG(std::random_device{}());
        std::uniform_int_distribution<uint32_t> Dist(0, std::numeric_limits<uint32_t>::max());
        return {Dist(RNG), Dist(RNG), Dist(RNG), Dist(RNG)};

    }

#endif //OBFUSCATOR_CRYPTOUTILS_H
