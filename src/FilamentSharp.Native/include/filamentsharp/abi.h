// FilamentSharp native bridge - public C ABI.
//
// This header is the single source of truth for the boundary between the
// managed FilamentSharp library and the native bridge. It must remain valid
// ISO C (compile it with a C compiler - tests/native/test_abi_c.c does exactly
// that) and must never expose C++ types, templates, exceptions, or STL types.
//
// Conventions (apply to every fs_* function added in later revisions):
//
//   * All symbols are extern "C" with default visibility; everything else in
//     the shared library is hidden. Exported names are stable and prefixed
//     `fs_`.
//   * All integers crossing the boundary are fixed-width (stdint.h).
//   * Handles are opaque pointers to incomplete struct types; one distinct
//     struct tag per resource type. Never dereference them from managed code.
//   * Functions returning handles return NULL on failure and set the
//     thread-local error state (fs_get_last_error).
//   * Functions returning fs_status return FS_OK on success and an error code
//     plus thread-local error text on failure.
//   * No C++ exception ever crosses this boundary; every entry point is
//     wrapped in a catch-all that converts exceptions into FS_ERROR + error
//     text.
//   * Strings are NUL-terminated UTF-8. Strings owned by the bridge remain
//     valid until the next bridge call on the same thread.
//   * Buffer parameters are always accompanied by explicit element/byte
//     lengths. Callers own all input memory.
//   * Threading: Filament Engine objects are not thread-safe. All fs_*
//     functions must be externally synchronized by the caller (typically:
//     called from a single rendering thread). The bridge itself adds no
//     internal locking except for the engine ownership registry, which is
//     internally synchronized.
//
// ABI version history:
//   1 - initial release: versioning, error model.
#ifndef FILAMENTSHARP_ABI_H
#define FILAMENTSHARP_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version of this C ABI. Managed code refuses to load a bridge with a
   different value. Bump on any breaking change and record it above. */
#define FILAMENTSHARP_ABI_VERSION 1

/* Symbol visibility (dllexport/dllimport on Windows, default visibility
   elsewhere). */
#if defined(_WIN32)
#  if defined(FILAMENTSHARP_BUILDING)
#    define FILAMENTSHARP_API __declspec(dllexport)
#  else
#    define FILAMENTSHARP_API __declspec(dllimport)
#  endif
#else
#  define FILAMENTSHARP_API __attribute__((visibility("default")))
#endif

/* Status codes returned by fs_* functions. */
enum fs_status {
    FS_OK = 0,              /* success */
    FS_ERROR = 1,           /* generic failure; retrieve details via fs_get_last_error */
    FS_ERROR_INVALID_ARG = 2, /* a caller-supplied argument was null/invalid */
    FS_ERROR_UNSUPPORTED = 3, /* the request is not supported in this build */
    FS_ERROR_INVALIDATED = 4, /* the owning engine has already been destroyed */
    FS_ERROR_NOT_READY = 5    /* the operation is still pending; retry later */
};
typedef enum fs_status fs_status;

/* Rendering backend selection. Values mirror filament::backend::Backend at
   the pinned release; managed code marshals them as int32. */
enum fs_backend {
    FS_BACKEND_DEFAULT = 0,
    FS_BACKEND_OPENGL = 1,
    FS_BACKEND_VULKAN = 2,
    FS_BACKEND_METAL = 3,
    FS_BACKEND_WEBGPU = 4,
    FS_BACKEND_NOOP = 5
};
typedef enum fs_backend fs_backend;

/* Opaque engine handle. Obtained from fs_engine_create, released with
   fs_engine_destroy. All other resources are owned by an engine. */
typedef struct fs_engine fs_engine_t;

/* ------------------------------------------------------------------ */
/* Versioning                                                          */
/* ------------------------------------------------------------------ */

/* Returns FILAMENTSHARP_ABI_VERSION of this bridge. */
FILAMENTSHARP_API uint32_t fs_abi_version(void);

/* Returns the pinned Google Filament release version this bridge was built
   against (e.g. "v1.78.0"). Never returns NULL. */
FILAMENTSHARP_API const char* fs_filament_version(void);

/* Returns the pinned upstream tag commit (hex SHA-1) of the Filament release
   this bridge was built against. Never returns NULL. */
FILAMENTSHARP_API const char* fs_filament_commit(void);

/* ------------------------------------------------------------------ */
/* Error model                                                         */
/* ------------------------------------------------------------------ */

/* Returns a NUL-terminated UTF-8 description of the most recent error on the
   calling thread, or "" when the last bridge call succeeded. Never returns
   NULL. The string is owned by the bridge and remains valid until the next
   bridge call on the same thread. */
FILAMENTSHARP_API const char* fs_get_last_error(void);

/* Clears the calling thread's error state. */
FILAMENTSHARP_API void fs_clear_last_error(void);

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                    */
/* ------------------------------------------------------------------ */

