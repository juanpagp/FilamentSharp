// Scene construction entry points: geometry buffers, transforms, and scene
// entity membership.
#include "bridge.h"
#include "child.h"
#include "types.h"
#include "registry.h"

#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/MaterialEnums.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <backend/BufferDescriptor.h>
#include <math/mat4.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>

#include <cstdlib>
#include <cstring>

namespace {

// BufferDescriptor callback: the bridge hands Filament a private copy and
// frees it when Filament is done with it. Must be lightweight and must not
// call Filament APIs (documented contract of BufferDescriptor::Callback).
void freeCopiedBuffer(void* buffer, size_t, void*) {
    std::free(buffer);
}

bool attributeFromAbi(int32_t attribute, filament::VertexAttribute* out) {
    switch (attribute) {
        case FS_VERTEX_POSITION: *out = filament::VertexAttribute::POSITION; return true;
        case FS_VERTEX_TANGENTS: *out = filament::VertexAttribute::TANGENTS; return true;
        case FS_VERTEX_COLOR: *out = filament::VertexAttribute::COLOR; return true;
        case FS_VERTEX_UV0: *out = filament::VertexAttribute::UV0; return true;
        case FS_VERTEX_UV1: *out = filament::VertexAttribute::UV1; return true;
        case FS_VERTEX_BONE_INDICES: *out = filament::VertexAttribute::BONE_INDICES; return true;
        case FS_VERTEX_BONE_WEIGHTS: *out = filament::VertexAttribute::BONE_WEIGHTS; return true;
        case FS_VERTEX_CUSTOM0: *out = filament::VertexAttribute::CUSTOM0; return true;
        case FS_VERTEX_CUSTOM1: *out = filament::VertexAttribute::CUSTOM1; return true;
        case FS_VERTEX_CUSTOM2: *out = filament::VertexAttribute::CUSTOM2; return true;
        case FS_VERTEX_CUSTOM3: *out = filament::VertexAttribute::CUSTOM3; return true;
        case FS_VERTEX_CUSTOM4: *out = filament::VertexAttribute::CUSTOM4; return true;
        case FS_VERTEX_CUSTOM5: *out = filament::VertexAttribute::CUSTOM5; return true;
        case FS_VERTEX_CUSTOM6: *out = filament::VertexAttribute::CUSTOM6; return true;
        case FS_VERTEX_CUSTOM7: *out = filament::VertexAttribute::CUSTOM7; return true;
    }
    return false;
}

bool elementTypeFromAbi(int32_t type, filament::backend::ElementType* out) {
    if (type < 0 || type > 25) {
        return false;
    }
    *out = static_cast<filament::backend::ElementType>(type);
    return true;
}

// Rebuilds the engine lock helper (same as rendering.cpp) for creates.
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

filament::math::mat4f mat4fFromColumnMajor(const float* m) {
    filament::math::mat4f result{};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            result[c][r] = m[c * 4 + r];
        }
    }
    return result;
}

void mat4fToColumnMajor(const filament::math::mat4f& m, float* out) {
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            out[c * 4 + r] = m[c][r];
        }
    }
}

} // namespace

