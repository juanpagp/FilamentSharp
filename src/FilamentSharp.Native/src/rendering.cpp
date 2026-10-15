// Rendering infrastructure entry points: renderer, headless swap chain,
// view, scene, camera, entities, and the frame loop with synchronous pixel
// readback support.
#include "bridge.h"
#include "child.h"
#include "types.h"
#include "registry.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <backend/DriverEnums.h>
#include <backend/PixelBufferDescriptor.h>
#include <math/mat4.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>

#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

void readbackCallback(void*, size_t, void* user) {
    *static_cast<bool*>(user) = true;
}

// Validates an engine handle for a create operation; returns the live
// fs_engine or nullptr with the error set. The registry lock is returned
// held (caller keeps it for the Filament call).
fs_engine* acquireEngine(std::unique_lock<std::mutex>& lock, fs_engine_t* engineHandle) {
    lock = std::unique_lock<std::mutex>(filamentsharp::Registry::mutex());
    if (engineHandle == nullptr) {
        filamentsharp::setLastError("engine handle is null");
        return nullptr;
    }
    fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
    if (engine == nullptr) {
        filamentsharp::setLastError("unknown engine handle");
        return nullptr;
    }
    if (!engine->alive.load()) {
        filamentsharp::setLastError("owning engine has been destroyed");
        return nullptr;
    }
    return engine;
}

} // namespace

