# Native test suite for the FilamentSharp bridge.
#
# Layers:
#   * test_abi_c      - compiles the public header as plain C (ABI portability
#                       proof), then dlopen/dlsym every documented symbol.
#   * test_foundation - links the bridge directly: version contracts, error
#                       model, struct/enum layout checks.
#   * bridge_smoke    - minimal native smoke-test executable (prints version
#                       info, exit code 0 = bridge loadable and callable).

# The version expectations are shared with the bridge build (deps/filament.lock).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/filament.lock" FS_LOCK_LINES)
foreach(_line IN LISTS FS_LOCK_LINES)
    if(_line MATCHES "^FILAMENT_VERSION=\"([^\"]+)\"")
        set(FS_FILAMENT_VERSION "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^FILAMENT_COMMIT=\"([^\"]+)\"")
        set(FS_FILAMENT_COMMIT "${CMAKE_MATCH_1}")
    endif()
endforeach()

# ---------------------------------------------------------------- test_abi_c
add_executable(test_abi_c test_abi_c.c)
target_include_directories(test_abi_c PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/FilamentSharp.Native/include)
target_link_libraries(test_abi_c PRIVATE ${CMAKE_DL_LIBS})
target_compile_definitions(test_abi_c PRIVATE
    FILAMENTSHARP_TEST_LIBRARY="$<TARGET_FILE:FilamentSharp.Native>"
    FS_EXPECTED_FILAMENT_VERSION="${FS_FILAMENT_VERSION}")
target_compile_options(test_abi_c PRIVATE -Wall -Wextra)

add_test(NAME abi_c COMMAND test_abi_c)

# ----------------------------------------------------------- test_foundation
add_executable(test_foundation test_foundation.cpp)
target_link_libraries(test_foundation PRIVATE FilamentSharp.Native)
target_compile_definitions(test_foundation PRIVATE
    FS_EXPECTED_FILAMENT_VERSION="${FS_FILAMENT_VERSION}"
    FS_EXPECTED_FILAMENT_COMMIT="${FS_FILAMENT_COMMIT}")
target_compile_features(test_foundation PRIVATE cxx_std_17)
target_compile_options(test_foundation PRIVATE -stdlib=libc++ -Wall -Wextra)

add_test(NAME foundation COMMAND test_foundation)

# ---------------------------------------------------------------- test_engine
add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE FilamentSharp.Native)
target_compile_features(test_engine PRIVATE cxx_std_17)
target_compile_options(test_engine PRIVATE -stdlib=libc++ -Wall -Wextra)

add_test(NAME engine COMMAND test_engine)

# ------------------------------------------------------------- test_rendering
add_executable(test_rendering test_rendering.cpp)
target_link_libraries(test_rendering PRIVATE FilamentSharp.Native)
target_compile_features(test_rendering PRIVATE cxx_std_17)
target_compile_options(test_rendering PRIVATE -stdlib=libc++ -Wall -Wextra)

add_test(NAME rendering COMMAND test_rendering)

# ---------------------------------------------------------------- test_scene
add_executable(test_scene test_scene.cpp)
target_link_libraries(test_scene PRIVATE FilamentSharp.Native)
target_compile_features(test_scene PRIVATE cxx_std_17)
target_compile_options(test_scene PRIVATE -stdlib=libc++ -Wall -Wextra)

add_test(NAME scene COMMAND test_scene)

# ------------------------------------------------------------- bridge_smoke
add_executable(bridge_smoke bridge_smoke.cpp)
target_link_libraries(bridge_smoke PRIVATE FilamentSharp.Native)
target_compile_features(bridge_smoke PRIVATE cxx_std_17)
target_compile_options(bridge_smoke PRIVATE -stdlib=libc++)

add_test(NAME bridge_smoke COMMAND bridge_smoke)

# Every test that links the bridge needs to find the shared library; the dlopen
# test needs it loadable by soname. Both are satisfied by prepending the
# library's own directory to LD_LIBRARY_PATH.
set_tests_properties(abi_c foundation bridge_smoke engine rendering scene PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:FilamentSharp.Native>:$ENV{LD_LIBRARY_PATH}")
