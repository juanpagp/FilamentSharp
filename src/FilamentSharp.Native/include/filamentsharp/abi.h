// FilamentSharp native bridge - public C ABI.
//
// WINDOWED RENDERING NOTE: fs_swap_chain_create_native (rendering section)
// binds a swap chain to an OS window handle (X11 XID on Linux). The bridge
// itself performs no windowing: creating/mapping/resizing the window and
// pumping its events is the application's job (see the WindowedRendering
// sample and docs/windowed-rendering.md).
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
   - Children still alive at engine destruction are torn down safely: their
   Filament objects are destroyed in REVERSE creation order (dependents
   before providers - e.g. material instances before materials, which
   Filament requires or its material destructor aborts the process), then
   the engine itself is destroyed. Wrappers are freed last.
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

/* Creates a swap chain that presents into an operating-system window.

   native_window is a platform window handle. On Linux (the only platform
   supported by this revision) it is the X11 Window id (XID) of the window
   you created yourself — the same value XCreateSimpleWindow/XCreateWindow
   returned, reinterpreted as void*. Filament derives the surface from it:
     * Vulkan backend: the VkSurfaceKHR is created through XCB from the XID.
       The X server must be reachable through DISPLAY (a real display or
       Xvfb both work; see docs/windowed-rendering.md).
     * OpenGL backend: the XID is used as the GLX drawable; a running X
       server is mandatory (upstream Filament terminates the process when
       it is not — see the warning on fs_engine_create).

   Lifecycle on resize: destroy this swap chain and create a new one with
   the same native_window; Filament re-derives the surface extents from the
   window. Do this outside begin_frame/end_frame.

   Returns the handle, or NULL with the error text set when the engine is
   null/unknown/destroyed, when native_window is null (rejected by the
   bridge — Filament would otherwise hand the null pointer to the platform
   layer, which dereferences it), or when surface creation fails. */
FILAMENTSHARP_API fs_swap_chain_t* fs_swap_chain_create_native(
        fs_engine_t* engine, void* native_window, fs_swap_chain_flags_t flags);

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

/* ------------------------------------------------------------------ */
/* Geometry buffers                                                    */
/* ------------------------------------------------------------------ */

/* Vertex attribute identifiers; values mirror filament::VertexAttribute
   (uint8) at the pinned release. */
enum fs_vertex_attribute {
    FS_VERTEX_POSITION = 0,
    FS_VERTEX_TANGENTS = 1,
    FS_VERTEX_COLOR = 2,
    FS_VERTEX_UV0 = 3,
    FS_VERTEX_UV1 = 4,
    FS_VERTEX_BONE_INDICES = 5,
    FS_VERTEX_BONE_WEIGHTS = 6,
    FS_VERTEX_CUSTOM0 = 8,
    FS_VERTEX_CUSTOM1 = 9,
    FS_VERTEX_CUSTOM2 = 10,
    FS_VERTEX_CUSTOM3 = 11,
    FS_VERTEX_CUSTOM4 = 12,
    FS_VERTEX_CUSTOM5 = 13,
    FS_VERTEX_CUSTOM6 = 14,
    FS_VERTEX_CUSTOM7 = 15
};
typedef enum fs_vertex_attribute fs_vertex_attribute;

/* Vertex attribute element types; values mirror
   filament::backend::ElementType at the pinned release. */
enum fs_element_type {
    FS_ELEMENT_BYTE = 0, FS_ELEMENT_BYTE2 = 1, FS_ELEMENT_BYTE3 = 2,
    FS_ELEMENT_BYTE4 = 3, FS_ELEMENT_UBYTE = 4, FS_ELEMENT_UBYTE2 = 5,
    FS_ELEMENT_UBYTE3 = 6, FS_ELEMENT_UBYTE4 = 7, FS_ELEMENT_SHORT = 8,
    FS_ELEMENT_SHORT2 = 9, FS_ELEMENT_SHORT3 = 10, FS_ELEMENT_SHORT4 = 11,
    FS_ELEMENT_USHORT = 12, FS_ELEMENT_USHORT2 = 13, FS_ELEMENT_USHORT3 = 14,
    FS_ELEMENT_USHORT4 = 15, FS_ELEMENT_INT = 16, FS_ELEMENT_UINT = 17,
    FS_ELEMENT_FLOAT = 18, FS_ELEMENT_FLOAT2 = 19, FS_ELEMENT_FLOAT3 = 20,
    FS_ELEMENT_FLOAT4 = 21, FS_ELEMENT_HALF = 22, FS_ELEMENT_HALF2 = 23,
    FS_ELEMENT_HALF3 = 24, FS_ELEMENT_HALF4 = 25
};
typedef enum fs_element_type fs_element_type;

