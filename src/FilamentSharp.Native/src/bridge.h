// Bridge-internal helpers shared by all fs_* implementation files.
//
// The macros implement the mandated error model:
//   * every entry point starts by clearing the thread-local error state;
//   * the body runs inside try/catch so no C++ exception can ever cross the
//     C ABI boundary;
//   * `return X;` inside FS_TRY maps exceptions onto `return X` with the
//     error text set (typically FS_ERROR or nullptr).
#ifndef FILAMENTSHARP_BRIDGE_COMMON_H
#define FILAMENTSHARP_BRIDGE_COMMON_H

#include "error.h"
#include "exports.h"

#include <exception>

// Opens an fs_* function body. Use together with FS_END / FS_END_VALUE.
// The expansion is a plain try-block: no trailing semicolon is needed after
// the matching FS_END* macro, and `return X;` inside the body maps onto
// `return X` with the error text set on failure.
#define FS_TRY \
    ::filamentsharp::clearLastError(); \
    try {

// Closes a void fs_* function.
#define FS_END \
    } catch (const std::exception& e_) { \
        ::filamentsharp::setLastError(e_.what()); \
    } catch (...) { \
        ::filamentsharp::setLastError("unknown native error"); \
    }

// Closes an fs_* function that returns `value` on failure.
#define FS_END_VALUE(value) \
    } catch (const std::exception& e_) { \
        ::filamentsharp::setLastError(e_.what()); \
        return (value); \
    } catch (...) { \
        ::filamentsharp::setLastError("unknown native error"); \
        return (value); \
    }

#endif // FILAMENTSHARP_BRIDGE_COMMON_H
