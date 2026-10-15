// Engine lifecycle entry points.
#include "bridge.h"
#include "registry.h"

#include <filament/Engine.h>
#include <filament/SwapChain.h> // for SwapChain flags in later revisions
#include <backend/DriverEnums.h>

#include <new>
#include <vector>

using filamentsharp::Registry;

namespace {

bool backendFromAbi(fs_backend backend, filament::backend::Backend* out) {
    using B = filament::backend::Backend;
    switch (backend) {
        case FS_BACKEND_DEFAULT: *out = B::DEFAULT; return true;
        case FS_BACKEND_OPENGL:  *out = B::OPENGL;  return true;
        case FS_BACKEND_VULKAN:  *out = B::VULKAN;  return true;
        case FS_BACKEND_METAL:   *out = B::METAL;   return true;
        case FS_BACKEND_WEBGPU:  *out = B::WEBGPU;  return true;
        case FS_BACKEND_NOOP:    *out = B::NOOP;    return true;
    }
    return false;
}

bool backendToAbi(filament::backend::Backend backend, fs_backend* out) {
    using B = filament::backend::Backend;
    switch (backend) {
        case B::DEFAULT: *out = FS_BACKEND_DEFAULT; return true;
        case B::OPENGL:  *out = FS_BACKEND_OPENGL;  return true;
        case B::VULKAN:  *out = FS_BACKEND_VULKAN;  return true;
        case B::METAL:   *out = FS_BACKEND_METAL;   return true;
        case B::WEBGPU:  *out = FS_BACKEND_WEBGPU;  return true;
        case B::NOOP:    *out = FS_BACKEND_NOOP;    return true;
    }
    return false;
}

} // namespace

extern "C" {

FILAMENTSHARP_API fs_engine_t* fs_engine_create(fs_backend backend) {
    FS_TRY
        filament::backend::Backend filamentBackend;
        if (!backendFromAbi(backend, &filamentBackend)) {
            ::filamentsharp::setLastError("invalid fs_backend value");
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(Registry::mutex());

        filament::Engine* engine = filament::Engine::Builder()
                .backend(filamentBackend)
                .build();
        if (engine == nullptr) {
            ::filamentsharp::setLastError(
                    "Filament engine creation failed (backend initialization error)");
            return nullptr;
        }

        auto* handle = new fs_engine(engine);
        Registry::instance().addEngineLocked(handle);
        return handle;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API void fs_engine_destroy(fs_engine_t* engine) {
    FS_TRY
        if (engine == nullptr) {
            return;
        }

        fs_engine* handle = nullptr;
        filament::Engine* doomed = nullptr;
        std::vector<filamentsharp::Registry::ChildRef> children;
        {
            std::lock_guard<std::mutex> lock(Registry::mutex());
            handle = Registry::instance().engineLocked(engine);
            if (handle == nullptr || !handle->alive.exchange(false)) {
                // Unknown handle or already destroyed: no-op.
                return;
            }
            // Children become invalid the moment `alive` flips. Their
            // registrations are kept until Filament teardown completes
            // (below), because wrappers may own memory Filament is still
            // using (e.g. a pending readback buffer). The children are
            // collected in CREATION order for the LIFO pass below.
            Registry::instance().collectChildrenLocked(handle, children);
            doomed = handle->engine;
            handle->engine = nullptr;
            // The wrapper itself is retained (documented in abi.h).
        }

        // Destroy the Filament objects owned by still-live children in
        // REVERSE creation order (LIFO), before the engine itself. This is
        // required, not an optimization: FMaterial's destructor terminates
        // the process when material instances outlive their material, and
        // Filament's own engine shutdown hits that destructor for leftover
        // materials (verified against the pinned v1.78.0 release). LIFO
        // guarantees instances die before materials because an instance can
        // only be created after its material. Each destroy is isolated: a
        // failure is recorded and teardown continues, so Engine::destroy
        // below always runs.
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (it->destroyObject == nullptr) {
                continue;
            }
            try {
                it->destroyObject(doomed, it->child);
            } catch (const std::exception& e) {
                ::filamentsharp::setLastError(e.what());
            } catch (...) {
                ::filamentsharp::setLastError("unknown error destroying child object");
            }
        }

        // Destroy the engine outside the registry mutex: this is the
        // expensive, thread-safe teardown. Nothing can reach `doomed` through
        // the bridge anymore (alive == false guards every child path). When
        // Engine::destroy returns, all Filament work (including pending
        // readback callbacks) has completed, so wrapper-owned buffers are no
        // longer referenced.
        if (doomed != nullptr) {
            filament::Engine::destroy(&doomed);
        }

        // Now the child wrappers themselves can be freed safely. A racing
        // child destroy between the phases above observed a dead engine and
        // returned as a no-op without touching the wrapper.
        {
            std::lock_guard<std::mutex> lock(Registry::mutex());
            for (const auto& ref : children) {
                Registry::instance().disposeChildLocked(ref.child);
            }
        }
    FS_END
}

FILAMENTSHARP_API fs_backend fs_engine_get_backend(fs_engine_t* engine) {
    FS_TRY
        if (engine == nullptr) {
            ::filamentsharp::setLastError("engine handle is null");
            return FS_BACKEND_DEFAULT;
        }
        std::lock_guard<std::mutex> lock(Registry::mutex());
        fs_engine* handle = Registry::instance().engineLocked(engine);
        if (handle == nullptr || !handle->alive.load() || handle->engine == nullptr) {
            ::filamentsharp::setLastError("engine handle is invalid or destroyed");
            return FS_BACKEND_DEFAULT;
        }
        fs_backend result;
        if (!backendToAbi(handle->engine->getBackend(), &result)) {
            ::filamentsharp::setLastError("unknown backend reported by Filament");
            return FS_BACKEND_DEFAULT;
        }
        return result;
    FS_END_VALUE(FS_BACKEND_DEFAULT)
}

} // extern "C"
