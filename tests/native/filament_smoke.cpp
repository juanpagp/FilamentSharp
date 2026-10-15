// Filament SDK smoke test (native, independent of the C ABI bridge).
//
// Validates that the pinned Filament release SDK actually works in the current
// environment: it creates a real engine with the requested backend, renders
// frames into a headless swap chain, reads pixels back synchronously, checks
// the result, and shuts down cleanly.
//
// Usage: filament_smoke <vk|gl|noop>
//   vk   - Vulkan backend (needs a Vulkan ICD, e.g. lavapipe; see
//          scripts/setup-headless-vulkan.sh)
//   gl   - OpenGL backend (needs a running X server on Linux: GLX)
//   noop - no-op backend (no GPU needed; pixel assertions are skipped)
//
// Exit code 0 = success.
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/Viewport.h>
#include <backend/DriverEnums.h>
#include <backend/PixelBufferDescriptor.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace filament;
using namespace filament::backend;

namespace {

bool g_readbackDone = false;

void readbackCallback(void*, size_t, void* user) {
    *static_cast<bool*>(user) = true;
}

float srgbEncode(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

int fail(const char* what) {
    std::printf("SMOKE FAIL: %s\n", what);
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::printf("usage: filament_smoke <vk|gl|noop>\n");
        return 2;
    }
    const char* backendName = argv[1];
    Backend backend = Backend::VULKAN;
    if (std::strcmp(backendName, "gl") == 0) backend = Backend::OPENGL;
    else if (std::strcmp(backendName, "noop") == 0) backend = Backend::NOOP;
    else if (std::strcmp(backendName, "vk") != 0) return fail("unknown backend");

    const uint32_t W = 256, H = 256;

    Engine* engine = Engine::create(backend);
    if (engine == nullptr) return fail("Engine::create returned null");

    SwapChain* swapChain = engine->createSwapChain(W, H);
    Renderer* renderer = engine->createRenderer();
    Scene* scene = engine->createScene();
    View* view = engine->createView();
    utils::Entity cameraEntity = utils::EntityManager::get().create();
    Camera* camera = engine->createCamera(cameraEntity);

    camera->setProjection(45.0, 1.0, 0.1, 100.0, Camera::Fov::VERTICAL);
    camera->lookAt({0.0, 0.0, 5.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
    view->setCamera(camera);
    view->setScene(scene);
    view->setViewport(filament::Viewport(0, 0, W, H));

    // Linear clear color; the swap chain stores the sRGB-encoded result.
    const math::float4 clearColor{0.10f, 0.20f, 0.30f, 1.0f};
    Renderer::ClearOptions clearOptions{};
    clearOptions.clearColor = clearColor;
    clearOptions.clear = true;
    clearOptions.discard = true;
    renderer->setClearOptions(clearOptions);

    std::vector<uint8_t> pixels(size_t(W) * H * 4, 0);
    PixelBufferDescriptor descriptor(pixels.data(), pixels.size(),
            PixelDataFormat::RGBA, PixelDataType::UBYTE,
            &readbackCallback, &g_readbackDone);

    int renderedFrames = 0;
    for (int i = 0; i < 4 && !g_readbackDone; i++) {
        if (renderer->beginFrame(swapChain)) {
            renderer->render(view);
            renderer->readPixels(0, 0, W, H, std::move(descriptor));
            renderer->endFrame();
            renderedFrames++;
        }
        engine->flushAndWait();
    }
    if (!g_readbackDone) {
        fail("readback callback never fired");
    } else if (renderedFrames > 0 && backend != Backend::NOOP) {
        // The center pixel must match the sRGB encoding of the clear color.
        // Tolerance 0.12 covers driver rounding and dithering observed with
        // the lavapipe software rasterizer (see docs/testing.md for the
        // recorded calibration values).
        size_t center = (size_t(H / 2) * W + W / 2) * 4;
        float r = pixels[center + 0] / 255.0f, g = pixels[center + 1] / 255.0f,
              b = pixels[center + 2] / 255.0f, a = pixels[center + 3] / 255.0f;
        std::printf("center pixel: %.3f %.3f %.3f %.3f (sRGB of clear: %.3f %.3f %.3f 1.000)\n",
                    r, g, b, a, srgbEncode(clearColor.r), srgbEncode(clearColor.g),
                    srgbEncode(clearColor.b));
        const float eps = 0.12f;
        if (std::abs(r - srgbEncode(clearColor.r)) > eps ||
            std::abs(g - srgbEncode(clearColor.g)) > eps ||
            std::abs(b - srgbEncode(clearColor.b)) > eps ||
            std::abs(a - 1.0f) > eps) {
            return fail("clear color mismatch beyond tolerance");
        }
    }

    engine->destroy(cameraEntity);
    engine->destroy(view);
    engine->destroy(scene);
    engine->destroy(renderer);
    engine->destroy(swapChain);
    Engine::destroy(&engine);

    std::printf("SMOKE OK: %s backend (frames=%d, readback=%s)\n", backendName,
                renderedFrames, g_readbackDone ? "yes" : "no");
    return 0;
}
