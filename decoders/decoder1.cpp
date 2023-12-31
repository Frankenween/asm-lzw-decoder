#include "lzw-decoders.h"
#include <algorithm>

// slow read code
static inline uint16_t readCode1(const uint8_t *data, size_t bitsRead, int codeLen) {
    uint16_t code = 0;
    for (int i = codeLen - 1; i >= 0; i--) {
        uint16_t bit = (data[bitsRead / 8] >> (7 - bitsRead % 8)) & 1;
        code |= bit << i;
        bitsRead++;
    }
    return code;
}

static inline uint8_t getBitLen(uint16_t code) {
    return 32 - __builtin_clz(static_cast<uint32_t>(code));
}

/// Now reuse entries, no copies are done
size_t decoder1(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    size_t entryLengths[DICT_LENGTH];
    size_t prefixStart[DICT_LENGTH]; // first string char in output
    uint8_t lastSymbol[DICT_LENGTH]; // last symbol of entry
    // init dict
    for (int i = 0; i < DICT_LENGTH; i++) {
        entryLengths[i] = 0;
        if (i < 256) {
            //globalDict[i][0] = static_cast<char>(i);
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
        cntCode = readCode1(src, bitsRead, codeLen);
        bitsRead += codeLen;
        if (cntCode == CLEAR_CODE) {
            for (int i = 258; i < DICT_LENGTH; i++) {
                entryLengths[i] = 0;
            }
            codeLen = 9;
            oldCode = -1;
            newCode = 258;
        } else if (cntCode == EOI_CODE) {
            done = true;
            break;
        } else {
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

                if (oldCode != static_cast<uint16_t>(-1)) {
                    entryLengths[newCode] = entryLengths[oldCode] + 1;
                    prefixStart[newCode] = prefixStart[oldCode];
                    lastSymbol[newCode] = out[prefixStart[cntCode]];
                    codeLen = getBitLen(newCode + 2);
                    newCode++;
                }
            } else {
                //int l = entryLengths[oldCode];
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
        }
    }
    if (!done) {
        return -1;
    }
    return outputIndex;
}