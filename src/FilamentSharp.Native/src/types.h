// Definitions of the opaque child wrapper types declared in abi.h.
//
// These structs ARE the C types the ABI talks about (global scope, like
// fs_engine in registry.h). Kept in one shared internal header so every
// bridge translation unit sees complete types - and so destroyFilamentObject
// below has one home for the type-correct Filament destruction of every
// child kind (used both by the explicit fs_*_destroy entry points and by the
// LIFO engine teardown path in engine.cpp).
#ifndef FILAMENTSHARP_TYPES_H
#define FILAMENTSHARP_TYPES_H

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>

#include <cstdlib>
#include <utils/Entity.h>
#include <vector>

// gltfio companion objects are held by pointer only; full definitions live
// in gltf.cpp, which includes the real gltfio headers.
namespace filament::gltfio {
class AssetLoader;
class MaterialProvider;
class TextureProvider;
class ResourceLoader;
class FilamentAsset;
}

struct fs_renderer {
    filament::Renderer* renderer = nullptr;

    // Pending synchronous readback state (one at a time per renderer).
    uint8_t* pendingPixels = nullptr;
    size_t pendingSize = 0;
    bool pendingDone = false;

    explicit fs_renderer(filament::Renderer* r) : renderer(r) {}

    ~fs_renderer() { std::free(pendingPixels); }
};

struct fs_swap_chain {
    filament::SwapChain* swapChain = nullptr;
    explicit fs_swap_chain(filament::SwapChain* s) : swapChain(s) {}
};

struct fs_view {
    filament::View* view = nullptr;
    explicit fs_view(filament::View* v) : view(v) {}
};

struct fs_scene {
    filament::Scene* scene = nullptr;
    explicit fs_scene(filament::Scene* s) : scene(s) {}
};

struct fs_camera {
    filament::Camera* camera = nullptr;
    utils::Entity entity{};
    explicit fs_camera(filament::Camera* c, utils::Entity e) : camera(c), entity(e) {}
};

struct fs_vertex_buffer {
    filament::VertexBuffer* vertexBuffer = nullptr;
    explicit fs_vertex_buffer(filament::VertexBuffer* v) : vertexBuffer(v) {}
};

struct fs_index_buffer {
    filament::IndexBuffer* indexBuffer = nullptr;
    explicit fs_index_buffer(filament::IndexBuffer* i) : indexBuffer(i) {}
};

struct fs_material {
    filament::Material* material = nullptr;
    explicit fs_material(filament::Material* m) : material(m) {}
};

struct fs_material_instance {
    filament::MaterialInstance* instance = nullptr;
    explicit fs_material_instance(filament::MaterialInstance* i) : instance(i) {}
};

struct fs_texture {
    filament::Texture* texture = nullptr;
    explicit fs_texture(filament::Texture* t) : texture(t) {}
};

struct fs_light {
    utils::Entity entity{};
    explicit fs_light(utils::Entity e) : entity(e) {}
};

struct fs_renderable {
    utils::Entity entity{};
    explicit fs_renderable(utils::Entity e) : entity(e) {}
};

// ---- glTF (full definitions here so the destroy dispatch below sees
// complete types; the gltfio companion objects are torn down in gltf.cpp) --

struct fs_gltf_asset; // the loader tracks its live assets

struct fs_gltf_loader {
    filament::gltfio::AssetLoader* loader = nullptr;
    filament::gltfio::MaterialProvider* materials = nullptr;
    filament::gltfio::TextureProvider* decoder = nullptr;
    filament::gltfio::ResourceLoader* resources = nullptr;
    // Assets created by this loader and still alive. The loader must
    // outlive them (creation-order registration guarantees LIFO destruction
    // at engine death); explicit loader destroy tears them down first.
    std::vector<fs_gltf_asset*> assets;

    fs_gltf_loader(filament::gltfio::AssetLoader* l,
            filament::gltfio::MaterialProvider* m,
            filament::gltfio::TextureProvider* d,
            filament::gltfio::ResourceLoader* r)
        : loader(l), materials(m), decoder(d), resources(r) {}
};

struct fs_gltf_asset {
    filament::gltfio::FilamentAsset* asset = nullptr;
    fs_gltf_loader* owner = nullptr;

    fs_gltf_asset(filament::gltfio::FilamentAsset* a, fs_gltf_loader* o)
        : asset(a), owner(o) {}
};

namespace filamentsharp {

// Type-correct destruction of the Filament object owned by each wrapper.
// One overload per wrapper type; the engine teardown path and the explicit
// fs_*_destroy entry points share these so the destruction call can never
// drift between the two.
inline void destroyFilamentObject(filament::Engine* engine, fs_renderer* w) {
    engine->destroy(w->renderer);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_swap_chain* w) {
    engine->destroy(w->swapChain);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_view* w) {
    engine->destroy(w->view);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_scene* w) {
    engine->destroy(w->scene);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_camera* w) {
    // Filament exposes both destroy(Camera*) and destroyCameraComponent;
    // the component form is the documented teardown for entity-backed cameras.
    engine->destroyCameraComponent(w->entity);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_vertex_buffer* w) {
    engine->destroy(w->vertexBuffer);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_index_buffer* w) {
    engine->destroy(w->indexBuffer);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_material* w) {
    engine->destroy(w->material);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_material_instance* w) {
    engine->destroy(w->instance);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_texture* w) {
    engine->destroy(w->texture);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_light* w) {
    engine->getLightManager().destroy(w->entity);
}
inline void destroyFilamentObject(filament::Engine* engine, fs_renderable* w) {
    engine->getRenderableManager().destroy(w->entity);
}

// glTF teardown implementations live in gltf.cpp (they need gltfio headers
// that are not included here); declared for the dispatch below.
void destroyGltfLoaderObjects(fs_gltf_loader* wrapper);
void destroyGltfAssetObjects(fs_gltf_asset* wrapper);

inline void destroyFilamentObject(filament::Engine*, fs_gltf_loader* w) {
    destroyGltfLoaderObjects(w);
}

inline void destroyFilamentObject(filament::Engine*, fs_gltf_asset* w) {
    destroyGltfAssetObjects(w);
}

// Adapter matching the registry's destroyObject signature.
template <typename T>
void destroyChildObject(filament::Engine* engine, void* child) {
    destroyFilamentObject(engine, static_cast<T*>(child));
}

} // namespace filamentsharp

#endif // FILAMENTSHARP_TYPES_H
