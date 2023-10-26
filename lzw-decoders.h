#ifndef LZW_TIFF_DECODER_LZW_DECODERS_H
#define LZW_TIFF_DECODER_LZW_DECODERS_H

#include <cstdint>
using std::size_t;

size_t decoderBasic(const uint8_t *src, size_t n, uint8_t *out, size_t outLen);

size_t decoder1(const uint8_t *src, size_t n, uint8_t *out, size_t outLen);

size_t decoder2(const uint8_t *src, size_t n, uint8_t *out, size_t outLen);

#endif //LZW_TIFF_DECODER_LZW_DECODERS_H
