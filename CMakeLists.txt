cmake_minimum_required(VERSION 3.26)
project(lzw_tiff_decoder C CXX)

set(CMAKE_C_STANDARD 11)

add_executable(lzw_tiff_decoder lzw-decode-tester.cpp
        decoders/lzw-decoders.h
        decoders/decoderBasic.cpp
        decoders/decoder1.cpp
        decoders/decoder2.cpp
        decoders/decoder3.cpp
        decoders/decoder4.cpp
        decoders/decoder8.cpp
        decoders/decoderSplitReceive.cpp
        decoders/decoderFastWrite.cpp
        decoders/decoderNoClean.cpp
        decoders/decoderPullFromIfs.cpp
        decoders/decoderFullFastWrite.cpp
        decoders/decoderInlineAll.cpp
        decoders/decoderFinalC.cpp
)
add_executable(lzw_encode lzw-encode.cpp)

