# x86 ASM TIFF decompressor
Implementation of LZW compression and decompression from [TIFF standard](https://developer.adobe.com/content/dam/udp/en/open/standards/tiff/TIFF6.pdf).

Compression is implemented in naive way just for test generation. 
Compressor is a standalone application, so just run it and compress. It can be run by executing CMake `tiff_encoder` target.

Decompression was first implemented in C/Cpp, then rewritten in x86 NASM and finally optimized.

Performance can be tested by running `test_decoders` CMake target. To run test with extended buffers, target `test_out_of_bounds` should be used.

Note: CMake file is using `file(GLOB ...)` command for source files lookup, so if any files are added, renamed or removed, reloading CMake project is required.
