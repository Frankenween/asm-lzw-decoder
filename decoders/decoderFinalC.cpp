#include "lzw-decoders.h"
#include <algorithm>

/// transform all loops to do-while/while
/// n >= 2, outLen > 0
size_t decoderFinalC(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    /// dst - edi
    /// src - esi
    size_t entryLengths[DICT_LENGTH];
    size_t prefixStart[DICT_LENGTH]; // first string char in output
    /// ST_TOP here
    // init dict
    {
        int i = 0;
        do {
            entryLengths[i] = 1;
            i++;
        } while (i < 256);
        do {
            entryLengths[i] = 0;
            i++;
        } while (i < DICT_LENGTH);
    }
    uint16_t newCode = 258; /// dx
    size_t outputIndex = 0; /// ebp
    int codeLen = 9; /// on stack [ST_TOP - 8]
    size_t bitsRead = 0; /// on stack [ST_TOP - 4]

    uint32_t cntCode; /// eax
    uint16_t oldCode = -1; /// edx >> 16
    do {
        // read next code
        size_t pos = bitsRead >> 3; /// ecx
        cntCode = 0;
        uint16_t bxVal = (bitsRead & 7) + codeLen + 7; /// bh
        if (bxVal <= 16) {
            // load big-endian
            // xchg but keep cntCode in clever place
//            cntCode = *((uint16_t*)(src + pos));
//            __asm__ __volatile__ (
//                    "xchg %%ah, %%al"
//                    :
//                    : "eax"(cntCode)
//                    );
            cntCode |= src[pos] << 8;
            cntCode |= src[pos + 1];
        } else {
            // if pos + 3 < n, can read big-endian uint32_t
            // otherwise: unlikely run this code
            /// here add pos + 3 and cmp
            if (pos + 3 < n) { // likely!
                cntCode = *((uint32_t*)(src + pos));
                __asm__ (
                        "bswap %0"
                        : "=r"(cntCode)
                        : "r"(cntCode)
                        );
                bxVal -= 8;
            } else {
                cntCode |= src[pos] << 16;
                cntCode |= src[pos + 1] << 8;
                cntCode |= src[pos + 2];
            }
            bxVal -= 8;
        }
        bxVal = 23 - bxVal;
        // PEXT with (1 << codeLen) - 1
        // No need to calculate mask. Can or with code during update
        cntCode >>= bxVal;
        cntCode &= (1 << codeLen) - 1;

        bitsRead += codeLen;
        if (outputIndex == outLen && (cntCode != EOI_CODE && cntCode != CLEAR_CODE)) {
            return -1;
        }
        if (cntCode < 256) {
            out[outputIndex] = static_cast<uint8_t>(cntCode);
            //prefixStart[cntCode] = outputIndex; // update last pos
            outputIndex++;
            if (oldCode != static_cast<uint16_t>(-1)) {
                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = outputIndex - entryLengths[newCode];
                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            }
            oldCode = cntCode;
        } else if (cntCode > 257) {
            entryLengths[newCode] = entryLengths[oldCode] + 1;
            prefixStart[newCode] = outputIndex - entryLengths[oldCode];

            size_t rest = outLen - outputIndex;
            size_t iterations; // ecx
            uint8_t *baseWrite = out + outputIndex;
            uint8_t *baseRead; // ebx
            iterations = std::min(entryLengths[cntCode], rest);
            baseRead = out + prefixStart[cntCode];

            iterations--;
            out[outputIndex] = out[prefixStart[cntCode]];
            baseRead++;
            outputIndex++;
            baseWrite++;
            if (cntCode > newCode) {
                return -1; // wrong code
            }
            if (iterations >= 4) {
                size_t i = 0;
                do {
                    auto *dst = (uint32_t*)(baseWrite + i);
                    *dst = *((uint32_t*)(baseRead + i));
                    i += 4;
                } while (i < iterations);
                i = iterations & (~3U);
                while (i < iterations) {
                    *(baseWrite + i) = *(baseRead + i);
                    i++;
                }
            } else if (iterations == 1) {
                *(baseWrite) = *baseRead;
            } else if (iterations == 2) {
                *((uint16_t*) baseWrite) = *((uint16_t*) baseRead);
            } else {
                *((uint16_t*) baseWrite) = *((uint16_t*) baseRead);
                *(baseWrite + 2) = *(baseRead + 2);
            }
            outputIndex += iterations;
            newCode++;
            codeLen += (newCode + 1) >> codeLen;
            oldCode = cntCode;
        } else if (cntCode == CLEAR_CODE) {
            codeLen = 9;
            oldCode = -1;
            newCode = 258;
        } else {
            return outputIndex;
            // ret
        }
    } while ((bitsRead + codeLen) < n * 8 && outputIndex <= outLen); // if failed - decoding failed
    return -1;
}