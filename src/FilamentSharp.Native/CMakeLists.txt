# FilamentSharp.Native - the C ABI bridge shared library.
#
# Links the pinned Filament release SDK (static libraries, libc++ ABI) into a
# single self-contained shared library whose exported surface is exactly the
# fs_* functions declared in include/filamentsharp/abi.h.

# --- Pinned dependency metadata (single source of truth: deps/filament.lock)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/filament.lock" FS_LOCK_LINES)
foreach(_line IN LISTS FS_LOCK_LINES)
    if(_line MATCHES "^FILAMENT_VERSION=\"([^\"]+)\"")
        set(FS_FILAMENT_VERSION "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^FILAMENT_COMMIT=\"([^\"]+)\"")
        set(FS_FILAMENT_COMMIT "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(NOT FS_FILAMENT_VERSION OR NOT FS_FILAMENT_COMMIT)
    message(FATAL_ERROR "Could not parse FILAMENT_VERSION/FILAMENT_COMMIT from deps/filament.lock")
endif()

# --- Filament SDK location
set(FILAMENT_SDK_DIR "" CACHE PATH "Filament SDK root (lib/x86_64 + include), see scripts/fetch-filament.sh")
if(NOT FILAMENT_SDK_DIR)
    # Fall back to the environment variable resolved by scripts/env.sh.
    set(FILAMENT_SDK_DIR "$ENV{FILAMENT_SDK_DIR}")
endif()
if(NOT FILAMENT_SDK_DIR OR NOT EXISTS "${FILAMENT_SDK_DIR}/lib/x86_64/libfilament.a")
    message(FATAL_ERROR
        "Filament SDK not found. Run scripts/fetch-filament.sh (or set "
        "FILAMENT_SDK_DIR to an existing install). Looked in: '${FILAMENT_SDK_DIR}'")
endif()
message(STATUS "Filament SDK: ${FILAMENT_SDK_DIR} (${FS_FILAMENT_VERSION})")

set(FS_SDK_LIB "${FILAMENT_SDK_DIR}/lib/x86_64")

# Import the SDK as one INTERFACE target. Link order matters for static
# linking: dependents first, then their providers. This exact order was
# verified to resolve the full transitive closure of the release SDK on
# Linux x64.
add_library(filament_sdk INTERFACE)
target_include_directories(filament_sdk INTERFACE "${FILAMENT_SDK_DIR}/include")
foreach(_lib IN ITEMS filament backend bluegl bluevk webgpu_dawn filabridge
                        filaflat smol-v utils geometry shaders ibl zstd)
    if(EXISTS "${FS_SDK_LIB}/lib${_lib}.a")
        target_link_libraries(filament_sdk INTERFACE "${FS_SDK_LIB}/lib${_lib}.a")
    else()
        message(FATAL_ERROR "Missing SDK library: ${FS_SDK_LIB}/lib${_lib}.a")
    endif()
endforeach()

# SDK runtime system dependencies (X11 is required by the WebGPU/Linux
# platform code pulled in by the backend's platform factory).
find_package(Threads REQUIRED)
find_library(FS_X11_LIB X11 REQUIRED)
target_link_libraries(filament_sdk INTERFACE
    ${FS_X11_LIB}
    ${CMAKE_DL_LIBS}
    Threads::Threads
    m
)

# --- The bridge library
add_library(FilamentSharp.Native SHARED
    src/error.cpp
    src/version.cpp
    src/engine.cpp
    src/rendering.cpp
)
set_target_properties(FilamentSharp.Native PROPERTIES
    OUTPUT_NAME "FilamentSharp.Native"
    PREFIX "lib"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_features(FilamentSharp.Native PUBLIC cxx_std_17)
target_include_directories(FilamentSharp.Native
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
target_compile_definitions(FilamentSharp.Native PRIVATE
    FILAMENTSHARP_FILAMENT_VERSION="${FS_FILAMENT_VERSION}"
    FILAMENTSHARP_FILAMENT_COMMIT="${FS_FILAMENT_COMMIT}")
target_compile_options(FilamentSharp.Native PRIVATE
    -stdlib=libc++ -Wall -Wextra -Wshadow -Wno-unused-parameter)

# libc++ headers. FILAMENTSHARP_LIBCXX_INCLUDE (from scripts/env.local.sh,
# exported as an environment variable) points at a user-local libc++ when the
# toolchain is not system-installed.
if(DEFINED ENV{FILAMENTSHARP_LIBCXX_INCLUDE})
    target_include_directories(FilamentSharp.Native SYSTEM PRIVATE "$ENV{FILAMENTSHARP_LIBCXX_INCLUDE}")
endif()

# Linker: prefer lld (GNU ld is pathologically slow on the SDK archives).
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LINK_OPTIONS -fuse-ld=lld)
check_cxx_source_compiles("int main(){return 0;}" FS_HAVE_LLD)
unset(CMAKE_REQUIRED_LINK_OPTIONS)
if(FS_HAVE_LLD)
    target_link_options(FilamentSharp.Native PRIVATE -fuse-ld=lld)
else()
    message(WARNING "lld not available; falling back to the default linker (slow for the SDK archives).")
endif()

# C++ standard library linkage: static libc++ is preferred so the resulting
# .so has no libc++ runtime dependency (better deployment story for Native
# AOT applications). Falls back to dynamic libc++ when the static archives
# are not found.
set(FS_LIBCXX_LIB_DIR "$ENV{FILAMENTSHARP_LIBCXX_LIB}")
find_library(FS_LIBCXX_STATIC NAMES libc++.a PATHS ${FS_LIBCXX_LIB_DIR})
find_library(FS_LIBCXXABI_STATIC NAMES libc++abi.a PATHS ${FS_LIBCXX_LIB_DIR})
find_library(FS_LIBUNWIND_STATIC NAMES libunwind.a PATHS ${FS_LIBCXX_LIB_DIR})
if(FILAMENTSHARP_STATIC_LIBCXX AND FS_LIBCXX_STATIC AND FS_LIBCXXABI_STATIC AND FS_LIBUNWIND_STATIC)
    message(STATUS "Linking static libc++ into the bridge")
    target_link_options(FilamentSharp.Native PRIVATE -nostdlib++)
    target_link_libraries(FilamentSharp.Native PRIVATE
        ${FS_LIBCXX_STATIC} ${FS_LIBCXXABI_STATIC} ${FS_LIBUNWIND_STATIC})
else()
    message(STATUS "Static libc++ not found; linking dynamic libc++")
    if(FS_LIBCXX_LIB_DIR)
        target_link_directories(FilamentSharp.Native PRIVATE "${FS_LIBCXX_LIB_DIR}")
    endif()
    target_link_options(FilamentSharp.Native PRIVATE -stdlib=libc++)
endif()

# Keep the exported surface exactly the fs_* symbols: archive symbols
# (filament, libc++) are localized, and hidden visibility covers our own.
target_link_options(FilamentSharp.Native PRIVATE
    -Wl,--exclude-libs,ALL
    -Wl,-z,relro -Wl,-z,now)

target_link_libraries(FilamentSharp.Native PRIVATE filament_sdk)

# --- Install rules
include(GNUInstallDirs)
install(TARGETS FilamentSharp.Native
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/filamentsharp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
