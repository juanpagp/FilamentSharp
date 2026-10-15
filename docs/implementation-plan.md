# FilamentSharp Implementation Plan

Status: **Complete** — all planned milestones (§6) are implemented, tested,
and committed. The final feature state is recorded in
`docs/api-coverage.md`; the commit history follows §8. Highlights of what
was verified along the way (each finding has a regression test):

* Filament v1.78.0 terminates the process when a `Material` is destroyed
  with live `MaterialInstance`s — including during engine shutdown. The
  bridge's ownership registry destroys leaked children in reverse creation
  order before `Engine::destroy` (LIFO proof in `tests/native/test_materials.cpp`).
* Filament's `.filamat` parser reads past the buffer end for truncated
  packages (a 4-byte payload segfaults). The bridge structurally pre-validates
  the chunk chain and rejects such packages with an error.
* gltfio's `ResourceLoader` destructor touches its registered texture
  providers; the loader teardown order (ResourceLoader before decoder) is
  load-bearing and pinned by `tests/native/test_gltf.cpp`.

This document records the engineering plan that governed the incremental
implementation of FilamentSharp. The Git history follows the commit sequence
described at the bottom.

## 1. Goal

FilamentSharp is an idiomatic, Native-AOT-compatible C# binding for
[Google Filament](https://github.com/google/filament), the real-time
physically-based rendering engine. The primary deployment target is a C#
application published as a **Native AOT** executable on **Linux x64** that does
not require an installed .NET runtime. Native Filament libraries, EGL/OpenGL
runtime libraries, and graphics drivers remain runtime requirements and are
never misrepresented as self-contained.

## 2. Environment findings (verified 2026-10-15)

The reference development environment is a constrained, headless container:

| Item | Value |
|------|-------|
| OS | Debian 13 (trixie), x86_64 |
| CPU / RAM | 2 cores / 4.1 GiB, no swap |
| Disk budget | ~9 GiB usable |
| Compilers | gcc/g++ 14.2 (system), clang 19 (user-local, for Native AOT link) |
| Build tools | CMake 3.31.6 + Ninja 1.13 (user-local via pip wheels) |
| .NET SDK | 10.0.402 (user-local, outside the repo) |
| GPU / display | **None** (no `/dev/dri`, no X11/Wayland) |
| EGL | No system `libEGL.so.1`; user-local Mesa EGL (llvmpipe software rendering) assembled from official Debian packages into a repo-external sysroot |
| Vulkan | Loader present, **no ICD** — software rendering achieved with a user-local Mesa lavapipe ICD assembled from the official `mesa-vulkan-drivers` Debian package |
| Network | Full outbound (github.com, nuget.org reachable) |

Consequences (verified empirically against the real SDK):

* The release SDK's static libraries are built with **libc++** (`std::__1`
  symbols), so the native bridge is built with **clang + libc++**, and linked
  with **lld** (GNU `ld` takes over 10 minutes on this machine; lld links the
  same inputs in about one second).
* The Filament **OpenGL backend requires X11/GLX on Linux** — engine creation
  aborts with "Failed to open X display" even for headless swap chains, and
  it does not use EGL on this platform. GL is therefore selectable in the API
  but cannot be exercised in this environment without an X server.
* **Vulkan + Mesa lavapipe** (software ICD) works fully headless: engine,
  frame rendering, and synchronous `readPixels` readback all verified.
  Rendering tests run on this path.
* The **NOOP backend** works for GPU-free lifecycle testing (callbacks fire,
  pixel readback returns zeroes).
* A full upstream Filament **source build** is possible but expensive on 2
  cores / 4 GiB. The default dependency strategy therefore consumes the
  **official prebuilt release SDK**, and the source-build path is documented
  and scripted but flagged as not exercised in this environment unless the
  verification build below succeeds.

## 3. Architecture

```
C# application (samples, tests)
        │  idiomatic managed API (SafeHandle ownership, XML docs)
        ▼
FilamentSharp (net10.0 class library, source-generated P/Invoke)
        │  [LibraryImport] — UTF-8 strings, blittable structs, spans
        ▼
libFilamentSharp.Native.so  (C ABI bridge, extern "C", opaque handles)
        │  status codes + thread-local error state, no exceptions cross the ABI
        ▼
Filament static libraries (official v1.78.0 release SDK)
        ▼
OpenGL backend → EGL → Mesa llvmpipe (headless) │ NOOP backend (fallback)
```

