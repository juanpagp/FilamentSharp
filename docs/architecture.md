# Architecture

FilamentSharp is a layered binding. From the application down to the GPU:

```
+-------------------------------------------------------------+
|  C# application (samples/BasicRendering, tests)             |
+-------------------------------------------------------------+
|  Managed FilamentSharp API                                   |
|  Engine, Renderer, SwapChain, View, Scene, Camera,          |
|  VertexBuffer/IndexBuffer, Material(-Instance), Texture,     |
|  Light, Renderable, GltfLoader/GltfAsset                     |
|  - SafeHandle ownership, argument validation, disposal rules |
+-------------------------------------------------------------+
|  Source-generated P/Invoke (LibraryImport, static partial)   |
|  src/FilamentSharp/Native.cs — no DllImport, no reflection,  |
|  no runtime codegen; trimming + AOT analyzers enabled        |
+-------------------------------------------------------------+
|  C ABI bridge (libFilamentSharp.Native.so)                   |
|  extern "C", opaque handles, fixed-width types,              |
|  status codes + thread-local error text, no exceptions       |
|  across the boundary                                         |
+-------------------------------------------------------------+
|  Google Filament v1.78.0 (static libs from the official      |
|  release SDK): filament, backend, gltfio, utils, ...         |
+-------------------------------------------------------------+
|  Graphics backend: Vulkan (llvmpipe software ICD in          |
|  headless environments), NOOP for CI-style structural runs   |
+-------------------------------------------------------------+
```

## The C ABI bridge

`src/FilamentSharp.Native` compiles a small C++ translation unit set into
one shared library. Everything it exposes lives in
`include/filamentsharp/abi.h`:

- **Opaque handles.** Every Filament object is wrapped in a one-field
  struct (`fs_engine`, `fs_renderer`, ...). The C side never exposes C++
  types, inheritance, or templates; layouts of the few plain structs
  (`fs_renderable_desc`, `fs_vertex_attribute_desc`) are pinned by
  compile-time `static_assert`s in the native tests and by managed
  `Marshal.OffsetOf` assertions.
- **Status codes.** Every function returns `fs_status` (or a nullable
  handle). Failures set thread-local UTF-8 text readable with
  `fs_get_last_error()`. No C++ exception can cross the boundary: the
  `FS_TRY`/`FS_END*` macros in `src/bridge.h` map exceptions onto error
  returns.
- **Versioning.** `fs_abi_version()` (currently 1) lets the managed side
  reject an incompatible bridge before calling anything else.
- **No I/O.** The bridge performs no filesystem or network access. Content
  (material packages, glTF blobs, pixel data) is passed through memory.

## Ownership registry

`src/registry.h` implements the lifetime core. All rules are enforced under
one mutex:

1. Every engine wrapper stays registered for the process lifetime
   ("immortal wrapper"): a late `Dispose()` from a finalizer thread can
   always detect that the engine is gone instead of dereferencing freed
   memory.
2. Every child is registered at creation with (a) a type-correct wrapper
   deleter and (b) a type-correct Filament-object destroy function, in
   **creation order**.
3. `fs_engine_destroy` flips the engine's `alive` flag first; every child
   operation that observes a dead engine becomes a documented no-op
   (`FS_OK` for destroys, `FS_ERROR_INVALIDATED` for everything else).
   Live children's Filament objects are then destroyed explicitly in
   **reverse creation order (LIFO)** before `Engine::destroy` runs — this
   is required, not cosmetic: Filament's `FMaterial` destructor terminates
   the process when material instances outlive their material, and
   Filament's own engine shutdown does not order leftover children
   (verified against v1.78.0). Wrappers are freed only after the engine
   has finished all pending work (readback buffers).

## Managed layer rules

- `SafeHandle`-based ownership: native release runs exactly once, from
  `Dispose()` or the finalizer; disposal is idempotent.
- The child handle stores the owning engine's (immortal) native wrapper, so
  release after engine disposal takes the native no-op path instead of
  using a dangling pointer.
- Public operations validate arguments in managed code (argument
  exceptions) and post-disposal use throws `ObjectDisposedException`
  without an interop hop.
- `Native.cs` uses `[LibraryImport]` source generation exclusively. String
  returns are declared as `IntPtr` and copied with
  `Marshal.PtrToStringUTF8` — a `[MarshalAs(LPUTF8Str)]` return would make
  the marshaller free a bridge-owned pointer (heap corruption; found and
  fixed during development).

## Backend strategy

Filament's Linux GL backend requires an X display even for headless swap
chains, so in this environment (and any headless CI) the working backends
are:

- **Vulkan** with a software ICD (llvmpipe). This is the primary path for
  pixel-verified integration tests and the sample. `scripts/setup-headless-vulkan.sh`
  installs a user-local lavapipe ICD and `scripts/env.sh` exports
  `VK_ICD_FILENAMES`.
- **NOOP**, which runs the full object model, frame loop, and readback
  plumbing with zeroed pixels — used by the unit suite for lifecycle,
  validation, and layout coverage without a GPU.

## Threading

The bridge serializes all Filament calls behind the registry mutex
(Filament itself requires external synchronization). Engine destruction is
internally synchronized and safe from a finalizer thread. The error state
is thread-local.