extern "C" {

// ---------------------------------------------------------------- entities

FILAMENTSHARP_API fs_entity_id fs_entity_create(void) {
    FS_TRY
        utils::Entity entity = utils::EntityManager::get().create();
        return entity.getId();
    FS_END_VALUE(0)
}

FILAMENTSHARP_API void fs_entity_destroy(fs_entity_id entity) {
    FS_TRY
        if (entity != 0) {
            utils::EntityManager::get().destroy(utils::Entity::import(int32_t(entity)));
        }
    FS_END
}

// ---------------------------------------------------------------- renderer

FILAMENTSHARP_API fs_renderer_t* fs_renderer_create(fs_engine_t* engineHandle) {
    FS_TRY
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::Renderer* renderer = engine->engine->createRenderer();
        if (renderer == nullptr) {
            filamentsharp::setLastError("filament::Engine::createRenderer returned null");
            return nullptr;
        }
        auto* wrapper = new fs_renderer(renderer);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_renderer>,
                &filamentsharp::destroyChildObject<fs_renderer>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_renderer_destroy(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle) {
    FS_TRY
        if (engineHandle == nullptr || rendererHandle == nullptr) {
            filamentsharp::setLastError("engine or renderer handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK; // disposed with the engine; documented no-op
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(rendererHandle) != engine) {
            filamentsharp::setLastError("unknown renderer handle or owned by another engine");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_renderer*>(rendererHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(rendererHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_begin_frame(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle, fs_swap_chain_t* swapChainHandle,
        uint64_t frameTimeNs, int32_t* outBegan) {
    FS_TRY
        if (outBegan == nullptr) {
            filamentsharp::setLastError("out_began is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        (void)engine; (void)lock;
        fs_swap_chain* swapChain = nullptr;
        if (swapChainHandle != nullptr) {
            FS_CHECK_CHILD_HELD(fs_swap_chain, swapChain, swapChainHandle)
        } else {
            filamentsharp::setLastError("swap chain handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outBegan = renderer->renderer->beginFrame(swapChain->swapChain, frameTimeNs) ? 1 : 0;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_render(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle, fs_view_t* viewHandle) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        (void)engine; (void)lock;
        fs_view* view = nullptr;
        FS_CHECK_CHILD_HELD(fs_view, view, viewHandle)
        renderer->renderer->render(view->view);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_end_frame(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        (void)engine; (void)lock;
        renderer->renderer->endFrame();
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_set_clear_options(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle, float r, float g, float b, float a,
        int32_t clear, int32_t discard) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        (void)engine; (void)lock;
        filament::Renderer::ClearOptions options{};
        options.clearColor = filament::math::float4{r, g, b, a};
        options.clear = clear != 0;
        options.discard = discard != 0;
        renderer->renderer->setClearOptions(options);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_read_pixels(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height) {
    FS_TRY
        if (width == 0 || height == 0) {
            filamentsharp::setLastError("readback size must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        (void)engine; (void)lock;
        if (renderer->pendingPixels != nullptr) {
            filamentsharp::setLastError("a readback is already pending for this renderer");
            return FS_ERROR;
        }
        const size_t size = size_t(width) * size_t(height) * 4;
        uint8_t* buffer = static_cast<uint8_t*>(std::malloc(size));
        if (buffer == nullptr) {
            filamentsharp::setLastError("out of memory allocating readback buffer");
            return FS_ERROR;
        }
        renderer->pendingPixels = buffer;
        renderer->pendingSize = size;
        renderer->pendingDone = false;

        filament::backend::PixelBufferDescriptor descriptor(
                buffer, size, filament::backend::PixelDataFormat::RGBA,
                filament::backend::PixelDataType::UBYTE, &readbackCallback,
                &renderer->pendingDone);
        renderer->renderer->readPixels(x, y, width, height, std::move(descriptor));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_renderer_read_pixels_complete(fs_engine_t* engineHandle,
        fs_renderer_t* rendererHandle, uint8_t* outRgba, uintptr_t outCapacity,
        uintptr_t* outBytesWritten) {
    FS_TRY
        if (outBytesWritten == nullptr) {
            filamentsharp::setLastError("out_bytes_written is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outBytesWritten = 0;
        if (outRgba == nullptr) {
            filamentsharp::setLastError("out_rgba is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_renderer, renderer, rendererHandle)
        if (renderer->pendingPixels == nullptr) {
            filamentsharp::setLastError("no pending readback for this renderer");
            return FS_ERROR;
        }
        if (outCapacity < renderer->pendingSize) {
            filamentsharp::setLastError("output buffer too small for the pending readback");
            return FS_ERROR_INVALID_ARG;
        }
        // Wait for the engine to complete the readback. flushAndWait()
        // processes all pending work; the callback fires on this thread.
        engine->engine->flushAndWait();
        if (!renderer->pendingDone) {
            filamentsharp::setLastError("readback not ready yet; retry after another frame");
            return FS_ERROR_NOT_READY;
        }
        std::memcpy(outRgba, renderer->pendingPixels, renderer->pendingSize);
        *outBytesWritten = renderer->pendingSize;
        std::free(renderer->pendingPixels);
        renderer->pendingPixels = nullptr;
        renderer->pendingSize = 0;
        renderer->pendingDone = false;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// -------------------------------------------------------------- swap chain

FILAMENTSHARP_API fs_swap_chain_t* fs_swap_chain_create_headless(
        fs_engine_t* engineHandle, uint32_t width, uint32_t height,
        fs_swap_chain_flags_t flags) {
    FS_TRY
        if (width == 0 || height == 0) {
            filamentsharp::setLastError("swap chain size must be non-zero");
            return nullptr;
        }
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::SwapChain* swapChain = engine->engine->createSwapChain(width, height, flags);
        if (swapChain == nullptr) {
            filamentsharp::setLastError("filament::Engine::createSwapChain returned null");
            return nullptr;
        }
        auto* wrapper = new fs_swap_chain(swapChain);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_swap_chain>,
                &filamentsharp::destroyChildObject<fs_swap_chain>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_swap_chain_destroy(fs_engine_t* engineHandle,
        fs_swap_chain_t* swapChainHandle) {
    FS_TRY
        if (engineHandle == nullptr || swapChainHandle == nullptr) {
            filamentsharp::setLastError("engine or swap chain handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK;
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(swapChainHandle) != engine) {
            filamentsharp::setLastError("unknown swap chain handle or owned by another engine");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_swap_chain*>(swapChainHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(swapChainHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// -------------------------------------------------------------------- view

FILAMENTSHARP_API fs_view_t* fs_view_create(fs_engine_t* engineHandle) {
    FS_TRY
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::View* view = engine->engine->createView();
        if (view == nullptr) {
            filamentsharp::setLastError("filament::Engine::createView returned null");
            return nullptr;
        }
        auto* wrapper = new fs_view(view);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_view>,
                &filamentsharp::destroyChildObject<fs_view>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_view_destroy(fs_engine_t* engineHandle, fs_view_t* viewHolder) {
    FS_TRY
        if (engineHandle == nullptr || viewHolder == nullptr) {
            filamentsharp::setLastError("engine or view handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK;
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(viewHolder) != engine) {
            filamentsharp::setLastError("unknown view handle or owned by another engine");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_view*>(viewHolder);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(viewHolder);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_view_set_scene(fs_engine_t* engineHandle, fs_view_t* viewHolder,
        fs_scene_t* sceneHandle) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_view, view, viewHolder)
        (void)engine; (void)lock;
        // A null scene is legal in Filament (detaches the scene).
        filament::Scene* scene = nullptr;
        if (sceneHandle != nullptr) {
            fs_scene* sceneWrapper = nullptr;
            FS_CHECK_CHILD_HELD(fs_scene, sceneWrapper, sceneHandle)
            scene = sceneWrapper->scene;
        }
        view->view->setScene(scene);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_view_set_camera(fs_engine_t* engineHandle, fs_view_t* viewHolder,
        fs_camera_t* cameraHandle) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_view, view, viewHolder)
        (void)engine; (void)lock;
        fs_camera* camera = nullptr;
        FS_CHECK_CHILD_HELD(fs_camera, camera, cameraHandle)
        view->view->setCamera(camera->camera);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_view_set_viewport(fs_engine_t* engineHandle, fs_view_t* viewHolder,
        int32_t left, int32_t bottom, uint32_t width, uint32_t height) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_view, view, viewHolder)
        (void)engine; (void)lock;
        view->view->setViewport(filament::Viewport(left, bottom, int32_t(width), int32_t(height)));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ------------------------------------------------------------------- scene

FILAMENTSHARP_API fs_scene_t* fs_scene_create(fs_engine_t* engineHandle) {
    FS_TRY
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::Scene* scene = engine->engine->createScene();
        if (scene == nullptr) {
            filamentsharp::setLastError("filament::Engine::createScene returned null");
            return nullptr;
        }
        auto* wrapper = new fs_scene(scene);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_scene>,
                &filamentsharp::destroyChildObject<fs_scene>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_scene_destroy(fs_engine_t* engineHandle, fs_scene_t* sceneHandle) {
    FS_TRY
        if (engineHandle == nullptr || sceneHandle == nullptr) {
            filamentsharp::setLastError("engine or scene handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK;
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(sceneHandle) != engine) {
            filamentsharp::setLastError("unknown scene handle or owned by another engine");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_scene*>(sceneHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(sceneHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ------------------------------------------------------------------ camera

FILAMENTSHARP_API fs_camera_t* fs_camera_create(fs_engine_t* engineHandle,
        fs_entity_id entity) {
    FS_TRY
        if (entity == 0) {
            filamentsharp::setLastError("camera entity must be non-zero; create one with fs_entity_create");
            return nullptr;
        }
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        utils::Entity filamentEntity = utils::Entity::import(int32_t(entity));
        filament::Camera* camera = engine->engine->createCamera(filamentEntity);
        if (camera == nullptr) {
            filamentsharp::setLastError("filament::Engine::createCamera returned null");
            return nullptr;
        }
        auto* wrapper = new fs_camera(camera, filamentEntity);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_camera>,
                &filamentsharp::destroyChildObject<fs_camera>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_camera_destroy(fs_engine_t* engineHandle,
        fs_camera_t* cameraHandle) {
    FS_TRY
        if (engineHandle == nullptr || cameraHandle == nullptr) {
            filamentsharp::setLastError("engine or camera handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK;
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(cameraHandle) != engine) {
            filamentsharp::setLastError("unknown camera handle or owned by another engine");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_camera*>(cameraHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(cameraHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_camera_set_projection(fs_engine_t* engineHandle,
        fs_camera_t* cameraHandle, double fovDegrees, double aspect, double nearPlane,
        double farPlane, int32_t fovDirection) {
    FS_TRY
        if (!(fovDegrees > 0.0) || !(aspect > 0.0) || !(farPlane > nearPlane) || !(nearPlane > 0.0)) {
            filamentsharp::setLastError(
                    "invalid projection: fov>0, aspect>0, far>near>0 required");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_camera, camera, cameraHandle)
        (void)engine; (void)lock;
        const filament::Camera::Fov direction = fovDirection == FS_FOV_HORIZONTAL
                ? filament::Camera::Fov::HORIZONTAL
                : filament::Camera::Fov::VERTICAL;
        camera->camera->setProjection(fovDegrees, aspect, nearPlane, farPlane, direction);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_camera_look_at(fs_engine_t* engineHandle,
        fs_camera_t* cameraHandle, const double* eye, const double* center,
        const double* up) {
    FS_TRY
        if (eye == nullptr || center == nullptr || up == nullptr) {
            filamentsharp::setLastError("eye/center/up must be non-null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_camera, camera, cameraHandle)
        (void)engine; (void)lock;
        camera->camera->lookAt(
                filament::math::double3{eye[0], eye[1], eye[2]},
                filament::math::double3{center[0], center[1], center[2]},
                filament::math::double3{up[0], up[1], up[2]});
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_camera_get_projection_matrix(fs_engine_t* engineHandle,
        fs_camera_t* cameraHandle, float* outMatrix) {
    FS_TRY
        if (outMatrix == nullptr) {
            filamentsharp::setLastError("out_matrix is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_camera, camera, cameraHandle)
        (void)engine; (void)lock;
        // Camera::getProjectionMatrix() returns a double-precision mat4;
        // the ABI exposes single precision (documented in abi.h).
        const filament::math::mat4 matrix = camera->camera->getProjectionMatrix();
        const double* columns = matrix.asArray();
        for (int i = 0; i < 16; i++) {
            outMatrix[i] = static_cast<float>(columns[i]);
        }
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

} // extern "C"
