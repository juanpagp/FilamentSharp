// Foundation entry points: ABI versioning, pinned-release reporting, and the
// thread-local error model.
#include "bridge.h"

#include <cstddef>
#include <cstring>

// Injected by CMake from deps/filament.lock (single source of truth).
#ifndef FILAMENTSHARP_FILAMENT_VERSION
#  error "FILAMENTSHARP_FILAMENT_VERSION must be defined by the build"
#endif
#ifndef FILAMENTSHARP_FILAMENT_COMMIT
#  error "FILAMENTSHARP_FILAMENT_COMMIT must be defined by the build"
#endif

namespace {

// Compile-time guarantees that the pinned values are non-empty string
// literals. The strlen calls are constant-folded.
constexpr std::size_t kVersionLen = sizeof(FILAMENTSHARP_FILAMENT_VERSION);
constexpr std::size_t kCommitLen = sizeof(FILAMENTSHARP_FILAMENT_COMMIT);
static_assert(kVersionLen > 1, "pinned Filament version must not be empty");
static_assert(kCommitLen > 1, "pinned Filament commit must not be empty");

} // namespace

extern "C" {

FILAMENTSHARP_API uint32_t fs_abi_version(void) {
    return FILAMENTSHARP_ABI_VERSION;
}

FILAMENTSHARP_API const char* fs_filament_version(void) {
    FS_TRY
        return FILAMENTSHARP_FILAMENT_VERSION;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API const char* fs_filament_commit(void) {
    FS_TRY
        return FILAMENTSHARP_FILAMENT_COMMIT;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API const char* fs_get_last_error(void) {
    // Deliberately does NOT clear the state: managed code calls this after a
    // failure. It is noexcept by construction.
    return ::filamentsharp::lastError();
}

FILAMENTSHARP_API void fs_clear_last_error(void) {
    ::filamentsharp::clearLastError();
}

} // extern "C"
