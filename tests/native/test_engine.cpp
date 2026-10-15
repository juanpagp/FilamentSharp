// Engine lifecycle contract tests against the linked bridge.
#include <cstdio>
#include <cstring>

#include "filamentsharp/abi.h"

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

// Layout checks: fs_backend mirrors the pinned filament::backend::Backend.
static_assert(FS_BACKEND_DEFAULT == 0 && FS_BACKEND_OPENGL == 1 &&
              FS_BACKEND_VULKAN == 2 && FS_BACKEND_METAL == 3 &&
              FS_BACKEND_WEBGPU == 4 && FS_BACKEND_NOOP == 5,
              "fs_backend values are part of the ABI");

int main() {
    // --- null / invalid handle handling --------------------------------
    fs_engine_destroy(nullptr); // must be a safe no-op
    CHECK(std::strcmp(fs_get_last_error(), "") == 0);

    CHECK(fs_engine_get_backend(nullptr) == FS_BACKEND_DEFAULT);
    CHECK(std::strlen(fs_get_last_error()) > 0);

    fs_engine_t* forged = reinterpret_cast<fs_engine_t*>(0x1234);
    fs_engine_destroy(forged); // unknown pointer: safe no-op
    fs_engine_get_backend(forged);
    CHECK(std::strlen(fs_get_last_error()) > 0);

    // --- invalid backend value ------------------------------------------
    fs_clear_last_error();
    CHECK(fs_engine_create(static_cast<fs_backend>(99)) == nullptr);
    CHECK(std::strlen(fs_get_last_error()) > 0);

    // --- NOOP engine lifecycle -------------------------------------------
    fs_engine_t* engine = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(engine != nullptr);
    if (engine == nullptr) {
        std::fprintf(stderr, "test_engine: cannot continue without engine\n");
        return 1;
    }
    CHECK(fs_engine_get_backend(engine) == FS_BACKEND_NOOP);

    // Destroy, then verify idempotence and invalidation.
    fs_engine_destroy(engine);
    fs_engine_destroy(engine); // double destroy: no-op, no crash

    CHECK(fs_engine_get_backend(engine) == FS_BACKEND_DEFAULT);
    CHECK(std::strlen(fs_get_last_error()) > 0);

    // --- second engine after the first one died ---------------------------
    fs_engine_t* engine2 = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(engine2 != nullptr);
    CHECK(engine2 != engine);
    CHECK(fs_engine_get_backend(engine2) == FS_BACKEND_NOOP);
    fs_engine_destroy(engine2);

    // --- Vulkan engine (skipped when no ICD is available) -----------------
    fs_clear_last_error();
    fs_engine_t* vk = fs_engine_create(FS_BACKEND_VULKAN);
    if (vk == nullptr) {
        std::printf("test_engine: SKIP vulkan (no usable ICD: %s)\n", fs_get_last_error());
    } else {
        CHECK(fs_engine_get_backend(vk) == FS_BACKEND_VULKAN);
        fs_engine_destroy(vk);
        std::printf("test_engine: vulkan engine lifecycle OK\n");
    }

    if (g_failures > 0) {
        std::fprintf(stderr, "test_engine: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_engine: OK\n");
    return 0;
}
