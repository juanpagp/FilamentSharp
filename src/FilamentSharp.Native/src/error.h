// Thread-local error state for the FilamentSharp bridge.
//
// Every fs_* entry point clears the state on entry (success leaves it as "")
// and fills it with a UTF-8 message on failure. The buffer is thread-local so
// the error model is safe under multi-threaded callers without locking.
#ifndef FILAMENTSHARP_ERROR_H
#define FILAMENTSHARP_ERROR_H

#include <cstddef>

namespace filamentsharp {

// Sets the calling thread's last error to `msg` (copied; truncated at 511
// bytes + NUL to keep the state fixed-size and allocation-free).
void setLastError(const char* msg);

// Overload accepting a pointer+length (no dependency on std::string at the
// ABI edge).
void setLastError(const char* msg, std::size_t length);

// Returns the calling thread's last error; never null. Points at a
// thread-local buffer that stays valid until the next bridge call on the same
// thread.
const char* lastError();

// Resets the calling thread's error state to "".
void clearLastError();

} // namespace filamentsharp

#endif // FILAMENTSHARP_ERROR_H
