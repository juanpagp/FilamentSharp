// Rendering infrastructure contract tests: renderer, headless swap chain,
// view, scene, camera, entities, frame loop, and synchronous pixel readback.
//
// Runs the full lifecycle twice: once on NOOP (always available; readback is
// structurally verified but contains no image) and once on VULKAN when an
// ICD is available (self-skipping otherwise), where the clear color is
// actually verified in the readback pixels.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "filamentsharp/abi.h"

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

static float srgbEncode(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

static void runScenario(fs_backend backend, const char* name, bool verifyPixels) {
    std::printf("test_rendering: scenario %s\n", name);
    fs_clear_last_error();

    fs_engine_t* engine = fs_engine_create(backend);
    CHECK(engine != nullptr);
    if (engine == nullptr) {
        std::printf("test_rendering:   SKIP (%s): %s\n", name, fs_get_last_error());
        return;
    }

    const uint32_t W = 128, H = 128;
    fs_swap_chain_t* sc = fs_swap_chain_create_headless(engine, W, H, 0);
    fs_renderer_t* renderer = fs_renderer_create(engine);
    fs_scene_t* scene = fs_scene_create(engine);
    fs_view_t* view = fs_view_create(engine);
    fs_entity_id cameraEntity = fs_entity_create();
    fs_camera_t* camera = fs_camera_create(engine, cameraEntity);
    CHECK(sc != nullptr && renderer != nullptr && scene != nullptr && view != nullptr
          && camera != nullptr);

    // Argument validation.
    CHECK(fs_swap_chain_create_headless(engine, 0, H, 0) == nullptr);
    CHECK(fs_swap_chain_create_native(engine, nullptr, 0) == nullptr);
    CHECK(std::strstr(fs_get_last_error(), "null") != nullptr);
    CHECK(fs_swap_chain_create_native(nullptr, (void*)0x1, 0) == nullptr);
    CHECK(fs_camera_create(engine, 0) == nullptr);
    CHECK(fs_camera_set_projection(engine, camera, -10.0, 1.0, 0.1, 100.0, 0)
          == FS_ERROR_INVALID_ARG);
    CHECK(fs_camera_set_projection(engine, camera, 45.0, 1.0, 10.0, 1.0, 0)
          == FS_ERROR_INVALID_ARG);
    CHECK(fs_renderer_begin_frame(engine, renderer, nullptr, 0, nullptr)
          == FS_ERROR_INVALID_ARG);

    // View wiring.
    CHECK(fs_view_set_scene(engine, view, scene) == FS_OK);
    CHECK(fs_view_set_scene(engine, view, nullptr) == FS_OK); // detach is legal
    CHECK(fs_view_set_scene(engine, view, scene) == FS_OK);
    CHECK(fs_view_set_camera(engine, view, camera) == FS_OK);
    CHECK(fs_view_set_viewport(engine, view, 0, 0, W, H) == FS_OK);

    // Camera setup.
    CHECK(fs_camera_set_projection(engine, camera, 45.0, 1.0, 0.1, 100.0,
                                   FS_FOV_VERTICAL) == FS_OK);
    const double eye[3] = { 0, 0, 5 }, center[3] = { 0, 0, 0 }, up[3] = { 0, 1, 0 };
    CHECK(fs_camera_look_at(engine, camera, eye, center, up) == FS_OK);

    float proj[16];
    CHECK(fs_camera_get_projection_matrix(engine, camera, proj) == FS_OK);
    // Perspective projection sanity: [3][2] (column-major element 11) is -1
    // for a right-handed GL-style projection.
    CHECK(proj[11] == -1.0f);
    CHECK(proj[15] == 0.0f);

    // Clear color: linear (0.1, 0.2, 0.3).
    CHECK(fs_renderer_set_clear_options(engine, renderer, 0.1f, 0.2f, 0.3f, 1.0f, 1, 1)
          == FS_OK);

    // Frame loop with synchronous readback.
    std::vector<uint8_t> pixels(size_t(W) * H * 4, 0);
    uintptr_t written = 0;
    int attempts = 0;
    for (attempts = 0; attempts < 6; attempts++) {
        int32_t began = 0;
        CHECK(fs_renderer_begin_frame(engine, renderer, sc, 0, &began) == FS_OK);
        CHECK(fs_renderer_render(engine, renderer, view) == FS_OK);
        CHECK(fs_renderer_read_pixels(engine, renderer, 0, 0, W, H) == FS_OK);
        CHECK(fs_renderer_end_frame(engine, renderer) == FS_OK);

        fs_status status = fs_renderer_read_pixels_complete(
                engine, renderer, pixels.data(), pixels.size(), &written);
        if (status == FS_OK) {
            break;
        }
        CHECK(std::strstr(fs_get_last_error(), "not ready") != nullptr);
    }
    CHECK(written == size_t(W) * H * 4);

    if (verifyPixels) {
        const size_t c = (size_t(H / 2) * W + W / 2) * 4;
        const float r = pixels[c + 0] / 255.0f, g = pixels[c + 1] / 255.0f,
                    b = pixels[c + 2] / 255.0f, a = pixels[c + 3] / 255.0f;
        std::printf("test_rendering:   center pixel %.3f %.3f %.3f %.3f\n", r, g, b, a);
        const float eps = 0.12f;
        CHECK(std::abs(r - srgbEncode(0.1f)) < eps);
        CHECK(std::abs(g - srgbEncode(0.2f)) < eps);
        CHECK(std::abs(b - srgbEncode(0.3f)) < eps);
        CHECK(std::abs(a - 1.0f) < eps);
    }

    // Native-window swap chain lifecycle (NOOP only: the platform layer
    // ignores the handle there. On the Vulkan path a fake XID would be
    // dereferenced by XCB; real windowed rendering is exercised by the
    // WindowedRendering sample - see docs/windowed-rendering.md).
    if (backend == FS_BACKEND_NOOP) {
        fs_swap_chain_t* native = fs_swap_chain_create_native(engine, (void*)0x1234, 0);
        CHECK(native != nullptr);
        if (native != nullptr) {
            int32_t nativeBegan = 0;
            CHECK(fs_renderer_begin_frame(engine, renderer, native, 0, &nativeBegan) == FS_OK);
            CHECK(nativeBegan == 1);
            CHECK(fs_renderer_render(engine, renderer, view) == FS_OK);
            CHECK(fs_renderer_end_frame(engine, renderer) == FS_OK);
            CHECK(fs_swap_chain_destroy(engine, native) == FS_OK);
            CHECK(fs_swap_chain_destroy(engine, native) == FS_ERROR_INVALID_ARG);
        }
        std::printf("test_rendering:   native swap chain lifecycle OK (noop)\n");
    }

    // Invalidated-after-engine-destroy semantics for children.
    fs_engine_t* dyingEngine = engine;
    fs_renderer_t* keptRenderer = renderer;

    // Teardown in documented order (children first, engine last).
    CHECK(fs_camera_destroy(engine, camera) == FS_OK);
    CHECK(fs_camera_destroy(engine, camera) == FS_ERROR_INVALID_ARG); // unknown now
    CHECK(fs_view_destroy(engine, view) == FS_OK);
    CHECK(fs_scene_destroy(engine, scene) == FS_OK);
    CHECK(fs_renderer_destroy(engine, renderer) == FS_OK);
    CHECK(fs_renderer_destroy(engine, renderer) == FS_ERROR_INVALID_ARG); // double destroy
    CHECK(fs_swap_chain_destroy(engine, sc) == FS_OK);
    fs_entity_destroy(cameraEntity);

    // Child operations on a dead engine. (out_began must be non-null: the
    // argument check runs before the engine-liveness check.)
    fs_engine_destroy(dyingEngine);
    CHECK(fs_renderer_destroy(dyingEngine, keptRenderer) == FS_OK); // documented no-op
    int32_t beganDummy = 0;
    CHECK(fs_renderer_begin_frame(dyingEngine, keptRenderer, nullptr, 0, &beganDummy)
          == FS_ERROR_INVALIDATED);

    std::printf("test_rendering:   scenario %s OK\n", name);
}

int main() {
    // Layout checks for shared enums/flags.
    static_assert(FS_SWAP_CHAIN_CONFIG_READABLE == 0x2, "swap chain flags are ABI");
    static_assert(FS_FOV_VERTICAL == 0 && FS_FOV_HORIZONTAL == 1, "fov direction is ABI");

    runScenario(FS_BACKEND_NOOP, "noop", /*verifyPixels=*/false);
    runScenario(FS_BACKEND_VULKAN, "vulkan", /*verifyPixels=*/true);

    if (g_failures > 0) {
        std::fprintf(stderr, "test_rendering: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_rendering: OK\n");
    return 0;
}
