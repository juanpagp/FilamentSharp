# API Coverage

Status of the FilamentSharp binding against the pinned Google Filament
release (**v1.78.0**, commit `c1dc4e444784850d6dc891a1fb38747c30b85463`;
see `deps/filament.lock`). This document is the honest map of what is bound,
what is deliberately out of scope for this revision, and why.

The managed API surface: 24 public types (1 engine, 14 resource wrappers,
4 option/struct types, 7 enums, exception + info helpers), 132 public
members. Every bound entry point is verified against the pinned release's
headers at build time (native layout static-asserts) and at runtime
(ABI-resolution integration test).

## Fully bound

| Filament concept | Managed API | Notes |
|---|---|---|
| `Engine` lifecycle | `Engine.Create` / `TryCreate` / `Dispose` | All five backends selectable (`Backend.Default/OpenGL/Vulkan/Metal/WebGPU/Noop`); creation failure maps to `TryCreate == false` instead of an abort where possible |
| `Engine` queries | `Engine.ActiveBackend` | Resolved backend after creation |
| `Renderer` | `Renderer` | `BeginFrame` / `Render` / `EndFrame` frame loop; `SetClearOptions`; synchronous `ReadPixels` + `CompleteReadPixels` readback |
| `SwapChain` | `SwapChain`, `SwapChainConfig` | Headless (offscreen) **and** native-window swap chains (`Engine.CreateNativeSwapChain(XID)` — X11 on Linux; surface via XCB on Vulkan, GLX on OpenGL; resize = destroy + recreate on the same handle). `SwapChainConfig` flags mirror `backend::SwapChainFlags` |
| `View` | `View` | Camera/scene wiring, viewport, background clear color; null scene accepted |
| `Scene` | `Scene` | Add/remove/query entities, entity counts, membership |
| `Camera` | `Camera` | Perspective/orthographic projection (perspective: all FOV directions; orthographic: full 6-plane slab — added for screen-space overlay layers such as PAL's text renderer), `LookAt`, view/projection matrix readback |
| `EntityManager` | `EntityManager` | Create/destroy entities (process-global, not engine-owned) |
| Transforms | `Engine.SetTransform`, `Engine.GetTransform`, `Engine.SetParent` | `mat4` world/local transforms via System.Numerics |
| `VertexBuffer` | `VertexBuffer`, `VertexAttributeDescription`, `VertexAttribute`, `ElementType` | All 12 standard attributes + custom attributes; all 26 element types; multi-buffer or interleaved layouts; per-buffer upload |
| `IndexBuffer` | `IndexBuffer`, `IndexType` | 16/32-bit indices, upload |
| `Material` | `Material` | Load matc-compiled `.filamat` packages (structurally pre-validated — see below), parameter introspection (`ParameterNames`) |
| `MaterialInstance` | `MaterialInstance` | Typed setters: float, float3, float4, bool, int, texture+sampler |
| `Texture` | `Texture`, `TextureFormat`, `TextureMinFilter/MagFilter/Wrap` | 2D RGBA8 / sRGB8_A8 with mips; RGBA8 byte upload |
| `TextureSampler` | sampler args on `MaterialInstance.SetTexture` | Filters and wrap modes |
| `LightManager` | `Light`, `LightType`, `LightOptions` | All five light types (sun, directional, point, focused spot, spot), color/intensity/direction/position/falloff/castShadows |
| `RenderableManager` | `Renderable`, `RenderableOptions` | Single-primitive renderables: geometry, bounds, culling, shadow flags |
| glTF (`gltfio`) | `GltfLoader`, `GltfAsset` | GLB / embedded-buffer glTF 2.0; ubershader materials (no runtime shadergen); stb PNG/JPEG decoding for embedded textures; entities/root/bounding box/renderable count; animation count + apply |
| Error model | `FilamentSharpException` | Every native failure surfaces as an exception carrying the bridge's diagnostic text; no exceptions cross the ABI |

## Bound with documented constraints

- **Material packages** must be self-contained and structurally valid. The
  bridge walks the `.filamat` chunk chain (8-byte tag + u32 size per chunk)
  and rejects truncated/corrupt packages itself, because Filament's parser
  can read past the buffer end for such inputs (a 4-byte package segfaults
  the engine without this check — verified against the pinned release).
- **Materials vs. instances**: Filament refuses to destroy a material while
  instances are alive. Explicit out-of-order disposal is safe (the refusal
  is surfaced as an error / ignored by `Dispose()` semantics), and the
  engine's teardown destroys live children in reverse creation order, which
  guarantees instances die before materials.
- **glTF resources**: the bridge performs no filesystem access. Load
  self-contained GLB files (embedded buffers, embedded textures). JSON glTF
  with external `.bin`/image files is not fetched; wrap such content in a
  GLB first.
- **TANGENTS**: lit-shading materials require a tangent quaternion
  attribute. glTF assets get this from gltfio; hand-built geometry must
  supply it (`VertexAttribute.Tangents`, typically `ElementType.Short4`
  normalized).

## Intentionally not bound in this revision

| Area | Rationale |
|---|---|
| On-screen swap chains (X11/Wayland/Win32 windows) | The X11 path is bound (`fs_swap_chain_create_native` + `Engine.CreateNativeSwapChain`, exercised by the `WindowedRendering` sample); Wayland/Win32/macOS handles remain out of scope for this revision — the bridge takes a raw `void*` platform handle by design, so adding them is a platform-layer decision, not an ABI change |
| Skyboxes, `IndirectLight`, IBL (`cmgen`) | Image-based lighting pipeline (`.cmgen` environment maps) is substantial and not required for the covered scenes; materials fall back to analytic lighting |
| Post-processing stack (`View.SetPostProcessingEnabled`, bloom, DOF, TAA, color grading) | `View` exposes the pipeline enable switches through clear/viewport today; per-effect quality settings are follow-up surface |
| Morph targets / skinning on hand-built renderables | glTF skinning works through the gltfio path (`Animator`); manual skinning buffer APIs are not exposed |
| Instanced glTF (`createInstancedAsset`) | Single-instance assets only; the ABI shape for instance arrays is a follow-up |
| `filamat` runtime compilation / JIT materials | Deliberately excluded: runtime shader generation conflicts with Native-AOT-friendly deployment; the ubershader provider covers glTF |
| `matdbg`, viewer, filamesh, ktx, DRACO-from-manual-buffers | Engine-debug and auxiliary formats; DRACO is linked (gltfio dependency) and used automatically for glTF meshes that need it |
| Bone/transform animation beyond `applyAnimation` | Cross-fades, warp, and per-bone control are follow-up `Animator` surface |

## ABI surface

The C ABI (`src/FilamentSharp.Native/include/filamentsharp/abi.h`) exposes 77
`fs_*` entry points (each mirrored by a source-generated LibraryImport
declaration) under a single opaque-handle + status-code model with
ABI version 1 (the orthographic-projection entry point added for screen-space
overlays is an additive extension; ABI version 1 clients are unaffected). Every entry point:

- is `extern "C"`, uses fixed-width types, and performs no I/O,
- converts C++ exceptions into `fs_status` + thread-local error text
  (`FS_OK` / `FS_ERROR` / `FS_ERROR_INVALID_ARG` / `FS_ERROR_UNSUPPORTED` /
  `FS_ERROR_INVALIDATED` / `FS_NOT_READY`),
- validates every handle against the ownership registry before touching
  Filament, and
- is verified to resolve by `tests/FilamentSharp.IntegrationTests`
  (`BridgeInteropTests.AllEntryPointsResolveAndBehave`), with the header
  additionally compiled as plain C (`tests/native/test_abi_c.c`) to prove
  portability.