extern "C" {

// ------------------------------------------------------------ vertex buffer

FILAMENTSHARP_API fs_vertex_buffer_t* fs_vertex_buffer_create(
        fs_engine_t* engineHandle, uint32_t vertexCount, uint32_t bufferCount,
        const fs_vertex_attribute_desc* attributes, uint32_t attributeCount) {
    FS_TRY
        if (vertexCount == 0 || bufferCount == 0) {
            filamentsharp::setLastError("vertex count and buffer count must be non-zero");
            return nullptr;
        }
        if (attributes == nullptr || attributeCount == 0) {
            filamentsharp::setLastError("at least one vertex attribute is required");
            return nullptr;
        }
        for (uint32_t i = 0; i < attributeCount; i++) {
            filament::VertexAttribute attribute;
            if (!attributeFromAbi(attributes[i].attribute, &attribute)) {
                filamentsharp::setLastError("invalid vertex attribute value");
                return nullptr;
            }
            filament::backend::ElementType elementType;
            if (!elementTypeFromAbi(attributes[i].element_type, &elementType)) {
                filamentsharp::setLastError("invalid vertex element type");
                return nullptr;
            }
            if (attributes[i].buffer_index >= bufferCount) {
                filamentsharp::setLastError("attribute buffer index out of range");
                return nullptr;
            }
        }

        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }

        filament::VertexBuffer::Builder builder;
        builder.vertexCount(vertexCount).bufferCount(bufferCount);
        for (uint32_t i = 0; i < attributeCount; i++) {
            filament::VertexAttribute attribute;
            attributeFromAbi(attributes[i].attribute, &attribute);
            filament::backend::ElementType elementType;
            elementTypeFromAbi(attributes[i].element_type, &elementType);
            builder.attribute(attribute, uint8_t(attributes[i].buffer_index),
                    elementType, attributes[i].byte_offset,
                    uint8_t(attributes[i].byte_stride));
        }
        filament::VertexBuffer* vertexBuffer = builder.build(*engine->engine);
        if (vertexBuffer == nullptr) {
            filamentsharp::setLastError("filament::VertexBuffer::Builder::build failed");
            return nullptr;
        }
        auto* wrapper = new fs_vertex_buffer(vertexBuffer);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_vertex_buffer>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_vertex_buffer_set_buffer(fs_engine_t* engineHandle,
        fs_vertex_buffer_t* vertexBufferHandle, uint32_t bufferIndex,
        const uint8_t* data, uintptr_t size) {
    FS_TRY
        if (data == nullptr || size == 0) {
            filamentsharp::setLastError("buffer data is null or empty");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_vertex_buffer, vb, vertexBufferHandle)
        (void)engine; (void)lock;

        void* copy = std::malloc(size);
        if (copy == nullptr) {
            filamentsharp::setLastError("out of memory copying vertex data");
            return FS_ERROR;
        }
        std::memcpy(copy, data, size);
        // Filament frees the copy through freeCopiedBuffer when done with it.
        filament::backend::BufferDescriptor descriptor(copy, size, &freeCopiedBuffer, nullptr);
        vb->vertexBuffer->setBufferAt(*engine->engine, uint8_t(bufferIndex),
                std::move(descriptor));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_vertex_buffer_destroy(fs_engine_t* engineHandle,
        fs_vertex_buffer_t* vertexBufferHandle) {
    FS_TRY
        if (engineHandle == nullptr || vertexBufferHandle == nullptr) {
            filamentsharp::setLastError("engine or vertex buffer handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(vertexBufferHandle) != engine) {
            filamentsharp::setLastError("unknown vertex buffer handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_vertex_buffer*>(vertexBufferHandle);
        engine->engine->destroy(wrapper->vertexBuffer);
        filamentsharp::Registry::instance().removeChildLocked(vertexBufferHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ------------------------------------------------------------- index buffer

FILAMENTSHARP_API fs_index_buffer_t* fs_index_buffer_create(fs_engine_t* engineHandle,
        uint32_t indexCount, fs_index_type indexType) {
    FS_TRY
        if (indexCount == 0) {
            filamentsharp::setLastError("index count must be non-zero");
            return nullptr;
        }
        const filament::IndexBuffer::IndexType type = indexType == FS_INDEX_UINT
                ? filament::IndexBuffer::IndexType::UINT
                : filament::IndexBuffer::IndexType::USHORT;

        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::IndexBuffer* indexBuffer =
                filament::IndexBuffer::Builder()
                        .indexCount(indexCount)
                        .bufferType(type)
                        .build(*engine->engine);
        if (indexBuffer == nullptr) {
            filamentsharp::setLastError("filament::IndexBuffer::Builder::build failed");
            return nullptr;
        }
        auto* wrapper = new fs_index_buffer(indexBuffer);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_index_buffer>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_index_buffer_set_buffer(fs_engine_t* engineHandle,
        fs_index_buffer_t* indexBufferHandle, const uint8_t* data, uintptr_t size) {
    FS_TRY
        if (data == nullptr || size == 0) {
            filamentsharp::setLastError("buffer data is null or empty");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_index_buffer, ib, indexBufferHandle)
        (void)engine; (void)lock;

        void* copy = std::malloc(size);
        if (copy == nullptr) {
            filamentsharp::setLastError("out of memory copying index data");
            return FS_ERROR;
        }
        std::memcpy(copy, data, size);
        filament::backend::BufferDescriptor descriptor(copy, size, &freeCopiedBuffer, nullptr);
        ib->indexBuffer->setBuffer(*engine->engine, std::move(descriptor));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_index_buffer_destroy(fs_engine_t* engineHandle,
        fs_index_buffer_t* indexBufferHandle) {
    FS_TRY
        if (engineHandle == nullptr || indexBufferHandle == nullptr) {
            filamentsharp::setLastError("engine or index buffer handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(indexBufferHandle) != engine) {
            filamentsharp::setLastError("unknown index buffer handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_index_buffer*>(indexBufferHandle);
        engine->engine->destroy(wrapper->indexBuffer);
        filamentsharp::Registry::instance().removeChildLocked(indexBufferHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ---------------------------------------------------------------- transforms

FILAMENTSHARP_API fs_status fs_transform_set_matrix(fs_engine_t* engineHandle,
        fs_entity_id entity, const float* columnMajor16) {
    FS_TRY
        if (columnMajor16 == nullptr) {
            filamentsharp::setLastError("matrix pointer is null");
            return FS_ERROR_INVALID_ARG;
        }
        if (entity == 0) {
            filamentsharp::setLastError("entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_ERROR_INVALIDATED;
        }
        auto& tm = engine->engine->getTransformManager();
        auto instance = tm.getInstance(utils::Entity::import(int32_t(entity)));
        if (!instance) {
            // SetTransform implicitly creates the transform component for the
            // entity (documented); matches how camera/renderable creation
            // attaches transforms upstream.
            tm.create(utils::Entity::import(int32_t(entity)),
                    filament::TransformManager::Instance{},
                    mat4fFromColumnMajor(columnMajor16));
            return FS_OK;
        }
        tm.setTransform(instance, mat4fFromColumnMajor(columnMajor16));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_transform_get_world_matrix(fs_engine_t* engineHandle,
        fs_entity_id entity, float* outMatrix16) {
    FS_TRY
        if (outMatrix16 == nullptr) {
            filamentsharp::setLastError("out_matrix is null");
            return FS_ERROR_INVALID_ARG;
        }
        if (entity == 0) {
            filamentsharp::setLastError("entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_ERROR_INVALIDATED;
        }
        auto& tm = engine->engine->getTransformManager();
        auto instance = tm.getInstance(utils::Entity::import(int32_t(entity)));
        if (!instance) {
            filamentsharp::setLastError("entity has no transform component");
            return FS_ERROR_INVALID_ARG;
        }
        mat4fToColumnMajor(tm.getWorldTransform(instance), outMatrix16);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_transform_set_parent(fs_engine_t* engineHandle,
        fs_entity_id child, fs_entity_id parent) {
    FS_TRY
        if (child == 0) {
            filamentsharp::setLastError("child entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        std::unique_lock<std::mutex> lock(filamentsharp::Registry::mutex());
        fs_engine* engine = filamentsharp::Registry::instance().engineLocked(engineHandle);
        if (engine == nullptr) {
            filamentsharp::setLastError("unknown engine handle");
            return FS_ERROR_INVALID_ARG;
        }
        if (!engine->alive.load()) {
            return FS_ERROR_INVALIDATED;
        }
        auto& tm = engine->engine->getTransformManager();
        auto childInstance = tm.getInstance(utils::Entity::import(int32_t(child)));
        if (!childInstance) {
            filamentsharp::setLastError("child entity has no transform component");
            return FS_ERROR_INVALID_ARG;
        }
        if (parent == 0) {
            tm.setParent(childInstance, filament::TransformManager::Instance{});
        } else {
            auto parentInstance = tm.getInstance(utils::Entity::import(int32_t(parent)));
            if (!parentInstance) {
                filamentsharp::setLastError("parent entity has no transform component");
                return FS_ERROR_INVALID_ARG;
            }
            tm.setParent(childInstance, parentInstance);
        }
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ------------------------------------------------------------ scene entities

FILAMENTSHARP_API fs_status fs_scene_add_entity(fs_engine_t* engineHandle,
        fs_scene_t* sceneHandle, fs_entity_id entity) {
    FS_TRY
        if (entity == 0) {
            filamentsharp::setLastError("entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_scene, scene, sceneHandle)
        (void)engine; (void)lock;
        scene->scene->addEntity(utils::Entity::import(int32_t(entity)));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_scene_remove_entity(fs_engine_t* engineHandle,
        fs_scene_t* sceneHandle, fs_entity_id entity) {
    FS_TRY
        if (entity == 0) {
            filamentsharp::setLastError("entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_scene, scene, sceneHandle)
        (void)engine; (void)lock;
        scene->scene->remove(utils::Entity::import(int32_t(entity)));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_scene_has_entity(fs_engine_t* engineHandle,
        fs_scene_t* sceneHandle, fs_entity_id entity, int32_t* outHas) {
    FS_TRY
        if (outHas == nullptr) {
            filamentsharp::setLastError("out_has is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outHas = 0;
        if (entity == 0) {
            filamentsharp::setLastError("entity must be non-zero");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_scene, scene, sceneHandle)
        (void)engine; (void)lock;
        *outHas = scene->scene->hasEntity(utils::Entity::import(int32_t(entity))) ? 1 : 0;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_scene_get_entity_count(fs_engine_t* engineHandle,
        fs_scene_t* sceneHandle, uint32_t* outCount) {
    FS_TRY
        if (outCount == nullptr) {
            filamentsharp::setLastError("out_count is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_scene, scene, sceneHandle)
        (void)engine; (void)lock;
        *outCount = uint32_t(scene->scene->getEntityCount());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

} // extern "C"
