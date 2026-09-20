#ifndef OBFUSCATOR_CRYPTOUTILS_H
#define OBFUSCATOR_CRYPTOUTILS_H
#include "llvm/ADT/StringRef.h"
#include <vector>
namespace llvm {
    // padding for blocks <8 bytes, for XTEA encryption
    inline std::vector<uint8_t> padToBlockSize(const StringRef Data, const size_t BlockSize = 8){
        std::vector<uint8_t> Out(Data.begin(), Data.end());
        size_t PadLen = BlockSize - (Out.size() % BlockSize);
        if (PadLen == 0) PadLen = BlockSize;
        for (size_t i = 0; i < PadLen; i++)
        Out.push_back(static_cast<uint8_t>(PadLen));
        return Out;
    }
}
#endif //OBFUSCATOR_CRYPTOUTILS_H