Key decisions:

* **C ABI bridge** (`src/FilamentSharp.Native`): every exported symbol is
  `extern "C"`, uses fixed-width integers, opaque pointer handles, explicit
  buffer lengths, and a documented status-code + `fs_get_last_error()`
  thread-local error model. No C++ types, exceptions, STL, or implementation
  details cross the boundary. All native code is wrapped in `try/catch`;
  failures return `FS_ERROR`/null with a retrievable UTF-8 message.
* **Ownership model**: Filament objects are owned by an `Engine`. The bridge
  keeps a per-engine registry of live child handles; destroying the engine
  invalidates children, and destroying a child afterwards is a safe no-op.
  The managed layer wraps every native resource in a `SafeHandle` whose
  release path goes back through the bridge, and managed wrappers hold a
  reference to their owning engine so a disposed engine surfaces as
  `ObjectDisposedException`/`InvalidOperationException` instead of
  undefined behavior. Double disposal is idempotent.
* **Managed interop**: `LibraryImport` source generation only (no runtime
  reflection or codegen), `IsAotCompatible` analyzers enabled, strings
  marshalled as UTF-8, buffers passed as pinned spans, `System.Numerics`
  math types. A resolver loads the native bridge only from an explicit,
  documented set of locations (env var, app dir, repo layout) — never from
  the current working directory or other unsafe search paths.
* **Materials** are authored as Filament `.mat` sources and compiled offline
  with the pinned release's `matc` at native build time; the small binary
  `.filamat` outputs are treated as build artifacts, and material sources are
  committed.

## 4. Dependency strategy

* **Pin**: Filament **v1.78.0**, tag commit `c1dc4e444784850d6dc891a1fb38747c30b85463`,
  official release artifact `filament-v1.78.0-linux.tgz`
  (76,777,878 bytes, SHA-256 recorded in `deps/filament.lock`).
  Upstream does not publish checksums for release assets; the lock file's
  SHA-256 was computed from the verified first download from GitHub and pins
  every subsequent fetch (tamper detection against the recorded first-party
  value).
* `scripts/fetch-filament.sh` downloads, verifies, and extracts the SDK into
  the Git-ignored `external/filament-sdk`. `FILAMENT_SDK_DIR` lets a developer
  supply an existing (e.g. self-built) Filament install instead.
* `scripts/build-filament-from-source.sh` documents the from-source path
  (pinned tag, recursive shallow clone, CMake options). It is best-effort:
  see `docs/building.md` for its verification status in this environment.
* The `matc` material compiler always comes from the same pinned SDK, so asset
  tooling never mixes releases.

## 5. Platform scope

* **Tier 1 (implemented + tested here)**: Linux x64, **Vulkan backend (headless,
  lavapipe software rendering)**, NOOP backend. Native AOT publish + run.
* **Tier 2 (implemented, untested here)**: Linux x64 with a real GPU (Vulkan
  or OpenGL/X11); the OpenGL backend requires a running X server on Linux
  (GLX) — verified limitation, not an API gap.
* **Out of scope for this iteration** (documented, not implemented): Windows,
  macOS/Metal, Android, WebGPU, windowed swap chains from OS handles on X11
  (an advanced overload exists for native windows on Linux).

## 6. API milestones (each = one tested commit)

1. **Scaffold** — solution, projects, scripts, docs skeleton, CI stub.
2. **Dependency workflow** — pinned fetch/verify scripts + lock file.
3. **C ABI foundation** — `fs_abi_version`, `fs_filament_version`, error
   model, symbol export discipline, native smoke test, C header
   compatibility test.
4. **Engine lifecycle (native)** — engine create/destroy, backend selection,
   child-handle registry, native tests.
5. **Managed interop foundation** — library resolver, `LibraryImport` layer,
   SafeHandles, exceptions, managed engine lifecycle tests.
