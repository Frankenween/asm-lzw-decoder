#ifndef LZW_TIFF_DECODER_LZW_DECODERS_H
#define LZW_TIFF_DECODER_LZW_DECODERS_H

#include <cstdint>
using std::size_t;

constexpr size_t DICT_LENGTH  = 4096;
constexpr uint16_t CLEAR_CODE = 256;
constexpr uint16_t EOI_CODE   = 257;

#define newDecoder(x) size_t __attribute__((noinline)) decoder##x(const uint8_t *src, size_t n, uint8_t *out, size_t outLen)

newDecoder(Basic);

newDecoder(1);

newDecoder(2);

newDecoder(3);

newDecoder(4);

newDecoder(5);

newDecoder(6);

newDecoder(7);

newDecoder(8);

newDecoder(SplitReceive);

#endif //LZW_TIFF_DECODER_LZW_DECODERS_H
