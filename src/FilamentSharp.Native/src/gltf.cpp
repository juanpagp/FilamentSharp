// glTF asset loading via gltfio (ubershader materials, embedded resources).
#include "bridge.h"
#include "child.h"
#include "registry.h"
#include "types.h"

#include <gltfio/Animator.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/FilamentInstance.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/materials/uberarchive.h>

#include <vector>

namespace {

// Engine acquisition for loader-create (no child handle yet).
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

namespace filamentsharp {

// Tears down a loader and its companion objects (materials cache, texture
// decoder, resource loader). Called from the explicit destroy path and from
// the engine's LIFO teardown; assets must already be gone.
void destroyGltfLoaderObjects(fs_gltf_loader* wrapper) {
    // Teardown order matters and follows gltfio's contract: the
    // ResourceLoader destructor touches its registered texture providers,
    // so it must be deleted BEFORE the decoder (deleting the decoder first
    // is a use-after-free that segfaults). Assets must already be gone.
    delete wrapper->resources;
    if (wrapper->materials != nullptr) {
        wrapper->materials->destroyMaterials();
        delete wrapper->materials;
    }
    delete wrapper->decoder;
    if (wrapper->loader != nullptr) {
        filament::gltfio::AssetLoader::destroy(&wrapper->loader);
    }
}

// Destroys an asset through its loader and unlinks it from the loader's
// asset list.
void destroyGltfAssetObjects(fs_gltf_asset* wrapper) {
    if (wrapper->owner != nullptr && wrapper->owner->loader != nullptr &&
            wrapper->asset != nullptr) {
        wrapper->owner->loader->destroyAsset(wrapper->asset);
    }
    wrapper->asset = nullptr;
    if (wrapper->owner != nullptr) {
        auto& list = wrapper->owner->assets;
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (*it == wrapper) {
                list.erase(it);
                break;
            }
        }
    }
}

} // namespace filamentsharp

