// glTF loading contract tests (NOOP backend; real gltfio + ubershader
// materials + embedded resources; triangle.glb from scripts/gen-test-gltf.py).
#include <cstdio>
#include <cstring>
#include <vector>

#include "filamentsharp/abi.h"

#ifndef FS_GLTF_MODEL
#  error "FS_GLTF_MODEL must be defined by the build"
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
    std::vector<uint8_t> glb = readFile(FS_GLTF_MODEL);
    CHECK(!glb.empty());
    if (glb.empty()) {
        std::fprintf(stderr, "test_gltf: missing model; run scripts/gen-test-gltf.py\n");
        return 1;
    }

    fs_engine_t* engine = fs_engine_create(FS_BACKEND_NOOP);
    CHECK(engine != nullptr);
    if (engine == nullptr) {
        return 1;
    }

    // ---- Loader ------------------------------------------------------------
    fs_gltf_loader_t* loader = fs_gltf_loader_create(engine);
    CHECK(loader != nullptr);
    CHECK(fs_gltf_loader_create(nullptr) == nullptr);

    // ---- Load + queries ----------------------------------------------------
    fs_gltf_asset_t* asset = fs_gltf_loader_load(engine, loader, glb.data(), glb.size());
    CHECK(asset != nullptr);
    if (asset == nullptr) {
        std::fprintf(stderr, "test_gltf: load failed: %s\n", fs_get_last_error());
        return 1;
    }

    uint32_t entityCount = 0;
    CHECK(fs_gltf_asset_get_entity_count(engine, asset, &entityCount) == FS_OK);
    // One glTF node -> at least one entity (the node); gltfio may or may not
    // add a separate scene-root entity.
    CHECK(entityCount >= 1);

    std::vector<fs_entity_id> entities(entityCount);
    uint32_t written = 0;
    CHECK(fs_gltf_asset_get_entities(engine, asset, entities.data(), entityCount,
            &written) == FS_OK);
    CHECK(written == entityCount);
    for (fs_entity_id e : entities) {
        CHECK(e != 0);
    }
    // Capacity smaller than the count: writes only capacity, still FS_OK.
    fs_entity_id few[1] = {};
    CHECK(fs_gltf_asset_get_entities(engine, asset, few, 1, &written) == FS_OK);
    CHECK(written == 1);

    fs_entity_id root = 0;
    CHECK(fs_gltf_asset_get_root(engine, asset, &root) == FS_OK);
    CHECK(root != 0);

    uint32_t renderableCount = 0;
    CHECK(fs_gltf_asset_get_renderable_count(engine, asset, &renderableCount) == FS_OK);
    CHECK(renderableCount == 1); // the triangle mesh node

    float min[3] = { 0 }, max[3] = { 0 };
    CHECK(fs_gltf_asset_get_bounding_box(engine, asset, min, max) == FS_OK);
    // The generated triangle spans (-1,-1,0)..(3,3,0).
    CHECK(min[0] > -1.01f && min[0] < -0.99f);
    CHECK(min[1] > -1.01f && min[1] < -0.99f);
    CHECK(max[0] > 2.99f && max[0] < 3.01f);
    CHECK(max[1] > 2.99f && max[1] < 3.01f);

    uint32_t animationCount = 99;
    CHECK(fs_gltf_asset_get_animation_count(engine, asset, &animationCount) == FS_OK);
    CHECK(animationCount == 0); // the generated model has no animations
    CHECK(fs_gltf_asset_apply_animation(engine, asset, 0, 0.0) == FS_ERROR_INVALID_ARG);

    // Invalid content is rejected, not fatal.
    const uint8_t junk[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    CHECK(fs_gltf_loader_load(engine, loader, junk, sizeof(junk)) == nullptr);
    CHECK(fs_gltf_loader_load(engine, loader, nullptr, 0) == nullptr);

    // Handle validation.
    CHECK(fs_gltf_loader_load(nullptr, loader, glb.data(), glb.size()) == nullptr);
    CHECK(fs_gltf_asset_get_entity_count(nullptr, asset, &entityCount)
          == FS_ERROR_INVALID_ARG);
    CHECK(fs_gltf_asset_get_entity_count(engine, nullptr, &entityCount)
          == FS_ERROR_INVALID_ARG);

    // ---- Teardown: explicit asset destroy, then loader ----------------------
    CHECK(fs_gltf_asset_destroy(engine, asset) == FS_OK);
    CHECK(fs_gltf_asset_destroy(engine, asset) == FS_ERROR_INVALID_ARG); // unknown now

    // A second asset destroyed implicitly by loader destroy.
    fs_gltf_asset_t* asset2 = fs_gltf_loader_load(engine, loader, glb.data(), glb.size());
    CHECK(asset2 != nullptr);
    CHECK(fs_gltf_loader_destroy(engine, loader) == FS_OK);
    CHECK(fs_gltf_asset_destroy(engine, asset2) == FS_ERROR_INVALID_ARG);

    // ---- Engine teardown with leaked loader + asset (LIFO) ------------------
    fs_gltf_loader_t* leakedLoader = fs_gltf_loader_create(engine);
    CHECK(leakedLoader != nullptr);
    fs_gltf_asset_t* leakedAsset =
            fs_gltf_loader_load(engine, leakedLoader, glb.data(), glb.size());
    CHECK(leakedAsset != nullptr);
    fs_engine_destroy(engine); // must survive leaked loader + asset

    if (g_failures > 0) {
        std::fprintf(stderr, "test_gltf: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_gltf: OK\n");
    return 0;
}
