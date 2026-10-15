// Scene construction contract tests: geometry buffers, transforms, and scene
// entity membership. Runs on NOOP (always available).
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

int main() {
    // ---- Compile-time layout checks for the shared struct ----------------
    static_assert(sizeof(fs_vertex_attribute_desc) == 20,
                  "fs_vertex_attribute_desc must be 20 bytes");
    static_assert(offsetof(fs_vertex_attribute_desc, attribute) == 0, "layout");
    static_assert(offsetof(fs_vertex_attribute_desc, element_type) == 4, "layout");
    static_assert(offsetof(fs_vertex_attribute_desc, buffer_index) == 8, "layout");
    static_assert(offsetof(fs_vertex_attribute_desc, byte_offset) == 12, "layout");
    static_assert(offsetof(fs_vertex_attribute_desc, byte_stride) == 16, "layout");
    static_assert(FS_ELEMENT_FLOAT3 == 20 && FS_ELEMENT_FLOAT2 == 19,
                  "element type values are ABI");

    fs_engine_t* engine = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(engine != nullptr);
    if (engine == nullptr) {
        std::fprintf(stderr, "test_scene: cannot continue without engine\n");
        return 1;
    }

    // ---- Vertex buffer -----------------------------------------------------
    const fs_vertex_attribute_desc positionAttribute[] = {
        { FS_VERTEX_POSITION, FS_ELEMENT_FLOAT3, 0, 0, 12 },
    };
    fs_vertex_buffer_t* vb = fs_vertex_buffer_create(engine, 3, 1,
            positionAttribute, 1);
    CHECK(vb != nullptr);

    // Validation: zero counts, missing attributes, bad attribute values,
    // out-of-range buffer index.
    CHECK(fs_vertex_buffer_create(engine, 0, 1, positionAttribute, 1) == nullptr);
    CHECK(fs_vertex_buffer_create(engine, 3, 0, positionAttribute, 1) == nullptr);
    CHECK(fs_vertex_buffer_create(engine, 3, 1, nullptr, 1) == nullptr);
    CHECK(fs_vertex_buffer_create(engine, 3, 1, positionAttribute, 0) == nullptr);
    const fs_vertex_attribute_desc badAttribute[] = { { 99, FS_ELEMENT_FLOAT3, 0, 0, 12 } };
    CHECK(fs_vertex_buffer_create(engine, 3, 1, badAttribute, 1) == nullptr);
    const fs_vertex_attribute_desc badElementType[] = { { FS_VERTEX_POSITION, 99, 0, 0, 12 } };
    CHECK(fs_vertex_buffer_create(engine, 3, 1, badElementType, 1) == nullptr);
    const fs_vertex_attribute_desc badBufferIndex[] = { { FS_VERTEX_POSITION, FS_ELEMENT_FLOAT3, 5, 0, 12 } };
    CHECK(fs_vertex_buffer_create(engine, 3, 1, badBufferIndex, 1) == nullptr);

    float triangle[9] = { 0, 1, 0, -1, -1, 0, 1, -1, 0 };
    CHECK(fs_vertex_buffer_set_buffer(engine, vb, 0,
            reinterpret_cast<const uint8_t*>(triangle), sizeof(triangle)) == FS_OK);
    CHECK(fs_vertex_buffer_set_buffer(engine, vb, 0, nullptr, 4) == FS_ERROR_INVALID_ARG);
    CHECK(fs_vertex_buffer_set_buffer(engine, vb, 0,
            reinterpret_cast<const uint8_t*>(triangle), 0) == FS_ERROR_INVALID_ARG);

    // ---- Index buffer ------------------------------------------------------
    fs_index_buffer_t* ib = fs_index_buffer_create(engine, 3, FS_INDEX_USHORT);
    CHECK(ib != nullptr);
    uint16_t indices[3] = { 0, 1, 2 };
    CHECK(fs_index_buffer_set_buffer(engine, ib,
            reinterpret_cast<const uint8_t*>(indices), sizeof(indices)) == FS_OK);
    CHECK(fs_index_buffer_create(engine, 0, FS_INDEX_USHORT) == nullptr);

    fs_index_buffer_t* ib32 = fs_index_buffer_create(engine, 3, FS_INDEX_UINT);
    CHECK(ib32 != nullptr);
    uint32_t indices32[3] = { 0, 1, 2 };
    CHECK(fs_index_buffer_set_buffer(engine, ib32,
            reinterpret_cast<const uint8_t*>(indices32), sizeof(indices32)) == FS_OK);

    // ---- Transforms and scene membership -----------------------------------
    fs_scene_t* scene = fs_scene_create(engine);
    CHECK(scene != nullptr);
    uint32_t count = 0;
    CHECK(fs_scene_get_entity_count(engine, scene, &count) == FS_OK);
    CHECK(count == 0);

    fs_entity_id e1 = fs_entity_create();
    fs_entity_id e2 = fs_entity_create();
    CHECK(e1 != 0 && e2 != 0);

    // Adding to a scene creates the transform component.
    CHECK(fs_scene_add_entity(engine, scene, e1) == FS_OK);
    CHECK(fs_scene_add_entity(engine, scene, e2) == FS_OK);
    CHECK(fs_scene_has_entity(engine, scene, e1, nullptr) == FS_ERROR_INVALID_ARG);
    int32_t has = 0;
    CHECK(fs_scene_has_entity(engine, scene, e1, &has) == FS_OK);
    CHECK(has == 1);
    CHECK(fs_scene_get_entity_count(engine, scene, &count) == FS_OK);
    CHECK(count == 2);

    CHECK(fs_scene_remove_entity(engine, scene, e1) == FS_OK);
    CHECK(fs_scene_has_entity(engine, scene, e1, &has) == FS_OK);
    CHECK(has == 0);
    CHECK(fs_scene_get_entity_count(engine, scene, &count) == FS_OK);
    CHECK(count == 1);

    // Set a translation on e2 and read back the world matrix.
    float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    CHECK(fs_transform_set_matrix(engine, e2, identity) == FS_OK);
    float world[16];
    CHECK(fs_transform_get_world_matrix(engine, e2, world) == FS_OK);
    CHECK(std::memcmp(world, identity, sizeof(identity)) == 0);

    // Translate by (2,0,0): column-major element [12].
    float translated[16];
    std::memcpy(translated, identity, sizeof(identity));
    translated[12] = 2.0f;
    CHECK(fs_transform_set_matrix(engine, e2, translated) == FS_OK);
    CHECK(fs_transform_get_world_matrix(engine, e2, world) == FS_OK);
    CHECK(std::memcmp(world, translated, sizeof(translated)) == 0);

    // Parent/child composition: parent e1 under e2 (both need transforms).
    CHECK(fs_scene_add_entity(engine, scene, e1) == FS_OK);
    CHECK(fs_transform_set_matrix(engine, e1, identity) == FS_OK);
    CHECK(fs_transform_set_parent(engine, e1, e2) == FS_OK);
    CHECK(fs_transform_get_world_matrix(engine, e1, world) == FS_OK);
    CHECK(world[12] == 2.0f); // child inherits parent translation

    // Detach: parent 0.
    CHECK(fs_transform_set_parent(engine, e1, 0) == FS_OK);
    CHECK(fs_transform_get_world_matrix(engine, e1, world) == FS_OK);
    CHECK(world[12] == 0.0f);

    // SetTransform auto-creates the transform component for fresh entities.
    fs_entity_id fresh = fs_entity_create();
    CHECK(fs_transform_set_matrix(engine, fresh, identity) == FS_OK);
    CHECK(fs_transform_get_world_matrix(engine, fresh, world) == FS_OK);
    CHECK(std::memcmp(world, identity, sizeof(identity)) == 0);
    CHECK(fs_transform_set_matrix(engine, 0, identity) == FS_ERROR_INVALID_ARG);

    // Invalidated after engine destroy.
    fs_engine_destroy(engine);
    CHECK(fs_scene_add_entity(engine, scene, e1) == FS_ERROR_INVALIDATED);
    CHECK(fs_vertex_buffer_set_buffer(engine, vb, 0,
            reinterpret_cast<const uint8_t*>(triangle), sizeof(triangle))
          == FS_ERROR_INVALIDATED);
    CHECK(fs_scene_destroy(engine, scene) == FS_OK); // no-op
    CHECK(fs_vertex_buffer_destroy(engine, vb) == FS_OK);
    CHECK(fs_index_buffer_destroy(engine, ib) == FS_OK);
    CHECK(fs_index_buffer_destroy(engine, ib32) == FS_OK);

    fs_entity_destroy(e1);
    fs_entity_destroy(e2);
    fs_entity_destroy(fresh);

    if (g_failures > 0) {
        std::fprintf(stderr, "test_scene: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_scene: OK\n");
    return 0;
}
