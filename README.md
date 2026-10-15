# FilamentSharp

Idiomatic, Native-AOT-compatible C# binding for [Google Filament](https://github.com/google/filament),
the real-time physically based rendering engine.

FilamentSharp lets an ordinary C# application create and drive a Filament
rendering engine — engine, renderer, swap chains (offscreen **and**
windowed), scenes, cameras, geometry buffers, materials, textures, lights,
renderables, and glTF 2.0 assets — with pixel-verified headless rendering,
real on-screen windowed rendering, and a clean, ordered shutdown, publishable
as a self-contained native executable.

```csharp
using FilamentSharp;

using var engine = Engine.Create(Backend.Vulkan);
using var renderer = engine.CreateRenderer();
using var swapChain = engine.CreateHeadlessSwapChain(128, 128);   // offscreen
// or, for a real OS window (X11): engine.CreateNativeSwapChain(xid)
using var view = engine.CreateView();
using var scene = engine.CreateScene();

using var loader = engine.CreateGltfLoader();
using var asset = loader.Load(File.ReadAllBytes("triangle.glb"));
foreach (var entity in asset.GetEntities())
{
    scene.AddEntity(entity);
}
// ... camera, light, frame loop, readback, disposal ...
```

The stack is:

```
C# application
  └─ FilamentSharp (net10.0, source-generated P/Invoke, SafeHandle ownership)
       └─ libFilamentSharp.Native.so (stable C ABI bridge: 76 fs_* entry points,
          error model, versioned ABI, ownership registry)
            └─ Google Filament v1.78.0 (official prebuilt release SDK, Apache-2.0)
                 └─ Vulkan (software ICD works headless) or NOOP
```

Design highlights:

- **No reflection, no runtime codegen.** `[LibraryImport]` source-generated
  P/Invoke only; trimming and AOT analyzers enabled; verified as a Native
  AOT self-contained publish (stripped ~1.4 MB ELF, zero .NET deps).
- **Stable C ABI.** `extern "C"`, opaque handles, fixed-width ints, status
  codes + thread-local error text; no C++ exceptions cross the boundary;
  ABI version negotiation; the header compiles as plain C.
- **Sound ownership.** SafeHandle-based lifetimes; an ownership registry
  that destroys leaked children in reverse creation order at engine death
  (Filament requires material instances to die before materials, or the
  process aborts — handled and tested).
- **Real rendering, honestly tested.** Pixel-verified integration tests on
  Vulkan (llvmpipe) with documented readback tolerances; NOOP backend for
  GPU-less structural coverage; environmental skips print their reason.
  Windowed rendering is real too — a live X11 window presenting animated
  frames through a native-window swap chain (verified on Xvfb + lavapipe).

## Quick start (Linux x64)

```bash
# 0. Prerequisites: .NET SDK 10, CMake >= 3.20, Ninja, a C++17 compiler
#    (clang + libc++ + lld; the Filament SDK is built with libc++).
# 1. Fetch the pinned Filament release SDK (into external/, Git-ignored):
./scripts/fetch-filament.sh
# 2. Build the native bridge (CMake + Ninja) and run the native tests:
./scripts/build-native.sh
# 3. Build everything managed and run the managed test suites:
./scripts/run-managed-tests.sh
# 4. Publish + run the samples as Native AOT executables:
./scripts/publish-aot.sh
./external/aot-publish/BasicRendering
# (headless machine? ./scripts/setup-headless-vulkan.sh first)
# 5. Real on-screen rendering in an X11 window (needs a display; Xvfb works):
Xvfb :99 -screen 0 1024x768x24 -nolisten tcp &
DISPLAY=:99 ./external/aot-publish/WindowedRendering/WindowedRendering --frames 240
```

The two samples: **BasicRendering** renders offscreen (a procedural lit
textured quad and a glTF triangle), verifies pixels, and writes PPM images;
**WindowedRendering** puts a real, animated, lit scene in an X11 window —
native-window swap chain, resize handling, keyboard quit, clean shutdown.

Documentation: `docs/building.md` · `docs/architecture.md` ·
`docs/api-coverage.md` · `docs/testing.md` · `docs/deployment.md` ·
`docs/windowed-rendering.md` (non-headless rendering) ·
`docs/continuing-development.md` (extending this repository) ·
`docs/third-party-notices.md` · plan and progress: `docs/implementation-plan.md`.

## License

FilamentSharp code is MIT licensed (see `LICENSE`). Google Filament is
Apache-2.0; third-party notices are in `docs/third-party-notices.md`.
