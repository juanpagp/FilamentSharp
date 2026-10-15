#include "error.h"

#include <cstring>
#include <string>

namespace filamentsharp {

namespace {

constexpr std::size_t kErrorCapacity = 512; // 511 chars + NUL

thread_local char g_lastError[kErrorCapacity] = { 0 };

} // namespace

void setLastError(const char* msg, std::size_t length) {
    if (msg == nullptr || length == 0) {
        g_lastError[0] = '\0';
        return;
    }
    const std::size_t n = length < kErrorCapacity - 1 ? length : kErrorCapacity - 1;
    std::memcpy(g_lastError, msg, n);
    g_lastError[n] = '\0';
}

void setLastError(const char* msg) {
    if (msg == nullptr) {
        g_lastError[0] = '\0';
        return;
    }
    setLastError(msg, std::strlen(msg));
}

const char* lastError() {
    return g_lastError;
}

void clearLastError() {
    g_lastError[0] = '\0';
}

} // namespace filamentsharp
