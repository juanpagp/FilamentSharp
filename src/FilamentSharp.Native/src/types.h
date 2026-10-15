// Definitions of the opaque child wrapper types declared in abi.h.
//
// These structs ARE the C types the ABI talks about (global scope, like
// fs_engine in registry.h). Kept in one shared internal header so every
// bridge translation unit sees complete types.
#ifndef FILAMENTSHARP_TYPES_H
#define FILAMENTSHARP_TYPES_H

#include <filament/Camera.h>
#include <filament/IndexBuffer.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/VertexBuffer.h>

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

#endif // FILAMENTSHARP_TYPES_H
