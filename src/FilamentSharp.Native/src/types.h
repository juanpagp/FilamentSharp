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

// Adapter matching the registry's destroyObject signature.
template <typename T>
void destroyChildObject(filament::Engine* engine, void* child) {
    destroyFilamentObject(engine, static_cast<T*>(child));
}

} // namespace filamentsharp

#endif // FILAMENTSHARP_TYPES_H