/* Index buffer element type for fs_index_buffer_create. */
enum fs_index_type {
    FS_INDEX_USHORT = 0, /* 16-bit indices */
    FS_INDEX_UINT = 1    /* 32-bit indices */
};
typedef enum fs_index_type fs_index_type;

/* Attribute declaration passed to fs_vertex_buffer_create.
   Layout contract: 20 bytes, natural alignment; verified by compile-time
   checks in the native tests. Managed code passes an array of these. */
typedef struct fs_vertex_attribute_desc {
    int32_t attribute;    /* fs_vertex_attribute */
    int32_t element_type; /* fs_element_type */
    uint32_t buffer_index;
    uint32_t byte_offset;
    uint32_t byte_stride;
} fs_vertex_attribute_desc;

typedef struct fs_vertex_buffer fs_vertex_buffer_t;
typedef struct fs_index_buffer fs_index_buffer_t;

/* Creates a vertex buffer. attributes points to attribute_count attribute
   declarations (at least one, POSITION is required by Filament for
   renderables); buffer_count is the number of separate data buffers the
   attributes may refer to. */
FILAMENTSHARP_API fs_vertex_buffer_t* fs_vertex_buffer_create(
        fs_engine_t* engine, uint32_t vertex_count, uint32_t buffer_count,
        const fs_vertex_attribute_desc* attributes, uint32_t attribute_count);

/* Uploads data for one of the buffer indices. The bridge copies the data;
   the caller's memory is not retained after the call returns. */
FILAMENTSHARP_API fs_status fs_vertex_buffer_set_buffer(fs_engine_t* engine,
        fs_vertex_buffer_t* vertex_buffer, uint32_t buffer_index,
        const uint8_t* data, uintptr_t size);

FILAMENTSHARP_API fs_status fs_vertex_buffer_destroy(fs_engine_t* engine,
        fs_vertex_buffer_t* vertex_buffer);

/* Creates an index buffer with 16-bit or 32-bit indices. */
FILAMENTSHARP_API fs_index_buffer_t* fs_index_buffer_create(fs_engine_t* engine,
        uint32_t index_count, fs_index_type index_type);

/* Uploads the index data (index_count * element_size bytes). The bridge
   copies the data. */
FILAMENTSHARP_API fs_status fs_index_buffer_set_buffer(fs_engine_t* engine,
        fs_index_buffer_t* index_buffer, const uint8_t* data, uintptr_t size);

FILAMENTSHARP_API fs_status fs_index_buffer_destroy(fs_engine_t* engine,
        fs_index_buffer_t* index_buffer);

/* ------------------------------------------------------------------ */
/* Transforms and scene entities                                       */
/* ------------------------------------------------------------------ */

/* Sets an entity's local transform from 16 column-major floats
   (Filament's memory layout). */
FILAMENTSHARP_API fs_status fs_transform_set_matrix(fs_engine_t* engine,
        fs_entity_id entity, const float* column_major16);

/* Writes the entity's world transform (parent-composed) into out_matrix16. */
FILAMENTSHARP_API fs_status fs_transform_get_world_matrix(fs_engine_t* engine,
        fs_entity_id entity, float* out_matrix16);

/* Parents child under parent (0 detaches the child from the hierarchy). */
FILAMENTSHARP_API fs_status fs_transform_set_parent(fs_engine_t* engine,
        fs_entity_id child, fs_entity_id parent);

