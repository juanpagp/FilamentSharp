// Foundation contract tests against the linked bridge: versioning and the
// thread-local error model, plus compile-time layout checks for every type
// the ABI shares with managed code.
#include <cstring>
#include <cstdio>

#include "filamentsharp/abi.h"

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

// Layout checks: the managed side marshals these enums as 32-bit ints.
static_assert(sizeof(fs_status) == 4, "fs_status layout");
static_assert(FS_OK == 0 && FS_ERROR == 1 && FS_ERROR_INVALID_ARG == 2 &&
              FS_ERROR_UNSUPPORTED == 3, "fs_status values are part of the ABI");

int main() {
    // --- ABI version -------------------------------------------------
    CHECK(fs_abi_version() == FILAMENTSHARP_ABI_VERSION);

    // --- Pinned-release reporting ------------------------------------
    const char* version = fs_filament_version();
    CHECK(version != nullptr);
    CHECK(std::strcmp(version, FS_EXPECTED_FILAMENT_VERSION) == 0);

    const char* commit = fs_filament_commit();
    CHECK(commit != nullptr);
    CHECK(std::strcmp(commit, FS_EXPECTED_FILAMENT_COMMIT) == 0);
    CHECK(std::strlen(commit) == 40);

    // --- Error model ---------------------------------------------------
    // Fresh state reads as empty, never null.
    fs_clear_last_error();
    CHECK(fs_get_last_error() != nullptr);
    CHECK(std::strcmp(fs_get_last_error(), "") == 0);

    // Reading does not consume the state.
    fs_clear_last_error();
    (void)fs_filament_version(); // success path must leave the error empty
    CHECK(std::strcmp(fs_get_last_error(), "") == 0);

    if (g_failures > 0) {
        std::fprintf(stderr, "test_foundation: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_foundation: OK (abi=%u, filament=%s, commit=%.8s...)\n",
                fs_abi_version(), fs_filament_version(), fs_filament_commit());
    return 0;
}
