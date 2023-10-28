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

/// If reorder
/// In-if optimizations
size_t decoder3(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    size_t entryLengths[DICT_LENGTH];
    size_t prefixStart[DICT_LENGTH]; // first string char in output
    uint8_t lastSymbol[DICT_LENGTH]; // last symbol of entry
    // init dict
    for (int i = 0; i < DICT_LENGTH; i++) {
        entryLengths[i] = 0;
        if (i < 256) {
            prefixStart[i] = 0;
            lastSymbol[i] = static_cast<char>(i);
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
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[cntCode]];
                codeLen = getBitLen(newCode + 2);
                newCode++;
            }
            oldCode = cntCode;
        } else if (cntCode > 257) {
            if (entryLengths[cntCode] > 0) {
                // met this entry
                size_t iterations = std::min(entryLengths[cntCode] - 1, outLen - outputIndex - 1);
                for (int i = 0; i < iterations; i++) {
                    out[outputIndex + i] = out[prefixStart[cntCode] + i];
                }
                if (outputIndex + iterations < outLen) { // not boundary
                    out[outputIndex + iterations] = lastSymbol[cntCode];
                    iterations++;
                }
                prefixStart[cntCode] = outputIndex; // update last pos
                outputIndex += iterations;

                // if (oldCode != static_cast<uint16_t>(-1)) { impossible for new codes
                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[cntCode]];
                codeLen = getBitLen(newCode + 2);
                newCode++;
                // }
            } else {
                entryLengths[newCode] = entryLengths[oldCode] + 1;
                prefixStart[newCode] = prefixStart[oldCode];
                lastSymbol[newCode] = out[prefixStart[oldCode]];

                size_t iterations = std::min(entryLengths[newCode] - 1, outLen - outputIndex - 1);
                for (int i = 0; i < iterations; i++) {
                    out[outputIndex + i] = out[prefixStart[newCode] + i];
                }
                if (outputIndex + iterations < outLen) {
                    out[outputIndex + iterations] = lastSymbol[newCode];
                    iterations++;
                }
                outputIndex += iterations;
                codeLen = getBitLen(newCode + 2);
                newCode++;
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
            done = true;
            break;
        }
    }
    if (!done) {
        return -1;
    }
    return outputIndex;
}