FILAMENTSHARP_API fs_status fs_scene_add_entity(fs_engine_t* engine,
        fs_scene_t* scene, fs_entity_id entity);

FILAMENTSHARP_API fs_status fs_scene_remove_entity(fs_engine_t* engine,
        fs_scene_t* scene, fs_entity_id entity);

/* out_has receives 1/0. */
FILAMENTSHARP_API fs_status fs_scene_has_entity(fs_engine_t* engine,
        fs_scene_t* scene, fs_entity_id entity, int32_t* out_has);

/* out_count receives the number of entities currently in the scene. */
FILAMENTSHARP_API fs_status fs_scene_get_entity_count(fs_engine_t* engine,
        fs_scene_t* scene, uint32_t* out_count);

/* ------------------------------------------------------------------ */
/* Materials, textures, lights, renderables                            */
/* ------------------------------------------------------------------ */

typedef struct fs_material fs_material_t;
typedef struct fs_material_instance fs_material_instance_t;
typedef struct fs_texture fs_texture_t;
typedef struct fs_light fs_light_t;
typedef struct fs_renderable fs_renderable_t;

/* Texture internal formats (subset; values mirror
   filament::Texture::InternalFormat at the pinned release). */
enum fs_texture_internal_format {
    FS_TEXTURE_RGBA8 = 30,
    FS_TEXTURE_SRGB8_A8 = 31
};
typedef enum fs_texture_internal_format fs_texture_internal_format;

/* Sampler filters/wrap (values mirror backend::SamplerMinFilter /
 * SamplerMagFilter / SamplerWrapMode). */
enum fs_min_filter {
    FS_MIN_NEAREST = 0, FS_MIN_LINEAR = 1, FS_MIN_NEAREST_MIPMAP_NEAREST = 2,
    FS_MIN_LINEAR_MIPMAP_NEAREST = 3, FS_MIN_NEAREST_MIPMAP_LINEAR = 4,
    FS_MIN_LINEAR_MIPMAP_LINEAR = 5
};
enum fs_mag_filter { FS_MAG_NEAREST = 0, FS_MAG_LINEAR = 1 };
enum fs_wrap_mode {
    FS_WRAP_CLAMP_TO_EDGE = 0, FS_WRAP_REPEAT = 1, FS_WRAP_MIRRORED_REPEAT = 2
};

/* Light types (values mirror filament::LightManager::Type). */
enum fs_light_type {
    FS_LIGHT_SUN = 0, FS_LIGHT_DIRECTIONAL = 1, FS_LIGHT_POINT = 2,
    FS_LIGHT_FOCUSED_SPOT = 3, FS_LIGHT_SPOT = 4
};
typedef enum fs_light_type fs_light_type;

/* ---- Material ---------------------------------------------------------- */

/* Creates a material from a matc-compiled package (.filamat bytes).
   The bridge structurally pre-validates the package (chunk chain must
   consume exactly the whole payload, first chunk MAT_VERS): truncated or
   corrupt packages are rejected with an error, because Filament's own
   parser can read past the buffer end for such inputs. Well-formed
   packages that Filament still rejects (unsupported version, incompatible
   shader payload) surface as an error with Filament's diagnostic text. */
FILAMENTSHARP_API fs_material_t* fs_material_create(fs_engine_t* engine,
        const uint8_t* package_data, uintptr_t package_size);

/* Destroys a material. Returns FS_ERROR (without destroying anything) when
   material instances created from this material are still alive: Filament
   refuses the destruction (precondition) and the bridge maps that to an
   error return. Destroy instances first. */
FILAMENTSHARP_API fs_status fs_material_destroy(fs_engine_t* engine,
        fs_material_t* material);

/* Writes the number of declared material parameters into out_count. */
FILAMENTSHARP_API fs_status fs_material_get_parameter_count(fs_engine_t* engine,
        fs_material_t* material, uint32_t* out_count);

/* Writes up to capacity pointers to bridge-owned NUL-terminated parameter
   names into out_names. The pointers stay valid for the lifetime of the
   material. Sets *out_written to the number of written pointers. */
