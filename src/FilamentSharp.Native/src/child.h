// Shared validation for engine-child operations.
//
// Every fs_* function that operates on a child resource takes the owning
// engine handle explicitly (mirroring Filament's own API). Functions that
// touch several children acquire the registry mutex ONCE (via
// FS_ACQUIRE_CHILD for the first child) and validate the remaining children
// with FS_CHECK_CHILD_HELD, which must be called with the lock already held.
// The registry mutex is a plain (non-recursive) mutex; taking it twice on
// the same thread would deadlock.
#ifndef FILAMENTSHARP_CHILD_H
#define FILAMENTSHARP_CHILD_H

#include "error.h"
#include "registry.h"

#include <mutex>

namespace filamentsharp {

enum class ChildCheck {
    Ok,
    Invalid,     // null/unknown/forged handle: caller returns FS_ERROR_INVALID_ARG
    Invalidated, // owning engine destroyed: caller returns FS_ERROR_INVALIDATED
};

// Validates one child against its engine. The caller MUST already hold the
// registry mutex.
template <typename T>
ChildCheck checkChildHeld(fs_engine_t* engineHandle, void* childHandle,
                          fs_engine** outEngine, T** outChild) {
    if (engineHandle == nullptr || childHandle == nullptr) {
        setLastError("engine or child handle is null");
        return ChildCheck::Invalid;
    }
    fs_engine* engine = Registry::instance().engineLocked(engineHandle);
    if (engine == nullptr) {
        setLastError("unknown engine handle");
        return ChildCheck::Invalid;
    }
    if (!engine->alive.load()) {
        setLastError("owning engine has been destroyed");
        return ChildCheck::Invalidated;
    }
    if (Registry::instance().ownerOfLocked(childHandle) != engine) {
        setLastError("unknown child handle or handle owned by another engine");
        return ChildCheck::Invalid;
    }
    *outEngine = engine;
    *outChild = static_cast<T*>(childHandle);
    return ChildCheck::Ok;
}

// Acquires the registry lock and validates the first child of the call.
template <typename T>
ChildCheck acquireChild(std::unique_lock<std::mutex>& lock, fs_engine_t* engineHandle,
                        void* childHandle, fs_engine** outEngine, T** outChild) {
    lock = std::unique_lock<std::mutex>(Registry::mutex());
    return checkChildHeld(engineHandle, childHandle, outEngine, outChild);
}

} // namespace filamentsharp

// Acquires the registry lock and validates the first child for the rest of
// the function body. On failure this expands into a `return` with the
// documented status code. Usable only in functions returning fs_status.
#define FS_ACQUIRE_CHILD(lockVar, engineVar, engineHandle, WrapperType, childVar, childHandle) \
    std::unique_lock<std::mutex> lockVar; \
    fs_engine* engineVar = nullptr; \
    WrapperType* childVar = nullptr; \
    switch (::filamentsharp::acquireChild(lockVar, engineHandle, childHandle, &engineVar, &childVar)) { \
        case ::filamentsharp::ChildCheck::Ok: \
            break; \
        case ::filamentsharp::ChildCheck::Invalidated: \
            return FS_ERROR_INVALIDATED; \
        case ::filamentsharp::ChildCheck::Invalid: \
        default: \
            return FS_ERROR_INVALID_ARG; \
    }

// Validates an additional child; the registry lock must already be held
// (FS_ACQUIRE_CHILD must have run earlier in the same function).
#define FS_CHECK_CHILD_HELD(WrapperType, childVar, childHandle) \
    { \
        WrapperType* heldChild = nullptr; \
        fs_engine* heldEngine = nullptr; \
        switch (::filamentsharp::checkChildHeld(engineHandle, childHandle, &heldEngine, &heldChild)) { \
            case ::filamentsharp::ChildCheck::Ok: \
                childVar = heldChild; \
                break; \
            case ::filamentsharp::ChildCheck::Invalidated: \
                return FS_ERROR_INVALIDATED; \
            case ::filamentsharp::ChildCheck::Invalid: \
            default: \
                return FS_ERROR_INVALID_ARG; \
        } \
    }

#endif // FILAMENTSHARP_CHILD_H