/* Creates a Filament engine with the requested backend.

   backend: one of fs_backend; FS_BACKEND_DEFAULT lets Filament choose
   (Vulkan on Linux at the pinned release).

   Returns an engine handle, or NULL on failure with the thread-local error
   text set. Failure to initialize a graphics backend (e.g. no Vulkan ICD)
   is reported as an ordinary error.

   WARNING (upstream behavior at the pinned release v1.78.0): requesting
   FS_BACKEND_OPENGL on Linux without a running X server terminates the
   process inside Filament ("Failed to open X display"); this cannot be
   converted into an error return by the bridge. Choose the backend
   deliberately. */
FILAMENTSHARP_API fs_engine_t* fs_engine_create(fs_backend backend);

/* Destroys an Filament engine and everything it owns.

   - Idempotent: destroying the same handle twice is a no-op.
   - After this call, every child resource handle owned by the engine is
   invalidated: destroying a child becomes a documented no-op (FS_OK), and
   any other child operation returns FS_ERROR_INVALIDATED instead of
   touching freed memory.
   - The engine wrapper itself is retained internally (not freed) so late
   child releases can detect invalidation safely; only the underlying
   Filament engine and its resources are destroyed.
   - Internally synchronized; safe to call from a finalizer thread. */
FILAMENTSHARP_API void fs_engine_destroy(fs_engine_t* engine);

/* Returns the backend actually in use by this engine, or FS_BACKEND_DEFAULT
   (0) with the error text set when the handle is invalid. */
FILAMENTSHARP_API fs_backend fs_engine_get_backend(fs_engine_t* engine);

/* ------------------------------------------------------------------ */
/* Entities                                                            */
/* ------------------------------------------------------------------ */

/* Opaque entity identifier: a 32-bit id from Filament's global
   EntityManager, valid process-wide. 0 is the null entity. */
typedef uint32_t fs_entity_id;

/* Creates a new entity id. Cannot fail under normal conditions. */
FILAMENTSHARP_API fs_entity_id fs_entity_create(void);

/* Destroys an entity id, removing all Filament components from it. */
FILAMENTSHARP_API void fs_entity_destroy(fs_entity_id entity);

/* ------------------------------------------------------------------ */
/* Rendering infrastructure: renderer, swap chain, view, scene, camera */
/* ------------------------------------------------------------------ */

/* Opaque handles for engine-owned rendering resources. Destroy order:
   children first, engine last (the engine destroy path also releases
   anything left over, but explicit destruction is the documented contract). */
typedef struct fs_renderer fs_renderer_t;
typedef struct fs_swap_chain fs_swap_chain_t;
typedef struct fs_view fs_view_t;
typedef struct fs_scene fs_scene_t;
typedef struct fs_camera fs_camera_t;

/* Swap chain configuration flags (bitmask); values mirror
   backend::SWAP_CHAIN_CONFIG_* at the pinned release. */
enum fs_swap_chain_flags {
    FS_SWAP_CHAIN_CONFIG_TRANSPARENT = 0x1,
    FS_SWAP_CHAIN_CONFIG_READABLE = 0x2,
    FS_SWAP_CHAIN_CONFIG_ENABLE_XCB = 0x4,
    FS_SWAP_CHAIN_CONFIG_APPLE_CVPIXELBUFFER = 0x8,
    FS_SWAP_CHAIN_CONFIG_SRGB_COLORSPACE = 0x10,
    FS_SWAP_CHAIN_CONFIG_HAS_STENCIL_BUFFER = 0x20,
    FS_SWAP_CHAIN_CONFIG_PROTECTED_CONTENT = 0x40,
    FS_SWAP_CHAIN_CONFIG_MSAA_4_SAMPLES = 0x80
};
typedef uint64_t fs_swap_chain_flags_t;

/* Camera field-of-view direction. */
enum fs_fov_direction {
    FS_FOV_VERTICAL = 0,
    FS_FOV_HORIZONTAL = 1
};

/* ---- Renderer --------------------------------------------------------- */

/* Creates a renderer. NULL on failure (engine null/destroyed). */
FILAMENTSHARP_API fs_renderer_t* fs_renderer_create(fs_engine_t* engine);

/* Destroys a renderer. No-op when the engine has already been destroyed
   (FS_OK). Invalid handles return FS_ERROR_INVALID_ARG. */
FILAMENTSHARP_API fs_status fs_renderer_destroy(fs_engine_t* engine, fs_renderer_t* renderer);

/* Begins a frame on the given swap chain.
   out_began receives 1 when the frame was acquired (call render/end_frame)
   or 0 when the frame was skipped (e.g. vsync backpressure).
   frame_time_ns is the target vsync timestamp in nanoseconds (0 = now). */
FILAMENTSHARP_API fs_status fs_renderer_begin_frame(fs_engine_t* engine,
        fs_renderer_t* renderer, fs_swap_chain_t* swap_chain,
        uint64_t frame_time_ns, int32_t* out_began);