extern "C" {

// ------------------------------------------------------------ gltf loader

FILAMENTSHARP_API fs_gltf_loader_t* fs_gltf_loader_create(fs_engine_t* engineHandle) {
    FS_TRY
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }

        // Precompiled ubershader materials: no runtime shader generation
        // (fast loads, Native-AOT friendly). The archive ships with the
        // SDK's uberarchive library.
        filament::gltfio::MaterialProvider* materials =
                filament::gltfio::createUbershaderProvider(engine->engine,
                        UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
        if (materials == nullptr) {
            filamentsharp::setLastError("createUbershaderProvider failed");
            return nullptr;
        }

        auto* loader = filament::gltfio::AssetLoader::create(
                filament::gltfio::AssetConfiguration{ engine->engine, materials });
        if (loader == nullptr) {
            materials->destroyMaterials();
            delete materials;
            filamentsharp::setLastError("gltfio::AssetLoader::create failed");
            return nullptr;
        }

        // PNG/JPEG decoding for embedded textures (stb, no external deps).
        auto* decoder = filament::gltfio::createStbProvider(engine->engine);
        if (decoder == nullptr) {
            filament::gltfio::AssetLoader::destroy(&loader);
            materials->destroyMaterials();
            delete materials;
            filamentsharp::setLastError("createStbProvider failed");
            return nullptr;
        }

        auto* resources = new filament::gltfio::ResourceLoader(
                filament::gltfio::ResourceConfiguration{
                        engine->engine, /*gltfPath=*/nullptr,
                        /*normalizeSkinningWeights=*/false });
        resources->addTextureProvider("image/png", decoder);
        resources->addTextureProvider("image/jpeg", decoder);

        auto* wrapper = new fs_gltf_loader(loader, materials, decoder, resources);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_gltf_loader>,
                &filamentsharp::destroyChildObject<fs_gltf_loader>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_gltf_loader_destroy(fs_engine_t* engineHandle,
        fs_gltf_loader_t* loaderHandle) {
    FS_TRY
        if (engineHandle == nullptr || loaderHandle == nullptr) {
            filamentsharp::setLastError("engine or gltf loader handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            // Engine teardown owns the cleanup (LIFO pass); the wrapper is
            // disposed there. Documented no-op, like every other child.
            return FS_OK;
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(loaderHandle) != engine) {
            filamentsharp::setLastError("unknown gltf loader handle");
            return FS_ERROR_INVALID_ARG;
        }

        auto* wrapper = static_cast<fs_gltf_loader*>(loaderHandle);

        // Assets still alive reference the loader; destroy them first.
        while (!wrapper->assets.empty()) {
            fs_gltf_asset* asset = wrapper->assets.back();
            filamentsharp::destroyGltfAssetObjects(asset);
            filamentsharp::Registry::instance().removeChildLocked(asset);
            delete asset;
        }

        filamentsharp::destroyGltfLoaderObjects(wrapper);
        filamentsharp::Registry::instance().removeChildLocked(loaderHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// -------------------------------------------------------------- gltf asset

FILAMENTSHARP_API fs_gltf_asset_t* fs_gltf_loader_load(fs_engine_t* engineHandle,
        fs_gltf_loader_t* loaderHandle, const uint8_t* content, uintptr_t size) {
    FS_TRY
        if (content == nullptr || size == 0) {
            filamentsharp::setLastError("gltf content is null or empty");
            return nullptr;
        }
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = nullptr;
        fs_gltf_loader* loader = nullptr;
        switch (filamentsharp::acquireChild(lock, engineHandle, loaderHandle,
                                            &engine, &loader)) {
            case filamentsharp::ChildCheck::Ok:
                break;
            default:
                // Error text already set by acquireChild.
                return nullptr;
        }

        filament::gltfio::FilamentAsset* asset =
                loader->loader->createAsset(content, uint32_t(size));
        if (asset == nullptr) {
            filamentsharp::setLastError("AssetLoader::createAsset failed "
                    "(invalid or unsupported glTF/GLB content)");
            return nullptr;
        }

        // Load embedded buffers and textures (GLB) and upload vertex data.
        // External resources are not fetched (the bridge performs no
        // filesystem access); use self-contained GLB files.
        if (!loader->resources->loadResources(asset)) {
            loader->loader->destroyAsset(asset);
            filamentsharp::setLastError("ResourceLoader::loadResources failed "
                    "(missing or corrupt buffer/texture data)");
            return nullptr;
        }
        asset->releaseSourceData();

        auto* wrapper = new fs_gltf_asset(asset, loader);
        loader->assets.push_back(wrapper);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_gltf_asset>,
                &filamentsharp::destroyChildObject<fs_gltf_asset>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_destroy(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle) {
    FS_TRY
        if (engineHandle == nullptr || assetHandle == nullptr) {
            filamentsharp::setLastError("engine or gltf asset handle is null");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_OK; // engine teardown owns the cleanup (LIFO pass)
        }
        if (filamentsharp::Registry::instance().ownerOfLocked(assetHandle) != engine) {
            filamentsharp::setLastError("unknown gltf asset handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_gltf_asset*>(assetHandle);
        filamentsharp::destroyGltfAssetObjects(wrapper);
        filamentsharp::Registry::instance().removeChildLocked(assetHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_entity_count(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, uint32_t* outCount) {
    FS_TRY
        if (outCount == nullptr) {
            filamentsharp::setLastError("out_count is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outCount = 0;
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        *outCount = uint32_t(wrapper->asset->getEntityCount());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_entities(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, fs_entity_id* outEntities,
        uint32_t capacity, uint32_t* outWritten) {
    FS_TRY
        if (outWritten == nullptr) {
            filamentsharp::setLastError("out_written is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outWritten = 0;
        if (outEntities == nullptr && capacity > 0) {
            filamentsharp::setLastError("out_entities is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        const utils::Entity* entities = wrapper->asset->getEntities();
        const size_t count = wrapper->asset->getEntityCount();
        uint32_t written = 0;
        for (uint32_t i = 0; i < capacity && i < count; i++) {
            outEntities[i] = fs_entity_id(entities[i].getId());
            written++;
        }
        *outWritten = written;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_root(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, fs_entity_id* outRoot) {
    FS_TRY
        if (outRoot == nullptr) {
            filamentsharp::setLastError("out_root is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outRoot = 0;
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        // The instance root is the scene graph root for regular assets.
        *outRoot = fs_entity_id(
                wrapper->asset->getInstance()->getRoot().getId());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_renderable_count(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, uint32_t* outCount) {
    FS_TRY
        if (outCount == nullptr) {
            filamentsharp::setLastError("out_count is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outCount = 0;
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        *outCount = uint32_t(wrapper->asset->getRenderableEntityCount());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_bounding_box(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, float* outMin, float* outMax) {
    FS_TRY
        if (outMin == nullptr || outMax == nullptr) {
            filamentsharp::setLastError("out_min or out_max is null");
            return FS_ERROR_INVALID_ARG;
        }
        for (int i = 0; i < 3; i++) {
            outMin[i] = 0.0f;
            outMax[i] = 0.0f;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        const filament::Aabb box = wrapper->asset->getBoundingBox();
        outMin[0] = box.min.x; outMin[1] = box.min.y; outMin[2] = box.min.z;
        outMax[0] = box.max.x; outMax[1] = box.max.y; outMax[2] = box.max.z;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_get_animation_count(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, uint32_t* outCount) {
    FS_TRY
        if (outCount == nullptr) {
            filamentsharp::setLastError("out_count is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outCount = 0;
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        filament::gltfio::Animator* animator =
                wrapper->asset->getInstance()->getAnimator();
        if (animator == nullptr) {
            filamentsharp::setLastError("asset has no animator");
            return FS_ERROR;
        }
        *outCount = uint32_t(animator->getAnimationCount());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_gltf_asset_apply_animation(fs_engine_t* engineHandle,
        fs_gltf_asset_t* assetHandle, uint32_t animationIndex, double timeSeconds) {
    FS_TRY
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_gltf_asset, wrapper, assetHandle)
        (void)engine; (void)lock;
        filament::gltfio::Animator* animator =
                wrapper->asset->getInstance()->getAnimator();
        if (animator == nullptr) {
            filamentsharp::setLastError("asset has no animator");
            return FS_ERROR;
        }
        if (animationIndex >= animator->getAnimationCount()) {
            filamentsharp::setLastError("animation index out of range");
            return FS_ERROR_INVALID_ARG;
        }
        animator->applyAnimation(animationIndex, float(timeSeconds));
        animator->updateBoneMatrices();
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

} // extern "C"
