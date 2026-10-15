# Building FilamentSharp

Everything below assumes a clean checkout of this repository and Linux x64.

## Prerequisites

| Requirement | Notes |
|---|---|
| .NET SDK 10.0 | pinned via `global.json` (any 10.0.xxx) |
| CMake ≥ 3.20 + Ninja | used for the native bridge |
| clang ≥ 15 with libc++ | **the Filament release SDK is built with libc++**; the bridge must match. Debian/Ubuntu: `apt install clang libc++-dev lld` |
| `libx11-dev`, `zlib` dev | link-time dependencies of the Filament backend libraries |
| curl, git, a C++17 toolchain | for dependency fetching |
| *(optional)* Vulkan ICD | needed to actually run the Vulkan backend; `scripts/setup-headless-vulkan.sh` installs a software (lavapipe) ICD user-locally on Debian/Ubuntu |

For a machine-local clang/libc++ (no root), see `scripts/env.local.sh.example`.

## 1. Fetch the pinned Filament release SDK

```bash
./scripts/fetch-filament.sh
```

Downloads the pinned official release artifact (`deps/filament.lock`:
Filament **v1.78.0**, tag commit `c1dc4e44...`, SHA-256 verified), and extracts
it to `external/filament-sdk` (Git-ignored). Set `FILAMENT_SDK_DIR` to supply
an existing install (for example one produced by
`scripts/build-filament-from-source.sh`).

## 2. (Optional) Headless software Vulkan for GPU-less machines

```bash
./scripts/setup-headless-vulkan.sh   # no-op when a Vulkan ICD already exists
```

## 3. Verify the SDK end-to-end

```bash
./scripts/verify-sdk.sh              # compiles+runs tests/native/filament_smoke.cpp
./scripts/verify-sdk.sh noop vk gl   # explicit backend list
```

The smoke test creates a real Filament engine, renders frames into a headless
swap chain, and reads pixels back. `noop` needs no GPU; `vk` needs a Vulkan
ICD; `gl` needs a running X server (the OpenGL backend uses GLX on Linux).

## 4. Build the native bridge

```bash
./scripts/build-native.sh            # CMake + Ninja; output in external/native-install
```

## 5. Build and test the managed solution

```bash
./scripts/run-managed-tests.sh       # dotnet build + unit + integration tests
```

## 6. Publish the sample as a Native AOT executable

```bash
./scripts/publish-aot.sh             # artifacts/publish/linux-x64/BasicRendering
```

## Clean rebuild

```bash
./scripts/clean.sh                   # removes native + managed build outputs
```

The full clean-checkout validation performed for this delivery is described in
`docs/testing.md`.

## Building Filament from source instead

`scripts/build-filament-from-source.sh` clones the pinned tag (with
submodules), configures CMake (Vulkan + OpenGL backends, samples off) and
installs to `external/filament-sdk-src-install`. Point
`FILAMENT_SDK_DIR` at that directory afterwards.

> **Status: unverified in the reference environment.** The reference machine
> (2 vCPU / 4 GiB RAM) cannot complete the upstream build within the session
> budget, so the default workflow uses the official prebuilt SDK. The script
> follows upstream `BUILDING.md` and is expected to work on a normal developer
> machine, but that claim is **not** backed by an executed run here.

## Environment variables

All scripts honor the overrides documented in `scripts/env.sh`:

| Variable | Meaning | Default |
|---|---|---|
| `FILAMENT_SDK_DIR` | Filament SDK/installs root | `external/filament-sdk` |
| `FILAMENTSHARP_NATIVE_DIR` | native bridge output dir | `external/native-install/lib` |
| `CXX` | C++ compiler for the bridge | `clang++` |
| `FILAMENTSHARP_LIBCXX_INCLUDE` / `_LIB` | user-local libc++ paths | (unset) |
| `VK_ICD_FILENAMES` | Vulkan ICD manifest | auto (local lavapipe if present) |

Machine-specific values belong in `scripts/env.local.sh` (Git-ignored).