/* Renders a view into the current frame (between begin_frame and end_frame). */
FILAMENTSHARP_API fs_status fs_renderer_render(fs_engine_t* engine,
        fs_renderer_t* renderer, fs_view_t* view);

/* Ends the current frame and presents it. */
FILAMENTSHARP_API fs_status fs_renderer_end_frame(fs_engine_t* engine,
        fs_renderer_t* renderer);

/* Sets the clear options used at the start of each frame. The clear color is
   given in linear RGBA by value; clear/discard are 0/1. */
FILAMENTSHARP_API fs_status fs_renderer_set_clear_options(fs_engine_t* engine,
        fs_renderer_t* renderer, float r, float g, float b, float a,
        int32_t clear, int32_t discard);

/* Queues an asynchronous RGBA8 readback of the current swap chain region.
   Call between render and end_frame. Only one readback may be pending per
   renderer; retrieve the result with fs_renderer_read_pixels_complete. */
FILAMENTSHARP_API fs_status fs_renderer_read_pixels(fs_engine_t* engine,
        fs_renderer_t* renderer, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height);

/* Completes the pending readback: flushes the engine and waits, then copies
   out_rgba (width*height*4 bytes; out_capacity must be at least that).
   Sets *out_bytes_written. Returns FS_ERROR_NOT_READY when the readback has
   not completed yet (retry after another frame). On success the pending
   readback is released. */
FILAMENTSHARP_API fs_status fs_renderer_read_pixels_complete(fs_engine_t* engine,
        fs_renderer_t* renderer, uint8_t* out_rgba, uintptr_t out_capacity,
        uintptr_t* out_bytes_written);

/* ---- Swap chain -------------------------------------------------------- */

/* Creates a headless (offscreen) swap chain of the given size. This is the
   platform-independent surface: no window system handle is involved. */
FILAMENTSHARP_API fs_swap_chain_t* fs_swap_chain_create_headless(
        fs_engine_t* engine, uint32_t width, uint32_t height,
        fs_swap_chain_flags_t flags);

/* Destroys a swap chain (same semantics as fs_renderer_destroy). */
FILAMENTSHARP_API fs_status fs_swap_chain_destroy(fs_engine_t* engine,
        fs_swap_chain_t* swap_chain);

/* ---- View -------------------------------------------------------------- */

/* Creates a view. */
FILAMENTSHARP_API fs_view_t* fs_view_create(fs_engine_t* engine);

/* Destroys a view (same semantics as fs_renderer_destroy). */
FILAMENTSHARP_API fs_status fs_view_destroy(fs_engine_t* engine, fs_view_t* view);

/* Sets the view's scene (scene may be NULL to detach). */
FILAMENTSHARP_API fs_status fs_view_set_scene(fs_engine_t* engine, fs_view_t* view,
        fs_scene_t* scene);

/* Sets the view's camera. */
FILAMENTSHARP_API fs_status fs_view_set_camera(fs_engine_t* engine, fs_view_t* view,
        fs_camera_t* camera);

/* Sets the view's viewport in pixels. */
FILAMENTSHARP_API fs_status fs_view_set_viewport(fs_engine_t* engine, fs_view_t* view,
        int32_t left, int32_t bottom, uint32_t width, uint32_t height);

/* ---- Scene ------------------------------------------------------------- */

/* Creates a scene. */
FILAMENTSHARP_API fs_scene_t* fs_scene_create(fs_engine_t* engine);

/* Destroys a scene (same semantics as fs_renderer_destroy). */
FILAMENTSHARP_API fs_status fs_scene_destroy(fs_engine_t* engine, fs_scene_t* scene);

/* ---- Camera ------------------------------------------------------------ */

/* Creates a camera component on the given entity (must be non-zero).
   Destroy with fs_camera_destroy (or fs_entity_destroy for the entity). */
FILAMENTSHARP_API fs_camera_t* fs_camera_create(fs_engine_t* engine,
        fs_entity_id entity);

/* Destroys a camera (removes the camera component from its entity). */
FILAMENTSHARP_API fs_status fs_camera_destroy(fs_engine_t* engine,
        fs_camera_t* camera);

/* Sets a perspective projection. fov_degrees > 0, aspect > 0, far > near > 0. */
FILAMENTSHARP_API fs_status fs_camera_set_projection(fs_engine_t* engine,
        fs_camera_t* camera, double fov_degrees, double aspect,
        double near_plane, double far_plane, int32_t fov_direction);

/* Orients the camera: eye/center/up point to 3 doubles each. */
FILAMENTSHARP_API fs_status fs_camera_look_at(fs_engine_t* engine,
        fs_camera_t* camera, const double* eye, const double* center,
        const double* up);

/* Writes the camera's 4x4 projection matrix into out_matrix (16 floats,
   column-major, Filament's memory layout). */
FILAMENTSHARP_API fs_status fs_camera_get_projection_matrix(fs_engine_t* engine,
        fs_camera_t* camera, float* out_matrix);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILAMENTSHARP_ABI_H */