6. **Rendering infrastructure** — Renderer, headless SwapChain, View, Scene,
   Camera, frame loop (`beginFrame`/`render`/`endFrame`), synchronous
   `readPixels` capture; first real rendered frame test (clear color).
7. **Entities and scene construction** — entity manager, transforms,
   renderables, vertex/index buffers with layout validation; unlit colored
   triangle rendered and pixel-verified.
8. **Materials, textures, lighting** — `matc` integration, `Material`,
   `MaterialInstance` typed parameters, `Texture` + sampler, directional and
   point lights via `LightManager`; lit/textured quad pixel-verified.
9. **glTF asset loading** — `gltfio` AssetLoader + UbershaderProvider
   (archive from the SDK) + ResourceLoader, asset lifecycle, bounding boxes,
   entity ownership; a small self-authored test glb; sample extended.
10. **Native AOT validation** — example published `linux-x64` Native AOT,
    executed, output verified; deployment docs.
11. **Docs finalization** — API coverage matrix, clean-checkout instructions,
    third-party notices, testing/deployment docs, CI.
12. **Windowed (non-headless) rendering** — `fs_swap_chain_create_native`
    (X11 XID → surface via XCB/GLX), managed `CreateNativeSwapChain`, the
    `WindowedRendering` sample (Xlib interop, event pump, resize = swap-chain
    recreate, keyboard quit, ordered teardown), `publish-aot.sh` covering both
    samples with a DISPLAY-gated smoke, and a DISPLAY-gated integration test.
    Verified for real on Xvfb + lavapipe (vkCreateSwapchain from the window,
    240 frames @ 800×600 ≈ 63 fps, clean exit 0).
13. **Continuation documentation** — `docs/windowed-rendering.md` (the
    non-headless guide) and `docs/continuing-development.md` (environment
    bootstrap, workflow, extension recipes, packaging contract).

## 7. Testing strategy

* **Native tests** (`tests/native`, CTest): ABI/version contracts, C header
  compilability, exported-symbol presence via `dlsym`, error model, engine
  lifecycle, ownership/double-destroy/ordering, null/invalid inputs.
* **Managed unit tests** (xunit): argument validation, disposal semantics,
  state transitions, error propagation — run against the real bridge but
  without requiring a GPU where possible (NOOP engine).
* **Interop integration tests** (xunit): load the real compiled bridge,
  resolve every used entry point, full object graph create/use/destroy,
  error propagation across the ABI.
* **Rendering integration tests** (xunit, skipped with explicit reasons when
  EGL is unavailable): real frames through the GL backend on a headless
  swap chain; content verified by reading back pixels after
  `Engine::flushAndWait()`.
* **Windowed integration test** (xunit): runs the `WindowedRendering` sample
  end-to-end when `DISPLAY` is set (real X11 window + native-window swap
  chain + finite frame budget + clean shutdown; exit code and output
  asserted); self-skips with a reason when headless.
* **Native AOT test**: publish + run the samples for `linux-x64`.
* Reporting: every claim in `docs/testing.md` reflects a command that was
  actually executed; skips record their environmental cause.

## 8. Commit sequence

Conventional commits, one per completed, tested milestone (the sequence in
§6), authored with the locally configured identity (`Z User <z@container>`,
repository-local). Features are implemented, built, and tested before their
commit; the history reflects the real development order.

## 9. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| OOM during any native compile (2 cores/4 GiB) | `-j1`/`-j2` budgets, bridge is small; full Filament source build is optional |
| lavapipe rendering differences vs GPU | pixel assertions use tolerant thresholds (±0.12) and structural checks (foreground/background separation, channel dominance) rather than exact colors; calibration numbers recorded in docs/testing.md |
| OpenGL backend needs X11 (GLX) on Linux | rendering tests run on Vulkan+lavapipe; GL selectable and documented |
| glTF ubershader availability | SDK ships `libuberarchive.a`; fallback: `matc`-compiled minimal materials |
| Release SDK lacks CMake package config | bridge CMake defines explicit imported targets for the SDK layout and also accepts a source-built install of the same layout |
| Release SDK built with libc++ | bridge toolchain pinned to clang + libc++ + lld (documented in docs/building.md) |
