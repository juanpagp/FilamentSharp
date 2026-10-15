# Deployment

## Publishing the sample as a Native AOT executable

```bash
# One-time: fetch the pinned Filament SDK and build the native bridge
scripts/fetch-filament.sh
scripts/build-native.sh

# Publish (bundles bridge + assets, then smoke-runs)
PATH=":$PATH" scripts/publish-aot.sh
```

Output: `external/aot-publish/`

```
BasicRendering          # self-contained native ELF (no .NET runtime)
external/native-install/lib/libFilamentSharp.Native.so
external/native-install/materials/*.filamat
assets/models/triangle.glb
```

The executable is a stripped ~1.4 MB ELF binary whose only dynamic
dependencies are the system C library and libm. It carries no .NET runtime:
the managed code was compiled ahead of time, which is why the binding is
built without reflection or runtime code generation.

## Runtime requirements

- `libFilamentSharp.Native.so` (the C ABI bridge) must be locatable. The
  resolver checks, in order: `$FILAMENTSHARP_NATIVE_DIR`, the application
  base directory, then `external/native-install/lib` relative to ancestors
  of the base directory (development checkout layout).
- The bridge links X11 (`libX11.so`) because Filament's Linux platform
  factory requires it, even for headless rendering.
- A rendering backend:
  - **Vulkan** — any ICD. For headless machines, `scripts/setup-headless-vulkan.sh`
    installs a user-local llvmpipe (software) ICD and writes
    `external/vulkan-runtime/lvp_icd_local.json`; `scripts/env.sh` exports
    `VK_ICD_FILENAMES` when that file exists.
  - **NOOP** — runs the full API with zeroed pixels; useful for smoke tests
    on machines without any GPU.

## Using FilamentSharp in your own application

1. Reference the `FilamentSharp` project (or a packaged build of it).
2. Ensure the native bridge is next to your output (see resolver order
   above) or set `FILAMENTSHARP_NATIVE_DIR`.
3. Ship your matc-compiled `.filamat` materials and self-contained GLB
   assets with the app.
4. Publish with `/p:PublishAot=true` for self-contained native executables,
   or as a regular self-contained framework-dependent app — the library has
   `IsAotCompatible=true`, trimming analyzers enabled, and no reflection.

## Version compatibility

The bridge reports its ABI version (`NativeInfo.AbiVersion`) and the pinned
Filament release (`NativeInfo.FilamentVersion`, `NativeInfo.FilamentCommit`).
The managed layer refuses to run against a bridge with a different ABI
version. The bridge is built against exactly one Filament release
(`deps/filament.lock`); mixing bridges across Filament versions is not
supported.
