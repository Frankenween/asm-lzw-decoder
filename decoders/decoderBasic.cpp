/// Test empty:            136200
/// Test new-line:          37315
/// Test single-char:       37157
/// Test 1:               2568183
/// Test 2:                 52172
/// Test 3:                 39926
/// Test 4:               1022652
/// Test big-pnm:      3025497732
/// Test shell32.dll:   388916552
/// Test big-a:         132449848

#include "lzw-decoders.h"

char globalDict[DICT_LENGTH][DICT_LENGTH];

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