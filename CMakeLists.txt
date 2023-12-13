cmake_minimum_required(VERSION 3.26)
project(lzw_tiff_decoder C CXX ASM)

enable_language(ASM_NASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-m32 -no-pie")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-m32 -no-pie")
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -f elf32 -o <OBJECT> <SOURCE>")


file(GLOB asm_sources "decoders/*.s" "*.asm")
message("List of asm files: ${asm_sources}")

file(GLOB cpp_decoders "decoders/decoder*.cpp")
message("List of cpp decoders: ${cpp_decoders}")

set_property(SOURCE ${asm_sources} PROPERTY LANGUAGE ASM_NASM)

add_executable(test_decoders lzw-decode-tester.cpp
    ${asm_sources}
    ${cpp_decoders}
)

add_executable(test_out_of_bounds lzw-decode-tester.cpp
    ${asm_sources}
    ${cpp_decoders}
)

target_compile_definitions(test_out_of_bounds PRIVATE BUFF_EXTEND=4096)

add_executable(tiff_encoder lzw-encode.cpp)


