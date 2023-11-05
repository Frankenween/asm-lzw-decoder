#include "lzw-decoders.h"
#include <algorithm>

/// transform all loops to do-while/while
/// n >= 2, outLen > 0
size_t decoderFinalC(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    size_t entryLengths[DICT_LENGTH];
    size_t prefixStart[DICT_LENGTH]; // first string char in output
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
    uint16_t newCode = 258;
    bool done = false;
    size_t outputIndex = 0;
    int codeLen = 9;
    size_t bitsRead = 0;

    uint32_t cntCode;
    uint16_t oldCode = -1;
    do {
        // read next code
        uint8_t readPref = bitsRead & 7;
        uint8_t needBits = readPref + codeLen + 7; // 2 or 3
        //uint8_t needBytes = needBytesArr[readPref][codeLen];
        size_t pos = bitsRead >> 3;
        cntCode = 0;
        uint8_t lowTrashBits = 2;
        if (needBits <= 16) {
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
            if (pos + 3 < n) { // likely!
                cntCode = *((uint32_t*)(src + pos));
                __asm__ (
                        "bswap %0"
                        : "=r"(cntCode)
                        : "r"(cntCode)
                        );
                lowTrashBits++;
            } else {
                cntCode |= src[pos] << 16;
                cntCode |= src[pos + 1] << 8;
                cntCode |= src[pos + 2];
            }
            lowTrashBits++;
        }
        lowTrashBits = 8 * lowTrashBits - needBits + 7; // 2 regs for lea
        // PEXT with (1 << codeLen) - 1
        // No need to calculate mask. Can or with code during update
        cntCode >>= lowTrashBits;
        cntCode &= (1 << codeLen) - 1;

        bitsRead += codeLen;
        if (cntCode < 256) {
            out[outputIndex] = static_cast<uint8_t>(cntCode);
            prefixStart[cntCode] = outputIndex; // update last pos
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
            size_t iterations;
            uint8_t *baseWrite = out + outputIndex;
            uint8_t *baseRead;
            if (cntCode < newCode) {
                // met this entry
                iterations = std::min(entryLengths[cntCode], rest);
                baseRead = out + prefixStart[cntCode];
                prefixStart[cntCode] = outputIndex; // update last pos
            } else {
                if (cntCode > newCode) {
                    return -1; // wrong code
                }
                iterations = std::min(entryLengths[newCode], rest) - 1;
                out[outputIndex] = out[prefixStart[newCode]];
                baseRead = out + prefixStart[newCode] + 1;
                outputIndex++;
                baseWrite++;
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
            done = true;
            break;
        }
    } while ((bitsRead + codeLen) < n * 8 && outputIndex < outLen);
    if (!done) {
        return -1;
    }
    return outputIndex;
}