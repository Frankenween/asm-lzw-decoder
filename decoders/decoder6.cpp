#include "lzw-decoders.h"
#include <algorithm>

static inline uint16_t readCodeFast(const uint8_t *data, size_t bitsRead, int codeLen) {
    size_t needBytes = (bitsRead + codeLen) / 8 - bitsRead / 8 + 1; // 2 or 3
    // TODO: try splitting on two cases
    // TODO: or BMI check
    size_t pos = bitsRead / 8;
    uint32_t result = 0;
    result |= data[pos] << (8 * (needBytes - 1));
    result |= data[pos + 1] << (8 * (needBytes - 2));
    if (needBytes == 3) { // try optimize
        result |= data[pos + 2];
    }
    size_t lowTrashBits = 8 * needBytes - (bitsRead & 7) - codeLen;
    result >>= lowTrashBits;
    result &= (1 << codeLen) - 1;
    return result;
}

static inline uint8_t getBitLen(uint16_t code) {
    return 32 - __builtin_clz(static_cast<uint32_t>(code));
}

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

/// unroll loops
size_t decoder6(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    size_t prefixStart[DICT_LENGTH]; // first string char in output
    size_t entryLengths[DICT_LENGTH];
    uint8_t lastSymbol[DICT_LENGTH]; // last symbol of entry
    // init dict
    for (int i = 0; i < DICT_LENGTH; i++) {
        entryLengths[i] = 0;
        prefixStart[i] = 0;
        if (i < 256) {
            lastSymbol[i] = static_cast<char>(i);
            entryLengths[i] = 1;
        }
    }
    uint16_t newCode = 258;
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
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[cntCode]];
                // codeLen = getBitLen(newCode + 2);
                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            }
            oldCode = cntCode;
        } else if (cntCode > 257) {
            if (entryLengths[cntCode] > 0) {
                // met this entry
                size_t iterations = std::min(entryLengths[cntCode], outLen - outputIndex) - 1;

                uint8_t *baseWrite = out + outputIndex;
                uint8_t *baseRead = out + prefixStart[cntCode];
                fastCopy(baseRead, baseWrite, iterations);

                if (outputIndex + iterations < outLen) { // not boundary
                    out[outputIndex + iterations] = lastSymbol[cntCode];
                    iterations++;
                }
                prefixStart[cntCode] = outputIndex; // update last pos
                outputIndex += iterations;

                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[cntCode]];

                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            } else {
                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[oldCode]];

                size_t iterations = std::min(entryLengths[newCode],
                                        outLen - outputIndex) - 1;

                uint8_t *baseWrite = out + outputIndex;
                uint8_t *baseRead = out + prefixStart[newCode];
                fastCopy(baseRead, baseWrite, iterations);
                if (outputIndex + iterations < outLen) {
                    out[outputIndex + iterations] = lastSymbol[newCode];
                    iterations++;
                }
                outputIndex += iterations;
                newCode++;
                codeLen += (newCode + 1) >> codeLen;
            }
            oldCode = cntCode;
        } else if (cntCode == CLEAR_CODE) {
            for (int i = 258; i < DICT_LENGTH; i++) {
                entryLengths[i] = 0;
            }
            codeLen = 9;
            oldCode = -1;
            newCode = 258;
        } else {
            break;
        }
    }
    return outputIndex;
}
