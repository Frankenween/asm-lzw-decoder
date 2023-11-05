#include "lzw-decoders.h"
#include <algorithm>

/// inline all calls
size_t decoderInlineAll(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    size_t entryLengths[DICT_LENGTH];
    size_t prefixStart[DICT_LENGTH]; // first string char in output
    // init dict
    for (int i = 0; i < DICT_LENGTH; i++) {
        entryLengths[i] = 0;
        if (i < 256) {
            prefixStart[i] = 0;
            entryLengths[i] = 1;
        }
    }
    uint16_t newCode = 258;
    bool done = false;
    size_t outputIndex = 0;
    int codeLen = 9;
    size_t bitsRead = 0;

    uint32_t cntCode;
    uint16_t oldCode = -1;
    while ((bitsRead + codeLen) < n * 8 && outputIndex < outLen) {
        // read next code
        size_t needBytes = (bitsRead + codeLen) / 8 - bitsRead / 8 + 1; // 2 or 3
        size_t pos = bitsRead / 8;
        cntCode = 0;
        if (needBytes == 2) {
            // load big-endian
            cntCode |= src[pos] << 8;
            cntCode |= src[pos + 1];
        } else {
            // if pos + 3 < n, can read big-endian uint32_t
            // otherwise: unlikely run this code
            cntCode |= src[pos] << 16;
            cntCode |= src[pos + 1] << 8;
            cntCode |= src[pos + 2];
        }
        size_t lowTrashBits = 8 * needBytes - (bitsRead & 7) - codeLen;
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
                iterations = std::min(entryLengths[newCode], rest) - 1;
                out[outputIndex] = out[prefixStart[newCode]];
                baseRead = out + prefixStart[newCode] + 1;
                outputIndex++;
                baseWrite++;
            }
            if (iterations >= 4) {
                for (size_t i = 0; i + 4 <= iterations; i += 4) {
                    auto *dst = (uint32_t*)(baseWrite + i);
                    *dst = *((uint32_t*)(baseRead + i));
                }
                for (size_t i = (iterations & (~3U)); i < iterations; i++) {
                    *(baseWrite + i) = *(baseRead + i);
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
    }
    if (!done) {
        return -1;
    }
    return outputIndex;
}