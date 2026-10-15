// ABI portability test: this file is compiled as *plain C* against the public
// header, proving the header exposes no C++ constructs. At runtime it dlopens
// the built bridge and resolves every documented symbol by name, verifying
// the exported-symbol contract independently of the linker.
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "filamentsharp/abi.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

int main(void) {
    /* --- Compile-time C compatibility of the header ------------------- */
    _Static_assert(sizeof(enum fs_status) == sizeof(int),
                   "fs_status must have int representation");
    CHECK(FILAMENTSHARP_ABI_VERSION > 0);

    /* --- Load the bridge ---------------------------------------------- */
    const char* libPath = FILAMENTSHARP_TEST_LIBRARY;
    void* handle = dlopen(libPath, RTLD_NOW | RTLD_LOCAL);
    CHECK(handle != NULL);
    if (handle == NULL) {
        fprintf(stderr, "dlopen(%s) failed: %s\n", libPath, dlerror());
        return 1;
    }

    /* --- Resolve and validate every documented symbol ------------------ */
    uint32_t (*abi_version)(void) = (uint32_t (*)(void))dlsym(handle, "fs_abi_version");
    const char* (*filament_version)(void) = (const char* (*)(void))dlsym(handle, "fs_filament_version");
    const char* (*filament_commit)(void) = (const char* (*)(void))dlsym(handle, "fs_filament_commit");
    const char* (*get_last_error)(void) = (const char* (*)(void))dlsym(handle, "fs_get_last_error");
    void (*clear_last_error)(void) = (void (*)(void))dlsym(handle, "fs_clear_last_error");
    fs_engine_t* (*engine_create)(fs_backend) = (fs_engine_t * (*)(fs_backend))dlsym(handle, "fs_engine_create");
    void (*engine_destroy)(fs_engine_t*) = (void (*)(fs_engine_t*))dlsym(handle, "fs_engine_destroy");
    fs_backend (*engine_get_backend)(fs_engine_t*) = (fs_backend (*)(fs_engine_t*))dlsym(handle, "fs_engine_get_backend");
    fs_swap_chain_t* (*swap_chain_create_native)(fs_engine_t*, void*, fs_swap_chain_flags_t) =
        (fs_swap_chain_t * (*)(fs_engine_t*, void*, fs_swap_chain_flags_t))dlsym(handle, "fs_swap_chain_create_native");

    CHECK(abi_version != NULL);
    CHECK(filament_version != NULL);
    CHECK(filament_commit != NULL);
    CHECK(get_last_error != NULL);
    CHECK(clear_last_error != NULL);
    CHECK(engine_create != NULL);
    CHECK(engine_destroy != NULL);
    CHECK(engine_get_backend != NULL);
    CHECK(swap_chain_create_native != NULL);
    if (g_failures > 0) {
        fprintf(stderr, "symbol resolution failed\n");
        dlclose(handle);
        return 1;
    }

    CHECK(abi_version() == (uint32_t)FILAMENTSHARP_ABI_VERSION);

    const char* version = filament_version();
    CHECK(version != NULL);
    CHECK(strcmp(version, FS_EXPECTED_FILAMENT_VERSION) == 0);

    const char* commit = filament_commit();
    CHECK(commit != NULL);
    CHECK(strlen(commit) == 40); /* hex SHA-1 */

    const char* error = get_last_error();
    CHECK(error != NULL);

    clear_last_error();
    CHECK(strcmp(get_last_error(), "") == 0);

    /* Engine entry points callable through the C ABI (full lifecycle tests
       live in test_engine, which links the bridge directly). */
    engine_destroy(NULL);
    fs_engine_t* noop = engine_create(FS_BACKEND_NOOP);
    CHECK(noop != NULL);
    if (noop != NULL) {
        CHECK(engine_get_backend(noop) == FS_BACKEND_NOOP);
        /* Native-window swap chain argument validation through the C ABI
           (the full windowed lifecycle needs a display; that is exercised by
           the WindowedRendering sample and covered in test_rendering on the
           NOOP backend). */
        CHECK(swap_chain_create_native(NULL, (void*)0x1, 0) == NULL);
        CHECK(swap_chain_create_native(noop, NULL, 0) == NULL);
        CHECK(strcmp(get_last_error(), "") != 0);
        clear_last_error();
        engine_destroy(noop);
        engine_destroy(noop); /* idempotent */
    }

    dlclose(handle);

    if (g_failures > 0) {
        fprintf(stderr, "test_abi_c: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_abi_c: OK (header is valid C; %d symbols resolved and validated)\n", 9);
    return 0;
}
