// Materials, textures, lights, and renderables.
#include "bridge.h"
#include "child.h"
#include "registry.h"
#include "types.h"

#include <filament/Box.h>
#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>
#include <backend/BufferDescriptor.h>
#include <backend/DriverEnums.h>
#include <backend/PixelBufferDescriptor.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <utils/Entity.h>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void freeCopiedBuffer(void* buffer, size_t, void*) {
    std::free(buffer);
}

// The .filamat container produced by matc is a sequence of chunks: an 8-byte
// ASCII tag stored byte-reversed, a uint32 little-endian payload size, then
// the payload; the first chunk is always the version chunk ("MAT_VERS").
//
// Filament's own parser does not fully pre-validate chunk headers against
// the package size: a truncated package can make it read past the end of the
// buffer (verified against the pinned v1.78.0 release - a 4-byte package
// segfaults inside Material::Builder::build, while well-formed-but-invalid
// packages throw a catchable panic). The bridge therefore walks the chunk
// chain first and only hands the package to Filament when the walk consumes
// exactly the whole package, so no chunk header or payload can point outside
// the buffer.
bool materialPackageLooksValid(const uint8_t* data, uintptr_t size) {
    if (data == nullptr || size < 12) {
        return false;
    }
    static const uint8_t kVersionTag[8] = { 'S', 'R', 'E', 'V', '_', 'T', 'A', 'M' };
    if (std::memcmp(data, kVersionTag, sizeof(kVersionTag)) != 0) {
        return false;
    }
    uintptr_t offset = 0;
    while (offset < size) {
        if (size - offset < 12) {
            return false; // truncated chunk header
        }
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, data + offset + 8, sizeof(chunkSize));
        if (chunkSize > size - offset - 12) {
            return false; // chunk payload extends beyond the package
        }
        offset += 12 + chunkSize;
    }
    return offset == size;
}

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

// ------------------------------------------------------------------ material

