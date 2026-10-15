// FilamentSharp native bridge - public C ABI.
//
// This header is the single source of truth for the boundary between the
// managed FilamentSharp library and the native bridge. It must remain valid
// ISO C (compile it with a C compiler - tests/native/test_abi_c.c does exactly
// that) and must never expose C++ types, templates, exceptions, or STL types.
//
// Conventions (apply to every fs_* function added in later revisions):
//
//   * All symbols are extern "C" with default visibility; everything else in
//     the shared library is hidden. Exported names are stable and prefixed
//     `fs_`.
//   * All integers crossing the boundary are fixed-width (stdint.h).
//   * Handles are opaque pointers to incomplete struct types; one distinct
//     struct tag per resource type. Never dereference them from managed code.
//   * Functions returning handles return NULL on failure and set the
//     thread-local error state (fs_get_last_error).
//   * Functions returning fs_status return FS_OK on success and an error code
//     plus thread-local error text on failure.
//   * No C++ exception ever crosses this boundary; every entry point is
//     wrapped in a catch-all that converts exceptions into FS_ERROR + error
//     text.
//   * Strings are NUL-terminated UTF-8. Strings owned by the bridge remain
//     valid until the next bridge call on the same thread.
//   * Buffer parameters are always accompanied by explicit element/byte
//     lengths. Callers own all input memory.
//   * Threading: Filament Engine objects are not thread-safe. All fs_*
//     functions must be externally synchronized by the caller (typically:
//     called from a single rendering thread). The bridge itself adds no
//     internal locking except for the engine ownership registry, which is
//     internally synchronized.
//
// ABI version history:
//   1 - initial release: versioning, error model.
#ifndef FILAMENTSHARP_ABI_H
#define FILAMENTSHARP_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version of this C ABI. Managed code refuses to load a bridge with a
   different value. Bump on any breaking change and record it above. */
#define FILAMENTSHARP_ABI_VERSION 1

/* Symbol visibility (dllexport/dllimport on Windows, default visibility
   elsewhere). */
#if defined(_WIN32)
#  if defined(FILAMENTSHARP_BUILDING)
#    define FILAMENTSHARP_API __declspec(dllexport)
#  else
#    define FILAMENTSHARP_API __declspec(dllimport)
#  endif
#else
#  define FILAMENTSHARP_API __attribute__((visibility("default")))
#endif

/* Status codes returned by fs_* functions. */
enum fs_status {
    FS_OK = 0,              /* success */
    FS_ERROR = 1,           /* generic failure; retrieve details via fs_get_last_error */
    FS_ERROR_INVALID_ARG = 2, /* a caller-supplied argument was null/invalid */
    FS_ERROR_UNSUPPORTED = 3  /* the request is not supported in this build */
};
typedef enum fs_status fs_status;

/* ------------------------------------------------------------------ */
/* Versioning                                                          */
/* ------------------------------------------------------------------ */

/* Returns FILAMENTSHARP_ABI_VERSION of this bridge. */
FILAMENTSHARP_API uint32_t fs_abi_version(void);

/* Returns the pinned Google Filament release version this bridge was built
   against (e.g. "v1.78.0"). Never returns NULL. */
FILAMENTSHARP_API const char* fs_filament_version(void);

/* Returns the pinned upstream tag commit (hex SHA-1) of the Filament release
   this bridge was built against. Never returns NULL. */
FILAMENTSHARP_API const char* fs_filament_commit(void);

/* ------------------------------------------------------------------ */
/* Error model                                                         */
/* ------------------------------------------------------------------ */

/* Returns a NUL-terminated UTF-8 description of the most recent error on the
   calling thread, or "" when the last bridge call succeeded. Never returns
   NULL. The string is owned by the bridge and remains valid until the next
   bridge call on the same thread. */
FILAMENTSHARP_API const char* fs_get_last_error(void);

/* Clears the calling thread's error state. */
FILAMENTSHARP_API void fs_clear_last_error(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILAMENTSHARP_ABI_H */