FILAMENTSHARP_API fs_status fs_material_get_parameter_names(fs_engine_t* engine,
        fs_material_t* material, const char** out_names, uint32_t capacity,
        uint32_t* out_written);

/* Creates a material instance (a specialization with its own parameter
   values) from a material. */
FILAMENTSHARP_API fs_material_instance_t* fs_material_create_instance(
        fs_engine_t* engine, fs_material_t* material);

FILAMENTSHARP_API fs_status fs_material_instance_destroy(fs_engine_t* engine,
        fs_material_instance_t* instance);

/* Typed parameter setters. name is a NUL-terminated UTF-8 parameter name.
   Setting a parameter the material does not declare returns FS_ERROR (the
   pinned Filament release raises a precondition panic for unknown uniform
   names, which the bridge's error model converts into a status + message
   instead of letting the exception cross the ABI). */
FILAMENTSHARP_API fs_status fs_material_instance_set_float(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name, float value);
FILAMENTSHARP_API fs_status fs_material_instance_set_float3(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name,
        float x, float y, float z);
FILAMENTSHARP_API fs_status fs_material_instance_set_float4(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name,
        float x, float y, float z, float w);
FILAMENTSHARP_API fs_status fs_material_instance_set_bool(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name, int32_t value);
FILAMENTSHARP_API fs_status fs_material_instance_set_int(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name, int32_t value);

/* Binds a texture parameter with the given sampler configuration. */
FILAMENTSHARP_API fs_status fs_material_instance_set_texture(fs_engine_t* engine,
        fs_material_instance_t* instance, const char* name,
        fs_texture_t* texture, int32_t min_filter, int32_t mag_filter,
        int32_t wrap_s, int32_t wrap_t);

/* ---- Texture ----------------------------------------------------------- */

/* Creates a 2D texture. levels is the number of mip levels (1 for a single
   image without mipmaps). */
FILAMENTSHARP_API fs_texture_t* fs_texture_create_2d(fs_engine_t* engine,
        uint32_t width, uint32_t height, uint32_t levels,
        fs_texture_internal_format format);

/* Uploads one mip level of RGBA8 data (width*height*4 bytes; expected to
   match the level size). The bridge copies the data. */
FILAMENTSHARP_API fs_status fs_texture_set_image_rgba8(fs_engine_t* engine,
        fs_texture_t* texture, uint32_t level, uint32_t width, uint32_t height,
        const uint8_t* rgba, uintptr_t size);

FILAMENTSHARP_API fs_status fs_texture_destroy(fs_engine_t* engine,
        fs_texture_t* texture);

/* ---- Light -------------------------------------------------------------- */

/* Creates a light component on an entity.
   has_direction/dir_*: required for SUN/DIRECTIONAL/SPOT types;
   has_position/pos_*: required for POINT/SPOT types;
   falloff > 0 enables a finite light radius;
   color is linear RGB; intensity is in lux (SUN/DIRECTIONAL) or candela
   (POINT/SPOT); cast_shadows is 0/1. */
FILAMENTSHARP_API fs_light_t* fs_light_create(fs_engine_t* engine,
        fs_entity_id entity, fs_light_type type,
        float r, float g, float b, float intensity,
        int32_t has_direction, double dir_x, double dir_y, double dir_z,
        int32_t has_position, double pos_x, double pos_y, double pos_z,
        float falloff, int32_t cast_shadows);

FILAMENTSHARP_API fs_status fs_light_destroy(fs_engine_t* engine,
        fs_light_t* light);

/* ---- Renderable --------------------------------------------------------- */

/* Renderable creation description (single primitive; multi-primitive
   renderables are not exposed by this ABI revision). Layout is verified by
   compile-time checks in the native tests. */