FILAMENTSHARP_API fs_material_t* fs_material_create(fs_engine_t* engineHandle,
        const uint8_t* packageData, uintptr_t packageSize) {
    FS_TRY
        if (packageData == nullptr || packageSize == 0) {
            filamentsharp::setLastError("material package data is null or empty");
            return nullptr;
        }
        // Structural pre-validation (see materialPackageLooksValid): keeps
        // Filament's parser inside the buffer for truncated/corrupt packages.
        if (!materialPackageLooksValid(packageData, packageSize)) {
            filamentsharp::setLastError("invalid .filamat package "
                    "(truncated or corrupt chunk layout)");
            return nullptr;
        }
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::Material* material = filament::Material::Builder()
                .package(packageData, packageSize)
                .build(*engine->engine);
        if (material == nullptr) {
            filamentsharp::setLastError("Material::Builder::build failed "
                    "(invalid or incompatible .filamat package)");
            return nullptr;
        }
        auto* wrapper = new fs_material(material);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_material>,
                &filamentsharp::destroyChildObject<fs_material>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_material_destroy(fs_engine_t* engineHandle,
        fs_material_t* materialHandle) {
    FS_TRY
        if (engineHandle == nullptr || materialHandle == nullptr) {
            filamentsharp::setLastError("engine or material handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(materialHandle) != engine) {
            filamentsharp::setLastError("unknown material handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_material*>(materialHandle);
        // Note: Filament refuses to destroy a material while instances are
        // still alive (PreconditionPanic -> mapped to FS_ERROR here).
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(materialHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_get_parameter_count(fs_engine_t* engineHandle,
        fs_material_t* materialHandle, uint32_t* outCount) {
    FS_TRY
        if (outCount == nullptr) {
            filamentsharp::setLastError("out_count is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outCount = 0;
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material, material, materialHandle)
        (void)engine; (void)lock;
        *outCount = uint32_t(material->material->getParameterCount());
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_get_parameter_names(fs_engine_t* engineHandle,
        fs_material_t* materialHandle, const char** outNames, uint32_t capacity,
        uint32_t* outWritten) {
    FS_TRY
        if (outWritten == nullptr) {
            filamentsharp::setLastError("out_written is null");
            return FS_ERROR_INVALID_ARG;
        }
        *outWritten = 0;
        if (outNames == nullptr && capacity > 0) {
            filamentsharp::setLastError("out_names is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material, material, materialHandle)
        (void)engine; (void)lock;
        const size_t count = material->material->getParameterCount();
        std::vector<filament::Material::ParameterInfo> info(count);
        material->material->getParameters(info.data(), count);
        uint32_t written = 0;
        for (uint32_t i = 0; i < capacity && i < count; i++) {
            outNames[i] = info[i].name;
            written++;
        }
        *outWritten = written;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_material_instance_t* fs_material_create_instance(
        fs_engine_t* engineHandle, fs_material_t* materialHandle) {
    FS_TRY
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = nullptr;
        fs_material* material = nullptr;
        switch (filamentsharp::acquireChild(lock, engineHandle, materialHandle,
                                            &engine, &material)) {
            case filamentsharp::ChildCheck::Ok:
                break;
            default:
                // Error text already set by acquireChild.
                return nullptr;
        }
        filament::MaterialInstance* instance = material->material->createInstance();
        if (instance == nullptr) {
            filamentsharp::setLastError("Material::createInstance returned null");
            return nullptr;
        }
        auto* wrapper = new fs_material_instance(instance);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_material_instance>,
                &filamentsharp::destroyChildObject<fs_material_instance>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_material_instance_destroy(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle) {
    FS_TRY
        if (engineHandle == nullptr || instanceHandle == nullptr) {
            filamentsharp::setLastError("engine or material instance handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(instanceHandle) != engine) {
            filamentsharp::setLastError("unknown material instance handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_material_instance*>(instanceHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(instanceHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_float(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name, float value) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        mi->instance->setParameter(name, value);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_float3(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name,
        float x, float y, float z) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        mi->instance->setParameter(name, filament::math::float3{x, y, z});
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_float4(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name,
        float x, float y, float z, float w) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        mi->instance->setParameter(name, filament::math::float4{x, y, z, w});
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_bool(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name, int32_t value) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        mi->instance->setParameter(name, value != 0);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_int(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name, int32_t value) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        mi->instance->setParameter(name, value);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_material_instance_set_texture(fs_engine_t* engineHandle,
        fs_material_instance_t* instanceHandle, const char* name,
        fs_texture_t* textureHandle, int32_t minFilter, int32_t magFilter,
        int32_t wrapS, int32_t wrapT) {
    FS_TRY
        if (name == nullptr) {
            filamentsharp::setLastError("parameter name is null");
            return FS_ERROR_INVALID_ARG;
        }
        if (minFilter < 0 || minFilter > 5 || magFilter < 0 || magFilter > 1 ||
            wrapS < 0 || wrapS > 2 || wrapT < 0 || wrapT > 2) {
            filamentsharp::setLastError("invalid sampler filter/wrap value");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_material_instance, mi, instanceHandle)
        (void)engine; (void)lock;
        fs_texture* texture = nullptr;
        FS_CHECK_CHILD_HELD(fs_texture, texture, textureHandle)

        const filament::TextureSampler sampler{
                filament::TextureSampler::MinFilter(minFilter),
                filament::TextureSampler::MagFilter(magFilter),
                filament::TextureSampler::WrapMode(wrapS)};
        mi->instance->setParameter(name, texture->texture, sampler);
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// ------------------------------------------------------------------ texture

FILAMENTSHARP_API fs_texture_t* fs_texture_create_2d(fs_engine_t* engineHandle,
        uint32_t width, uint32_t height, uint32_t levels,
        fs_texture_internal_format format) {
    FS_TRY
        if (width == 0 || height == 0 || levels == 0) {
            filamentsharp::setLastError("texture dimensions and levels must be non-zero");
            return nullptr;
        }
        if (format != FS_TEXTURE_RGBA8 && format != FS_TEXTURE_SRGB8_A8) {
            filamentsharp::setLastError("unsupported texture internal format");
            return nullptr;
        }
        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }
        filament::Texture* texture = filament::Texture::Builder()
                .width(width)
                .height(height)
                .levels(uint8_t(levels))
                .sampler(filament::Texture::Sampler::SAMPLER_2D)
                .format(format == FS_TEXTURE_RGBA8
                        ? filament::Texture::InternalFormat::RGBA8
                        : filament::Texture::InternalFormat::SRGB8_A8)
                .build(*engine->engine);
        if (texture == nullptr) {
            filamentsharp::setLastError("Texture::Builder::build failed");
            return nullptr;
        }
        auto* wrapper = new fs_texture(texture);
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_texture>,
                &filamentsharp::destroyChildObject<fs_texture>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_texture_set_image_rgba8(fs_engine_t* engineHandle,
        fs_texture_t* textureHandle, uint32_t level, uint32_t width, uint32_t height,
        const uint8_t* rgba, uintptr_t size) {
    FS_TRY
        if (rgba == nullptr || size == 0) {
            filamentsharp::setLastError("texture data is null or empty");
            return FS_ERROR_INVALID_ARG;
        }
        const size_t expected = size_t(width) * size_t(height) * 4;
        if (size < expected) {
            filamentsharp::setLastError("texture data smaller than width*height*4");
            return FS_ERROR_INVALID_ARG;
        }
        FS_ACQUIRE_CHILD(lock, engine, engineHandle, fs_texture, texture, textureHandle)
        (void)engine; (void)lock;

        void* copy = std::malloc(size);
        if (copy == nullptr) {
            filamentsharp::setLastError("out of memory copying texture data");
            return FS_ERROR;
        }
        std::memcpy(copy, rgba, size);
        filament::backend::PixelBufferDescriptor descriptor(copy, size,
                filament::backend::PixelDataFormat::RGBA,
                filament::backend::PixelDataType::UBYTE,
                &freeCopiedBuffer, nullptr);
        texture->texture->setImage(*engine->engine, level, std::move(descriptor));
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

FILAMENTSHARP_API fs_status fs_texture_destroy(fs_engine_t* engineHandle,
        fs_texture_t* textureHandle) {
    FS_TRY
        if (engineHandle == nullptr || textureHandle == nullptr) {
            filamentsharp::setLastError("engine or texture handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(textureHandle) != engine) {
            filamentsharp::setLastError("unknown texture handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_texture*>(textureHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(textureHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// -------------------------------------------------------------------- light

FILAMENTSHARP_API fs_light_t* fs_light_create(fs_engine_t* engineHandle,
        fs_entity_id entity, fs_light_type type,
        float r, float g, float b, float intensity,
        int32_t hasDirection, double dirX, double dirY, double dirZ,
        int32_t hasPosition, double posX, double posY, double posZ,
        float falloff, int32_t castShadows) {
    FS_TRY
        if (entity == 0) {
            filamentsharp::setLastError("light entity must be non-zero");
            return nullptr;
        }
        using LT = filament::LightManager::Type;
        LT lightType;
        switch (type) {
            case FS_LIGHT_SUN: lightType = LT::SUN; break;
            case FS_LIGHT_DIRECTIONAL: lightType = LT::DIRECTIONAL; break;
            case FS_LIGHT_POINT: lightType = LT::POINT; break;
            case FS_LIGHT_FOCUSED_SPOT: lightType = LT::FOCUSED_SPOT; break;
            case FS_LIGHT_SPOT: lightType = LT::SPOT; break;
            default:
                filamentsharp::setLastError("invalid light type");
                return nullptr;
        }
        const bool needsDirection = type == FS_LIGHT_SUN || type == FS_LIGHT_DIRECTIONAL ||
                                    type == FS_LIGHT_FOCUSED_SPOT || type == FS_LIGHT_SPOT;
        const bool needsPosition = type == FS_LIGHT_POINT || type == FS_LIGHT_FOCUSED_SPOT ||
                                   type == FS_LIGHT_SPOT;
        if (needsDirection && hasDirection == 0) {
            filamentsharp::setLastError("this light type requires a direction");
            return nullptr;
        }
        if (needsPosition && hasPosition == 0) {
            filamentsharp::setLastError("this light type requires a position");
            return nullptr;
        }

        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }

        auto builder = filament::LightManager::Builder(lightType)
                .color({r, g, b})
                .intensity(intensity)
                .castShadows(castShadows != 0);
        if (hasDirection != 0) {
            builder.direction(filament::math::float3{float(dirX), float(dirY), float(dirZ)});
        }
        if (hasPosition != 0) {
            builder.position(filament::math::float3{float(posX), float(posY), float(posZ)});
        }
        if (falloff > 0.0f) {
            builder.falloff(falloff);
        }
        const filament::LightManager::Builder::Result result =
                builder.build(*engine->engine, utils::Entity::import(int32_t(entity)));
        if (result != filament::LightManager::Builder::Result::Success) {
            filamentsharp::setLastError("LightManager::Builder::build failed "
                    "(invalid light parameters or entity)");
            return nullptr;
        }

        auto* wrapper = new fs_light(utils::Entity::import(int32_t(entity)));
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_light>,
                &filamentsharp::destroyChildObject<fs_light>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_light_destroy(fs_engine_t* engineHandle,
        fs_light_t* lightHandle) {
    FS_TRY
        if (engineHandle == nullptr || lightHandle == nullptr) {
            filamentsharp::setLastError("engine or light handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(lightHandle) != engine) {
            filamentsharp::setLastError("unknown light handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_light*>(lightHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(lightHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

// --------------------------------------------------------------- renderable

FILAMENTSHARP_API fs_renderable_t* fs_renderable_create(fs_engine_t* engineHandle,
        const fs_renderable_desc* desc) {
    FS_TRY
        if (desc == nullptr) {
            filamentsharp::setLastError("renderable description is null");
            return nullptr;
        }
        if (desc->entity == 0) {
            filamentsharp::setLastError("renderable entity must be non-zero");
            return nullptr;
        }
        if (desc->vertices == nullptr || desc->indices == nullptr ||
            desc->material == nullptr) {
            filamentsharp::setLastError(
                    "renderable requires vertices, indices, and a material instance");
            return nullptr;
        }
        if (desc->primitive_count == 0) {
            filamentsharp::setLastError("primitive count must be non-zero");
            return nullptr;
        }

        std::unique_lock<std::mutex> lock;
        fs_engine* engine = acquireEngine(lock, engineHandle);
        if (engine == nullptr) {
            return nullptr;
        }

        // All referenced children must belong to this engine.
        if (filamentsharp::Registry::instance().ownerOfLocked(desc->vertices) != engine ||
            filamentsharp::Registry::instance().ownerOfLocked(desc->indices) != engine ||
            filamentsharp::Registry::instance().ownerOfLocked(desc->material) != engine) {
            filamentsharp::setLastError(
                    "renderable references buffers or material from a different engine "
                    "(or destroyed handles)");
            return nullptr;
        }
        auto* vertices = static_cast<fs_vertex_buffer*>(desc->vertices);
        auto* indices = static_cast<fs_index_buffer*>(desc->indices);
        auto* material = static_cast<fs_material_instance*>(desc->material);

        filament::RenderableManager::Builder builder(1);
        builder.geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                        vertices->vertexBuffer, indices->indexBuffer,
                        desc->primitive_offset, desc->primitive_count);
        builder.boundingBox(filament::Box{
                        filament::math::float3{desc->bounds_min[0], desc->bounds_min[1],
                                               desc->bounds_min[2]},
                        filament::math::float3{desc->bounds_max[0], desc->bounds_max[1],
                                               desc->bounds_max[2]}});
        builder.material(0, material->instance);
        builder.culling(desc->culling != 0);
        builder.castShadows(desc->cast_shadows != 0);
        builder.receiveShadows(desc->receive_shadows != 0);

        const filament::RenderableManager::Builder::Result result =
                builder.build(*engine->engine, utils::Entity::import(int32_t(desc->entity)));
        if (result != filament::RenderableManager::Builder::Result::Success) {
            filamentsharp::setLastError("RenderableManager::Builder::build failed "
                    "(invalid parameters, or buffers/material missing required attributes)");
            return nullptr;
        }

        auto* wrapper = new fs_renderable(utils::Entity::import(int32_t(desc->entity)));
        filamentsharp::Registry::instance().addChildLocked(
                engine, wrapper, &filamentsharp::disposeChild<fs_renderable>,
                &filamentsharp::destroyChildObject<fs_renderable>);
        return wrapper;
    FS_END_VALUE(nullptr)
}

FILAMENTSHARP_API fs_status fs_renderable_destroy(fs_engine_t* engineHandle,
        fs_renderable_t* renderableHandle) {
    FS_TRY
        if (engineHandle == nullptr || renderableHandle == nullptr) {
            filamentsharp::setLastError("engine or renderable handle is null");
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
        if (filamentsharp::Registry::instance().ownerOfLocked(renderableHandle) != engine) {
            filamentsharp::setLastError("unknown renderable handle");
            return FS_ERROR_INVALID_ARG;
        }
        auto* wrapper = static_cast<fs_renderable*>(renderableHandle);
        filamentsharp::destroyFilamentObject(engine->engine, wrapper);
        filamentsharp::Registry::instance().removeChildLocked(renderableHandle);
        delete wrapper;
        return FS_OK;
    FS_END_VALUE(FS_ERROR)
}

} // extern "C"
