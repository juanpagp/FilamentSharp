# FilamentSharp native build (C ABI bridge + native tests).
#
# Usage:
#   cmake -G Ninja -S . -B external/native-build -DCMAKE_BUILD_TYPE=Release \
#         -DFILAMENT_SDK_DIR=/path/to/filament-sdk
#   ninja -C external/native-build
#   ctest --test-dir external/native-build --output-on-failure
#
# The Filament release SDK is built with libc++, therefore this project must
# be compiled with clang + libc++ (see docs/building.md).
cmake_minimum_required(VERSION 3.20)

project(FilamentSharpNative VERSION 0.1.0 LANGUAGES C CXX)

option(FILAMENTSHARP_BUILD_TESTS "Build the native test suite" ON)
option(FILAMENTSHARP_STATIC_LIBCXX "Statically link libc++ into the bridge (preferred when available)" ON)

if(NOT UNIX)
    message(FATAL_ERROR
        "FilamentSharp.Native currently supports Linux only. Other platforms "
        "are documented as unsupported in docs/api-coverage.md.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "The Filament release SDK is built with libc++ (std::__1 ABI); the "
        "bridge must be compiled with clang (found: ${CMAKE_CXX_COMPILER_ID}). "
        "See docs/building.md.")
endif()

add_subdirectory(src/FilamentSharp.Native)

if(FILAMENTSHARP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/native)
endif()
