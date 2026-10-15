# Testing

Four layers, all run from the repository root. Honest results only: every
command below is the exact one used in development, and skips are
environmental with the reason printed by the test itself.

## 1. Native bridge tests (C++, CTest)

```bash
PATH=... scripts/build-native.sh
```

Builds the bridge and runs 8 ctest suites:

| Test | Covers |
|---|---|
| `abi_c` | the public header compiles as plain C; every documented symbol resolves via `dlsym` |
| `foundation` | ABI version, Filament version/commit contracts, error model, struct layouts |
| `engine` | engine lifecycle: creation (all backends available to it), idempotent destruction, cross-engine independence, child invalidation |
| `rendering` | renderer/swap chain/view/scene/camera lifecycle, frame loop, readback (Vulkan path pixel-verified) |
| `scene` | entities, transforms, vertex/index buffers, scene membership |
| `materials` | material loading incl. adversarial packages, parameters, instances, textures, lights, renderables, refused destroys, engine teardown with leaked material+instance+renderable+scene+light (LIFO proof) |
| `gltf` | GLB parse/queries, junk rejection, handle validation, multi-asset lifecycle, loader-owns-assets teardown, engine death with leaked loader+asset |
| `bridge_smoke` | loads the library and prints version info |

The Vulkan-dependent tests require a Vulkan ICD; `scripts/env.sh` exports
`VK_ICD_FILENAMES` to the user-local lavapipe ICD installed by
`scripts/setup-headless-vulkan.sh`. Without it, those tests abort inside
Filament's instance creation — run everything through the scripts.

## 2. Managed unit tests (NOOP backend, no GPU)

```bash
PATH=... scripts/run-managed-tests.sh
```

48 xUnit facts covering the object model: lifecycle and idempotent
disposal, argument validation, invalidation after engine disposal, the
managed mirror of the `fs_renderable_desc` layout (byte-for-byte
`Marshal.OffsetOf` pinning), transform math round-trips, material parameter
introspection, glTF queries, and the native-window swap chain lifecycle
(fake XID on NOOP: create → frame loop → dispose).

## 3. Managed integration tests (real native bridge)

Same command as (2); the integration project loads `libFilamentSharp.Native.so`
and runs:

- bridge interop: ABI version agreement, all entry points resolve, native
  errors propagate as exceptions with text;
- engine lifecycle on Vulkan when available;
- frame loop with readback of the clear color (pixel-verified against the
  documented lavapipe tolerance);
- full scene rendering, pixel-verified:
  - unlit full-screen triangle sampled against its sRGB material color,
  - lit textured quad with a directional light (red-foreground /
    dark-background proportions),
  - glTF triangle loaded from `assets/models/triangle.glb`;
- windowed rendering: `WindowedRenderingSampleRunsAndShutsDownCleanly` runs
  the sample end-to-end when `DISPLAY` is set — a real X11 window, a
  native-window swap chain, 5 frames, exit code 0, `completed cleanly`
  asserted in the output; without a display it prints
  `ENVIRONMENTAL SKIP: no DISPLAY` and passes.
- every Vulkan test prints `ENVIRONMENTAL SKIP: ...` and passes when no
  ICD is available (the reason is asserted to be environmental, never a
  silent pass).

## Pixel-verification tolerances (lavapipe)

llvmpipe's readback is close to, but not exactly, sRGB. Measured against
the pinned release: clear (0.02, 0.02, 0.05) linear reads back luma
~0.10-0.11; pure linear magenta reads back with ~0.14 green crosstalk.
Tests therefore use structural assertions (proportions of red/dark pixels)
or per-channel tolerance 0.2, and document the measured baseline in the
assertion comments.

## 4. Native AOT publish test

```bash
PATH=... scripts/publish-aot.sh
```

Publishes **both** samples as self-contained Native AOT executables, bundles
the bridge and assets, and smoke-runs them. `BasicRendering` runs headlessly;
`WindowedRendering` runs its smoke when `DISPLAY` is set and otherwise
reports the skip honestly (build + link still verified). Verified
properties: stripped ELF, zero .NET runtime dependencies, pixel statistics
identical to the JIT run (BasicRendering), exit code 0 with `completed
cleanly` in the output (WindowedRendering).

## 5. Windowed rendering verification (reference environment)

Real runs on Xvfb + lavapipe in the reference container (2026-10-15):

- `vkCreateSwapchain: 128x96 ... swapchain-size=4` — a real Vulkan swap chain
  created from the X11 window through XCB;
- 5-frame run: `rendered 5 frame(s), 0 resize(s)` + exit 0;
- 240-frame run at 800×600: ~63 fps average on llvmpipe, exit 0;
- Native AOT build (1.4 MB stripped ELF) behaves identically to the JIT run;
- without `DISPLAY`: clean error, exit 1; without any Vulkan ICD the engine
  aborts inside Filament at the pinned release (documented upstream behavior;
  `--backend noop` is the explicit escape hatch).

Resize events require a window manager; the reference Xvfb has none, so the
resize path is covered by code review + desktop runs, not by automation
(see `docs/windowed-rendering.md` §8).

## Clean-checkout verification

```bash
scripts/verify-sdk.sh        # checksum of the fetched SDK against deps/filament.lock
scripts/clean.sh             # remove build outputs, keep fetched deps
# then: fetch -> build native -> managed tests -> AOT publish
```
