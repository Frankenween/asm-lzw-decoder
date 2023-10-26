#include <iostream>
#include "lzw-decoders.h"

using namespace std;

constexpr size_t DICT_LENGTH  = 4096;
constexpr uint16_t CLEAR_CODE = 256;
constexpr uint16_t EOI_CODE   = 257;

char globalDict[DICT_LENGTH][DICT_LENGTH];

// slow read code
static uint16_t readCode1(const uint8_t *data, size_t bitsRead, int codeLen) {
    uint16_t code = 0;
    for (int i = codeLen - 1; i >= 0; i--) {
        uint16_t bit = (data[bitsRead / 8] >> (7 - bitsRead % 8)) & 1;
        code |= bit << i;
        bitsRead++;
    }
    return code;
}

static uint16_t readCodeFast(const uint8_t *data, size_t bitsRead, int codeLen) {
    size_t needBytes = (bitsRead + codeLen) / 8 - bitsRead / 8 + 1; // 2 or 3
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

static uint8_t getBitLen(uint16_t code) {
    return 32 - __builtin_clz(static_cast<uint32_t>(code));
}

/// Basic stupid decoder
size_t decoderBasic(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
    int entryLengths[DICT_LENGTH];
    // init dict
    for (int i = 0; i < DICT_LENGTH; i++) {
        entryLengths[i] = 0;
        if (i < 256) {
            globalDict[i][0] = static_cast<char>(i);
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
            for (int i = 0; i < DICT_LENGTH; i++) {
                entryLengths[i] = 0;
                if (i < 256) {
                    globalDict[i][0] = static_cast<char>(i);
                    entryLengths[i] = 1;
                }
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
                for (int i = 0; i < entryLengths[cntCode] && outputIndex < outLen; i++) {
                    out[outputIndex] = globalDict[cntCode][i];
                    outputIndex++;
                }
                if (oldCode != static_cast<uint16_t>(-1)) {
                    int l = entryLengths[oldCode];
                    entryLengths[newCode] = l + 1;
                    for (int i = 0; i < l; i++) {
                        globalDict[newCode][i] = globalDict[oldCode][i];
                    }
                    globalDict[newCode][l] = globalDict[cntCode][0];
                    codeLen = getBitLen(newCode + 2);
                    newCode++;
                }
            } else {
                int l = entryLengths[oldCode];
                entryLengths[newCode] = l + 1;
                for (int i = 0; i < l; i++) {
                    globalDict[newCode][i] = globalDict[oldCode][i];
                }
                globalDict[newCode][l] = globalDict[oldCode][0];
                for (int i = 0; i <= l && outputIndex < outLen; i++) {
                    out[outputIndex] = globalDict[newCode][i];
                    outputIndex++;
                }
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
                size_t iterations = min(entryLengths[cntCode] - 1, outLen - outputIndex - 1);
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

                size_t iterations = min(entryLengths[newCode] - 1, outLen - outputIndex - 1);
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

/// Fast code receive
size_t decoder2(const uint8_t *src, size_t n, uint8_t *out, size_t outLen) {
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
                size_t iterations = min(entryLengths[cntCode] - 1, outLen - outputIndex - 1);
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

                size_t iterations = min(entryLengths[newCode] - 1, outLen - outputIndex - 1);
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
                size_t iterations = min(entryLengths[cntCode] - 1, outLen - outputIndex - 1);
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

                size_t iterations = min(entryLengths[newCode] - 1, outLen - outputIndex - 1);
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
