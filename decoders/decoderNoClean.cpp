#include "lzw-decoders.h"
#include <algorithm>

static void fastCopy(const uint8_t *baseRead, uint8_t *baseWrite, size_t len) {
    if (len >= 4) {
        for (size_t i = 0; i + 4 <= len; i += 4) {
            auto *dst = (uint32_t*)(baseWrite + i);
            *dst = *((uint32_t*)(baseRead + i));
        }
        for (size_t i = (len & (~3U)); i < len; i++) {
            *(baseWrite + i) = *(baseRead + i);
        }
    } else if (len == 1) {
        *(baseWrite) = *baseRead;
    } else if (len == 2) {
        *((uint16_t*) baseWrite) = *((uint16_t*) baseRead);
    } else {
        *((uint16_t*) baseWrite) = *((uint16_t*) baseRead);
        *(baseWrite + 2) = *(baseRead + 2);
    }
}

static inline uint16_t readCodeFast(const uint8_t *data, size_t bitsRead, int codeLen) {
    size_t needBytes = (bitsRead + codeLen) / 8 - bitsRead / 8 + 1; // 2 or 3
    // TODO: or BMI check
    size_t pos = bitsRead / 8;
    uint32_t result = 0;
    if (needBytes == 2) {
        result |= data[pos] << 8;
        result |= data[pos + 1];
    } else {
        result |= data[pos] << 16;
        result |= data[pos + 1] << 8;
        result |= data[pos + 2];
    }
    size_t lowTrashBits = 8 * needBytes - (bitsRead & 7) - codeLen;
    result >>= lowTrashBits;
    result &= (1 << codeLen) - 1;
    return result;
}

/// no table clean
size_t decoderNoClean(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
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

    uint16_t cntCode, oldCode = -1;
    while ((bitsRead + codeLen) < n * 8 && outputIndex < outLen) {
        cntCode = readCodeFast(src, bitsRead, codeLen);
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
            if (cntCode < newCode) {
                // met this entry
                size_t iterations = std::min(entryLengths[cntCode], outLen - outputIndex);
                uint8_t *baseWrite = out + outputIndex;
                uint8_t *baseRead = out + prefixStart[cntCode];

                fastCopy(baseRead, baseWrite, iterations);

                prefixStart[cntCode] = outputIndex; // update last pos

                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = outputIndex - entryLengths[oldCode];
                outputIndex += iterations;
                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            } else {
                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = outputIndex - entryLengths[oldCode];

                size_t iterations = std::min(entryLengths[newCode], outLen - outputIndex);
                for (int i = 0; i < iterations; i++) {
                    out[outputIndex + i] = out[prefixStart[newCode] + i];
                }
                outputIndex += iterations;
                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            }
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