// Materials, textures, lights, and renderables contract tests (NOOP backend
// where possible; .filamat packages compiled at build time by matc).
#include <cstdio>
#include <cstring>
#include <vector>

#include "filamentsharp/abi.h"

#ifndef FS_MATERIALS_DIR
#  error "FS_MATERIALS_DIR must be defined by the build"
#endif

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

static std::vector<uint8_t> readFile(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return {};
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    const size_t read = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (read != data.size()) {
        return {};
    }
    return data;
}

int main() {
    // Layout checks for the shared renderable description.
    static_assert(sizeof(fs_renderable_desc) == 80, "fs_renderable_desc layout");
    static_assert(offsetof(fs_renderable_desc, entity) == 0, "layout");
    static_assert(offsetof(fs_renderable_desc, vertices) == 8, "layout");
    static_assert(offsetof(fs_renderable_desc, indices) == 16, "layout");
    static_assert(offsetof(fs_renderable_desc, primitive_offset) == 24, "layout");
    static_assert(offsetof(fs_renderable_desc, primitive_count) == 28, "layout");
    static_assert(offsetof(fs_renderable_desc, bounds_min) == 32, "layout");
    static_assert(offsetof(fs_renderable_desc, bounds_max) == 44, "layout");
    static_assert(offsetof(fs_renderable_desc, material) == 56, "layout");
    static_assert(offsetof(fs_renderable_desc, culling) == 64, "layout");
    static_assert(FS_TEXTURE_RGBA8 == 30 && FS_TEXTURE_SRGB8_A8 == 31, "texture formats ABI");

    fs_engine_t* engine = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(engine != nullptr);
    if (engine == nullptr) {
        return 1;
    }

    // ---- Material ----------------------------------------------------------
    std::vector<uint8_t> package = readFile(FS_MATERIALS_DIR "/unlit_color.filamat");
    CHECK(!package.empty());
    if (package.empty()) {
        std::fprintf(stderr, "test_materials: missing .filamat; is matc part of the build?\n");
        return 1;
    }

    fs_material_t* material = fs_material_create(engine, package.data(), package.size());
    CHECK(material != nullptr);

    // Invalid package data is rejected, not fatal. This includes the
    // dangerous cases the bridge pre-validates: Filament's parser reads
    // past the buffer for truncated packages (verified: a 4-byte payload
    // segfaults inside Material::Builder::build without the check).
    const uint8_t junk[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    CHECK(fs_material_create(engine, junk, sizeof(junk)) == nullptr);
    CHECK(fs_material_create(engine, nullptr, 0) == nullptr);
    CHECK(fs_material_create(engine, junk, 4) == nullptr);          // truncated header
    CHECK(fs_material_create(engine, junk, 11) == nullptr);         // header minus one byte
    // Valid MAT_VERS tag + a chunk size that overruns the payload.
    const uint8_t overrun[16] = {
        'S', 'R', 'E', 'V', '_', 'T', 'A', 'M', 0xFF, 0xFF, 0x00, 0x00
    };
    CHECK(fs_material_create(engine, overrun, sizeof(overrun)) == nullptr);
    // Walkable chunk chain with a wrong first tag (not MAT_VERS).
    const uint8_t wrongTag[16] = {
        'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 4, 0, 0, 0, 78, 0, 0, 0
    };
    CHECK(fs_material_create(engine, wrongTag, sizeof(wrongTag)) == nullptr);

    uint32_t paramCount = 0;
    CHECK(fs_material_get_parameter_count(engine, material, &paramCount) == FS_OK);
    CHECK(paramCount == 1); // unlit_color declares one parameter: color

    const char* names[4] = {};
    uint32_t written = 0;
    CHECK(fs_material_get_parameter_names(engine, material, names, 4, &written) == FS_OK);
    CHECK(written == 1);
    CHECK(std::strcmp(names[0], "color") == 0);

    fs_material_instance_t* instance = fs_material_create_instance(engine, material);
    CHECK(instance != nullptr);

    CHECK(fs_material_instance_set_float3(engine, instance, "color", 1.0f, 0.0f, 0.0f)
          == FS_OK);
    // Unknown parameter names: the pinned Filament release raises a
    // precondition panic, which the bridge error model converts into an
    // error status (no exception crosses the ABI, no crash).
    CHECK(fs_material_instance_set_float(engine, instance, "nonexistent", 1.0f)
          != FS_OK);
    CHECK(fs_material_instance_set_float3(engine, instance, "also_nonexistent",
          1.0f, 2.0f, 3.0f) != FS_OK);

    // ---- Texture -------------------------------------------------------------
    fs_texture_t* texture = fs_texture_create_2d(engine, 4, 4, 1, FS_TEXTURE_RGBA8);
    CHECK(texture != nullptr);
    uint8_t pixels[4 * 4 * 4];
    std::memset(pixels, 0xFF, sizeof(pixels));
    CHECK(fs_texture_set_image_rgba8(engine, texture, 0, 4, 4, pixels, sizeof(pixels))
          == FS_OK);
    CHECK(fs_texture_set_image_rgba8(engine, texture, 0, 4, 4, pixels, 8)
          == FS_ERROR_INVALID_ARG); // too small
    CHECK(fs_texture_create_2d(engine, 0, 4, 1, FS_TEXTURE_RGBA8) == nullptr);
    CHECK(fs_texture_create_2d(engine, 4, 4, 1, fs_texture_internal_format(77)) == nullptr);

    // Sampler binding requires a sampler parameter: use the textured material.
    std::vector<uint8_t> litPackage = readFile(FS_MATERIALS_DIR "/lit_textured.filamat");
    CHECK(!litPackage.empty());
    fs_material_t* litMaterial = fs_material_create(engine, litPackage.data(), litPackage.size());
    CHECK(litMaterial != nullptr);
    fs_material_instance_t* litInstance = fs_material_create_instance(engine, litMaterial);
    CHECK(litInstance != nullptr);
    CHECK(fs_material_instance_set_texture(engine, litInstance, "baseColorMap", texture,
          FS_MIN_LINEAR, FS_MAG_LINEAR, FS_WRAP_CLAMP_TO_EDGE, FS_WRAP_CLAMP_TO_EDGE)
          == FS_OK);
    // Binding a texture to a non-sampler parameter is rejected.
    CHECK(fs_material_instance_set_texture(engine, litInstance, "roughness", texture,
          FS_MIN_LINEAR, FS_MAG_LINEAR, FS_WRAP_CLAMP_TO_EDGE, FS_WRAP_CLAMP_TO_EDGE)
          != FS_OK);

    // ---- Light ----------------------------------------------------------------
    fs_entity_id lightEntity = fs_entity_create();
    fs_light_t* light = fs_light_create(engine, lightEntity, FS_LIGHT_DIRECTIONAL,
            1.0f, 1.0f, 1.0f, 100000.0f, 1, 0.0, -1.0, 0.0, 0, 0.0, 0.0, 0.0, 0.0f, 0);
    CHECK(light != nullptr);

    // Directional without a direction is rejected.
    CHECK(fs_light_create(engine, fs_entity_create(), FS_LIGHT_DIRECTIONAL,
            1, 1, 1, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == nullptr);
    // Point requires a position.
    CHECK(fs_light_create(engine, fs_entity_create(), FS_LIGHT_POINT,
            1, 1, 1, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == nullptr);
    // Sun with direction.
    fs_light_t* sun = fs_light_create(engine, fs_entity_create(), FS_LIGHT_SUN,
            1, 1, 1, 120000.0f, 1, 0.1, -1.0, 0.2, 0, 0, 0, 0, 0.0f, 1);
    CHECK(sun != nullptr);
    CHECK(fs_light_destroy(engine, sun) == FS_OK);
    // Point with position and falloff.
    fs_light_t* point = fs_light_create(engine, fs_entity_create(), FS_LIGHT_POINT,
            1, 1, 1, 100.0f, 0, 0, 0, 0, 1, 0.0, 2.0, 0.0, 10.0f, 0);
    CHECK(point != nullptr);
    CHECK(fs_light_destroy(engine, point) == FS_OK);

    // ---- Renderable -------------------------------------------------------------
    const fs_vertex_attribute_desc attrs[] = { { FS_VERTEX_POSITION, FS_ELEMENT_FLOAT3, 0, 0, 12 } };
    fs_vertex_buffer_t* vb = fs_vertex_buffer_create(engine, 3, 1, attrs, 1);
    CHECK(vb != nullptr);
    float triangle[9] = { 0, 1, 0, -1, -1, 0, 1, -1, 0 };
    CHECK(fs_vertex_buffer_set_buffer(engine, vb, 0,
            reinterpret_cast<const uint8_t*>(triangle), sizeof(triangle)) == FS_OK);

    fs_index_buffer_t* ib = fs_index_buffer_create(engine, 3, FS_INDEX_USHORT);
    CHECK(ib != nullptr);
    uint16_t indices[3] = { 0, 1, 2 };
    CHECK(fs_index_buffer_set_buffer(engine, ib,
            reinterpret_cast<const uint8_t*>(indices), sizeof(indices)) == FS_OK);

    fs_entity_id mesh = fs_entity_create();
    fs_scene_t* scene = fs_scene_create(engine);
    CHECK(scene != nullptr);

    fs_renderable_desc desc{};
    desc.entity = mesh;
    desc.vertices = vb;
    desc.indices = ib;
    desc.primitive_offset = 0;
    desc.primitive_count = 3;
    desc.bounds_min[0] = -1; desc.bounds_min[1] = -1; desc.bounds_min[2] = -1;
    desc.bounds_max[0] = 1; desc.bounds_max[1] = 1; desc.bounds_max[2] = 1;
    desc.material = instance;
    desc.culling = 0;
    desc.cast_shadows = 0;
    desc.receive_shadows = 0;
    fs_renderable_t* renderable = fs_renderable_create(engine, &desc);
    CHECK(renderable != nullptr);

    CHECK(fs_scene_add_entity(engine, scene, mesh) == FS_OK);

    // Validation.
    CHECK(fs_renderable_create(engine, nullptr) == nullptr);
    fs_renderable_desc bad = desc;
    bad.entity = 0;
    CHECK(fs_renderable_create(engine, &bad) == nullptr);
    bad = desc;
    bad.material = nullptr;
    CHECK(fs_renderable_create(engine, &bad) == nullptr);
    bad = desc;
    bad.primitive_count = 0;
    CHECK(fs_renderable_create(engine, &bad) == nullptr);

    // ---- Teardown (documented order: children first) -----------------------------
    CHECK(fs_scene_remove_entity(engine, scene, mesh) == FS_OK);
    CHECK(fs_material_instance_destroy(engine, litInstance) == FS_OK);
    CHECK(fs_material_destroy(engine, litMaterial) == FS_OK);
    CHECK(fs_renderable_destroy(engine, renderable) == FS_OK);
    CHECK(fs_light_destroy(engine, light) == FS_OK);
    CHECK(fs_material_instance_destroy(engine, instance) == FS_OK);
    CHECK(fs_material_destroy(engine, material) == FS_OK);
    CHECK(fs_texture_destroy(engine, texture) == FS_OK);
    CHECK(fs_vertex_buffer_destroy(engine, vb) == FS_OK);
    CHECK(fs_index_buffer_destroy(engine, ib) == FS_OK);
    CHECK(fs_scene_destroy(engine, scene) == FS_OK);
    fs_entity_destroy(mesh);
    fs_entity_destroy(lightEntity);

    // Engine destroy with leaked children must not crash - including the
    // hard case: a leaked material WITH a live instance (Filament's material
    // destructor terminates the process when instances outlive it, so the
    // bridge's engine teardown must destroy instances first) plus a leaked
    // renderable, texture, scene, and light for full coverage.
    fs_material_t* leaked = fs_material_create(engine, package.data(), package.size());
    CHECK(leaked != nullptr);
    fs_material_instance_t* leakedInstance =
            fs_material_create_instance(engine, leaked);
    CHECK(leakedInstance != nullptr);

    fs_texture_t* leakedTexture =
            fs_texture_create_2d(engine, 2, 2, 1, FS_TEXTURE_RGBA8);
    CHECK(leakedTexture != nullptr);

    // Fresh buffers for the leaked renderable (the earlier ones were
    // destroyed above).
    const fs_vertex_attribute_desc leakAttrs[] = {
        { FS_VERTEX_POSITION, FS_ELEMENT_FLOAT3, 0, 0, 12 }
    };
    fs_vertex_buffer_t* vb2 = fs_vertex_buffer_create(engine, 3, 1, leakAttrs, 1);
    CHECK(vb2 != nullptr);
    float leakTriangle[9] = { 0, 1, 0, -1, -1, 0, 1, -1, 0 };
    CHECK(fs_vertex_buffer_set_buffer(engine, vb2, 0,
            reinterpret_cast<const uint8_t*>(leakTriangle), sizeof(leakTriangle))
          == FS_OK);
    fs_index_buffer_t* ib2 = fs_index_buffer_create(engine, 3, FS_INDEX_USHORT);
    CHECK(ib2 != nullptr);
    uint16_t leakIndices[3] = { 0, 1, 2 };
    CHECK(fs_index_buffer_set_buffer(engine, ib2,
            reinterpret_cast<const uint8_t*>(leakIndices), sizeof(leakIndices))
          == FS_OK);

    fs_entity_id leakEntity = fs_entity_create();
    fs_renderable_desc leakDesc{};
    leakDesc.entity = leakEntity;
    leakDesc.vertices = vb2;
    leakDesc.indices = ib2;
    leakDesc.primitive_offset = 0;
    leakDesc.primitive_count = 3;
    leakDesc.bounds_min[0] = -1; leakDesc.bounds_min[1] = -1; leakDesc.bounds_min[2] = -1;
    leakDesc.bounds_max[0] = 1; leakDesc.bounds_max[1] = 1; leakDesc.bounds_max[2] = 1;
    leakDesc.material = leakedInstance;
    leakDesc.culling = 0;
    leakDesc.cast_shadows = 0;
    leakDesc.receive_shadows = 0;
    fs_renderable_t* leakedRenderable = fs_renderable_create(engine, &leakDesc);
    CHECK(leakedRenderable != nullptr);

    fs_scene_t* leakedScene = fs_scene_create(engine);
    CHECK(leakedScene != nullptr);
    CHECK(fs_scene_add_entity(engine, leakedScene, leakEntity) == FS_OK);

    fs_light_t* leakedLight = fs_light_create(engine, fs_entity_create(),
            FS_LIGHT_SUN, 1, 1, 1, 100000.0f, 1, 0.0, -1.0, 0.0, 0, 0, 0, 0, 0.0f, 0);
    CHECK(leakedLight != nullptr);

    fs_engine_destroy(engine); // must survive all of the above

    CHECK(fs_material_destroy(engine, leaked) == FS_OK); // invalidated no-op
    CHECK(fs_material_instance_destroy(engine, leakedInstance) == FS_OK);
    CHECK(fs_texture_destroy(engine, leakedTexture) == FS_OK);
    CHECK(fs_renderable_destroy(engine, leakedRenderable) == FS_OK);
    CHECK(fs_scene_destroy(engine, leakedScene) == FS_OK);
    CHECK(fs_light_destroy(engine, leakedLight) == FS_OK);

    // Destroying a material with a live instance returns an error (not a
    // crash). NOTE: Filament's own destroy path is interrupted by the
    // precondition, so the material is left in a half-destroyed state and a
    // retried destroy may fail again ("missing its material domain").
    // The guaranteed-safe reclamation is engine teardown (LIFO child pass),
    // exercised by fs_engine_destroy below.
    fs_engine_t* e2 = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(e2 != nullptr);
    fs_material_t* m2 = fs_material_create(e2, package.data(), package.size());
    CHECK(m2 != nullptr);
    fs_material_instance_t* mi2 = fs_material_create_instance(e2, m2);
    CHECK(mi2 != nullptr);
    CHECK(fs_material_destroy(e2, m2) != FS_OK);
    CHECK(fs_material_instance_destroy(e2, mi2) == FS_OK);
    fs_engine_destroy(e2); // reclaims the refused material; must not crash

    fs_entity_destroy(fs_entity_create()); // harmless

    if (g_failures > 0) {
        std::fprintf(stderr, "test_materials: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_materials: OK\n");
    return 0;
}
