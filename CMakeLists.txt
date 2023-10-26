cmake_minimum_required(VERSION 3.26)
project(lzw_tiff_decoder C CXX)

set(CMAKE_C_STANDARD 11)

add_executable(lzw_tiff_decoder lzw-decode-tester.cpp
        lzw-decoders.cpp
        lzw-decoders.h)
add_executable(lzw_encode lzw-encode.cpp)

