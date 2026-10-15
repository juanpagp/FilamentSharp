// Engine/child ownership registry - the bridge's lifetime-management core.
//
// Invariants (all enforced under a single internal mutex):
//   * Every engine wrapper ever created stays registered for the process
//     lifetime (wrappers are tiny; this makes late child releases safe
//     without use-after-free on the wrapper itself).
//   * An engine wrapper is "alive" from fs_engine_create until
//     fs_engine_destroy. Destruction flips the flag BEFORE the underlying
//     Filament engine is destroyed, so any racing child operation either
//     observes a dead engine (and returns FS_ERROR_INVALIDATED) or holds the
//     registry mutex, which fs_engine_destroy also acquires before flipping.
//   * Every child wrapper is registered with its owning engine at creation
//     together with a type-correct dispose function. Child wrappers are
//     deleted either by their own destroy entry point or, if still alive
//     when the engine dies, by the engine destroy path (Filament frees the
//     underlying objects; the registry frees the wrapper memory).
//   * Filament-level operations run while holding the registry mutex, which
//     serializes bridge calls. Filament itself requires external
//     synchronization anyway (see abi.h threading notes); the cost is an
//     uncontended lock per call in the single-threaded case.
#ifndef FILAMENTSHARP_REGISTRY_H
#define FILAMENTSHARP_REGISTRY_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace filament {
class Engine;
}

// Definition of the opaque fs_engine_t declared in filamentsharp/abi.h.
// Deliberately at global scope: this IS the C type the ABI talks about.
struct fs_engine {
    filament::Engine* engine = nullptr;
    std::atomic<bool> alive{ false };

    explicit fs_engine(filament::Engine* e) : engine(e) { alive.store(true); }
};

namespace filamentsharp {

class Registry {
public:
    // Per-child registration: owning engine plus a type-correct deleter for
    // the wrapper (wrappers may own heap memory, e.g. pending readback
    // buffers, so they must be deleted as their real type).
    struct ChildEntry {
        fs_engine* owner;
        void (*dispose)(void*);
    };

    static Registry& instance() {
        static Registry registry;
        return registry;
    }

    static std::mutex& mutex() { return instance().m_mutex; }

    // Caller must hold mutex().
    fs_engine* engineLocked(void* engineHandle) const {
        const auto it = m_engines.find(engineHandle);
        return it == m_engines.end() ? nullptr : it->second;
    }

    // Caller must hold mutex(). Registers a freshly created engine wrapper.
    void addEngineLocked(fs_engine* handle) { m_engines[handle] = handle; }

    // Caller must hold mutex(). Registers a child wrapper with its owner.
    void addChildLocked(fs_engine* owner, void* child, void (*dispose)(void*)) {
        m_children[child] = ChildEntry{ owner, dispose };
    }

    // Caller must hold mutex(). Looks up a child's owner; nullptr if the
    // child pointer was never registered (or was already destroyed).
    fs_engine* ownerOfLocked(void* child) const {
        const auto it = m_children.find(child);
        return it == m_children.end() ? nullptr : it->second.owner;
    }

    // Caller must hold mutex(). Removes a child registration (called from
    // its destroy path, including the invalidated-engine no-op path).
    void removeChildLocked(void* child) { m_children.erase(child); }

    // Caller must hold mutex(). Collects every child wrapper belonging to
    // `owner` without disposing anything (used by the engine destroy path:
    // wrappers are disposed only AFTER Engine::destroy has completed, since
    // wrappers may own memory Filament is still writing to, e.g. pending
    // readback buffers).
    void collectChildrenLocked(fs_engine* owner, std::vector<void*>& out) const {
        for (const auto& [child, entry] : m_children) {
            if (entry.owner == owner) {
                out.push_back(child);
            }
        }
    }

    // Caller must hold mutex(). Disposes and unregisters a single child
    // wrapper (engine destroy path, after Filament teardown).
    void disposeChildLocked(void* child) {
        const auto it = m_children.find(child);
        if (it != m_children.end()) {
            it->second.dispose(it->first);
            m_children.erase(it);
        }
    }

private:
    Registry() = default;

    std::mutex m_mutex;
    // Engine wrapper pointer -> wrapper. Wrappers are never removed.
    std::unordered_map<void*, fs_engine*> m_engines;
    // Child wrapper pointer -> registration.
    std::unordered_map<void*, ChildEntry> m_children;
};

// Type-correct dispose function for child wrappers.
template <typename T>
void disposeChild(void* child) {
    delete static_cast<T*>(child);
}

} // namespace filamentsharp

#endif // FILAMENTSHARP_REGISTRY_H