typedef struct fs_renderable_desc {
    fs_entity_id entity;
    fs_vertex_buffer_t* vertices;
    fs_index_buffer_t* indices;
    uint32_t primitive_offset; /* in indices */
    uint32_t primitive_count;
    float bounds_min[3];
    float bounds_max[3];
    fs_material_instance_t* material;
    int32_t culling;        /* 0/1 */
    int32_t cast_shadows;   /* 0/1 */
    int32_t receive_shadows;/* 0/1 */
} fs_renderable_desc;

/* Creates a renderable component on desc->entity. */
FILAMENTSHARP_API fs_renderable_t* fs_renderable_create(fs_engine_t* engine,
        const fs_renderable_desc* desc);

FILAMENTSHARP_API fs_status fs_renderable_destroy(fs_engine_t* engine,
        fs_renderable_t* renderable);

/* ------------------------------------------------------------------ */
/* glTF asset loading (gltfio)                                         */
/* ------------------------------------------------------------------ */

typedef struct fs_gltf_loader fs_gltf_loader_t;
typedef struct fs_gltf_asset fs_gltf_asset_t;

/* Creates a glTF asset loader bound to the engine. Materials come from the
   precompiled ubershader archive (no runtime shader generation); PNG/JPEG
   textures embedded in GLB files are decoded with the bundled stb provider.
   The loader owns a cache of Filament materials that is freed when the
   loader is destroyed. */
FILAMENTSHARP_API fs_gltf_loader_t* fs_gltf_loader_create(fs_engine_t* engine);

/* Destroys a loader and every asset it still owns (assets must not outlive
   their loader). As with other children, this is a no-op once the engine
   has been destroyed. */
FILAMENTSHARP_API fs_status fs_gltf_loader_destroy(fs_engine_t* engine,
        fs_gltf_loader_t* loader);

/* Parses glTF 2.0 content from memory (GLB with embedded buffers, or a
   JSON glTF whose buffers are embedded as data URIs) and loads its
   resources. The bridge performs no filesystem access: self-contained
   files only. Returns null with the error text set for invalid content or
   missing resource data. */
FILAMENTSHARP_API fs_gltf_asset_t* fs_gltf_loader_load(fs_engine_t* engine,
        fs_gltf_loader_t* loader, const uint8_t* content, uintptr_t size);

/* Destroys an asset: removes its entities' components and frees its
   Filament resources. Entities created by the asset become inert (they can
   be destroyed with fs_entity_destroy). */
FILAMENTSHARP_API fs_status fs_gltf_asset_destroy(fs_engine_t* engine,
        fs_gltf_asset_t* asset);

/* Writes the number of entities (transform hierarchy + renderables) into
   out_count. */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_entity_count(fs_engine_t* engine,
        fs_gltf_asset_t* asset, uint32_t* out_count);

/* Writes up to capacity entity ids (creation order) into out_entities and
   the number written into out_written. */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_entities(fs_engine_t* engine,
        fs_gltf_asset_t* asset, fs_entity_id* out_entities, uint32_t capacity,
        uint32_t* out_written);

/* Writes the asset's scene-graph root entity into out_root. */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_root(fs_engine_t* engine,
        fs_gltf_asset_t* asset, fs_entity_id* out_root);

/* Writes the number of renderable entities into out_count. */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_renderable_count(fs_engine_t* engine,
        fs_gltf_asset_t* asset, uint32_t* out_count);

/* Writes the asset's axis-aligned bounding box into out_min/out_max
   (3 floats each, XYZ). */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_bounding_box(fs_engine_t* engine,
        fs_gltf_asset_t* asset, float* out_min, float* out_max);

/* Writes the number of animations into out_count. */
FILAMENTSHARP_API fs_status fs_gltf_asset_get_animation_count(fs_engine_t* engine,
        fs_gltf_asset_t* asset, uint32_t* out_count);

/* Applies animation `animation_index` at `time_seconds` and updates bone
   matrices. Follows gltfio's Animator contract (values outside the
   animation duration are clamped/looped by gltfio). */
FILAMENTSHARP_API fs_status fs_gltf_asset_apply_animation(fs_engine_t* engine,
        fs_gltf_asset_t* asset, uint32_t animation_index, double time_seconds);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILAMENTSHARP_ABI_H */
