cmake_minimum_required(VERSION 3.26)
project(lzw_tiff_decoder C CXX)

set(CMAKE_C_STANDARD 11)

add_compile_options(-fno-lto)

add_executable(lzw_tiff_decoder lzw-decode-tester.cpp
        decoders/lzw-decoders.h
        decoders/decoderBasic.cpp
        decoders/decode1.cpp
        decoders/decoder2.cpp
        decoders/decoder3.cpp
        decoders/decoder4.cpp
        decoders/decoder5.cpp
        decoders/decoder6.cpp
        decoders/decoder7.cpp)
add_executable(lzw_encode lzw-encode.cpp)

