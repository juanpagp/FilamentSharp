# Continuing Development

A hands-on guide for anyone picking this repository up: how to get a working
environment from a clean checkout, the day-to-day build/test workflow, how to
extend the binding correctly (with the windowed swap chain as the worked
example — it went through every station described here), and the rules that
keep the repository honest.

If you only want to *use* the library, start with the root `README.md` quick
start instead.

## 1. Repository map

```
FilamentSharp/
├── deps/filament.lock            # the pin: version, commit, URL, SHA-256, size
├── src/FilamentSharp/            # managed library (net10.0, LibraryImport, AOT-safe)
│   └── Native.cs                 #   the P/Invoke mirror of abi.h (one decl per entry point)
├── src/FilamentSharp.Native/     # the C ABI bridge (CMake, clang+libc++, static SDK)
│   ├── include/filamentsharp/abi.h   # single source of truth of the boundary
│   └── src/*.cpp                #   engine/rendering/scene/materials/gltf + registry
├── tests/native/                 # CTest suites (plain-C ABI test + C++ contract tests)
├── tests/FilamentSharp.UnitTests/        # NOOP-backed, no GPU needed
├── tests/FilamentSharp.IntegrationTests/ # real bridge + real rendering
├── samples/BasicRendering/       # headless: quad + glTF, readback, PPM output
├── samples/WindowedRendering/    # real on-screen frames in an X11 window
├── scripts/                      # every workflow is a script (see §3)
├── assets/                       # .mat sources, test .glb (matc runs in build)
└── docs/                         # the document you are reading and its siblings
```

Everything under `external/` is *generated state* (Git-ignored): the fetched
SDK, native build/install dirs, lavapipe runtime, AOT outputs. Everything
under `scripts/` is *process*. The two-file contract that matters most:
`abi.h` (native truth) and `Native.cs` (managed mirror) must stay in sync —
the tests enforce this (see §6).

## 2. One-time environment bootstrap

Toolchain requirements: .NET SDK 10, CMake ≥ 3.20, Ninja, clang with libc++
(the Filament SDK is built with libc++ — mixing libstdc++ into the bridge
fails to link or crashes), lld (GNU ld is pathologically slow on the SDK
archives), and about 1 GB of disk for `external/`.

```bash
git clone <repo> FilamentSharp && cd FilamentSharp

# Optional, machine-specific (Git-ignored): scripts/env.local.sh
#   - user-local clang/libc++ paths      (see scripts/env.local.sh.example)
#   - an existing Filament install        (FILAMENT_SDK_DIR=...)
#   - a forced Vulkan ICD                 (VK_ICD_FILENAMES=...)

./scripts/fetch-filament.sh        # pinned SDK -> external/filament-sdk (76 MB)
./scripts/setup-headless-vulkan.sh # only if the machine has no Vulkan ICD
./scripts/build-native.sh          # bridge + matc materials + native tests
dotnet build FilamentSharp.sln     # managed: library, tests, samples
```

Notes for constrained machines (all verified in the reference container):

* No root: `fetch-filament.sh` and `setup-headless-vulkan.sh` download and
  extract *without installing*; `env.local.sh` points at user-local toolchains.
* No GPU: lavapipe (software Vulkan) makes even the *windowed* sample run
  against Xvfb at interactive frame rates (see `windowed-rendering.md`).
* No display at all: `Xvfb` provides one; without any X server the windowed
  sample refuses to start with a clear message (by design).

## 3. Daily workflow

```bash
# Native side (abi.h / bridge C++ changed):
./scripts/build-native.sh            # configure+build+ctest+install (fast, incremental)

# Managed side (C# changed):
dotnet build FilamentSharp.sln
dotnet test FilamentSharp.sln --no-build

# Samples:
dotnet run --project samples/BasicRendering -- <outdir>
dotnet run --project samples/WindowedRendering -- --frames 240

# Full verification before committing / packaging (the exact matrix used for
# the delivery report; run from a clean checkout for release claims):
./scripts/verify-sdk.sh              # SDK sanity independent of the bridge
./scripts/build-native.sh            # 8/8 native suites
./scripts/run-managed-tests.sh       # unit + integration (env-skips recorded)
./scripts/publish-aot.sh             # AOT publish + smoke for every sample
```

Read the test output — do not just grep for "Passed". Environmental skips
print `ENVIRONMENTAL SKIP: <reason>`; they are verified skips (the convention
is documented in `testing.md`), and the reference results are recorded in
`implementation-plan.md` §7.

## 4. Conventions that keep this repo honest

* **Real, not mocked.** No fake handles, no stub backends in tests. NOOP is a
  *real* Filament backend (frames flow, pixels stay zero) and is labeled as
  such wherever it is used.
* **Every claim is reproducible.** Verification commands, exit codes, and
  tolerances live in `testing.md`; the reference environment's results in
  `implementation-plan.md`.
* **The pin is law.** Filament is consumed only through `deps/filament.lock`
  (version + commit + URL + SHA-256 + size). Never point at `main`.
* **Commits are conventional and feature-shaped** (`feat(scope):`,
  `docs:`, `build:`), one tested feature per commit, no history rewrites.
* **AOT is a first-class constraint.** No reflection, no runtime codegen;
  `LibraryImport` everywhere (source-generated P/Invoke), analyzer-clean.

## 5. Adding a new `fs_*` entry point — the worked example

The windowed swap chain (`fs_swap_chain_create_native`) is the most recent
entry point; it went through every station below. Use it as the template
(`git show f54ebd3 8ad3893` for the native and managed halves).

Checklist — native half (one commit):

1. **`abi.h`**: declare with full doc comment (semantics, failure modes,
   platform caveats). Keep it valid ISO C — `test_abi_c.c` compiles it as
   plain C and dlopens the bridge.
2. **`src/FilamentSharp.Native/src/<area>.cpp`**: implement behind
   `FS_TRY`. Validate arguments *before* touching Filament whenever Filament
   would dereference/crash on them (the null-window rejection is there
   precisely because Filament's platform layer dereferences a null handle).
   Register children with the ownership registry (`addChildLocked` with the
   per-type disposer) — this is what makes engine teardown safe.
3. **Native tests**: extend `test_abi_c.c` (resolve the new symbol as C) and
   the suite that owns the area (e.g. `test_rendering.cpp`). Argument
   validation runs in every scenario; full lifecycle only where it is
   actually possible without special hardware (NOOP for the swap chain; the
   real-windowed path is covered by the sample + DISPLAY-gated integration
   test).
4. **`docs/api-coverage.md`**: update the entry-point count and the
   bound/unbound map honestly.

Managed half (one commit):

5. **`Native.cs`**: add the `LibraryImport` declaration (fixed-width types;
   `StringMarshalling.Utf8` for strings; the return-int status pattern).
6. **Public API** in the owning type (e.g. `Engine.CreateNativeSwapChain`),
   with managed-side validation for cases you want as CLR exceptions rather
   than native errors (zero window → `ArgumentOutOfRangeException`).
7. **Unit tests** (NOOP, no GPU): validation + lifecycle. If the operation
   needs a display, cover the lifecycle in the integration suite instead and
   gate it on the environment.
8. **`BridgeInteropTests.AllEntryPointsResolveAndBehave`**: resolve the new
   P/Invoke through the raw ABI and assert its documented failure behavior —
   this test is the managed/native sync contract.

Then: build-native (8/8), managed suites, docs, and only then commit.

## 6. Adding a sample, a script, or a doc

* **Sample**: `samples/<Name>/` with `<Name>.csproj` (copy
  `BasicRendering.csproj` — `AllowUnsafeBlocks`, `IsAotCompatible`,
  `PublishAot` driven by `publish-aot.sh`), add to `FilamentSharp.sln`
  (project + configs + nested under `samples`), extend
  `scripts/publish-aot.sh` (publish + bundle + smoke; gate smoke runs on what
  they genuinely need — e.g. `DISPLAY` for windowed — and *say so* when
  skipped). WindowedRendering is the template for anything display-bound.
* **Script**: must be idempotent, safe without root, fail loud with a
  remediation hint, and source `scripts/env.sh` first. Document it in the
  relevant doc (`building.md` / `testing.md` / here).
* **Doc**: cross-link from `README.md`. The docs live and die with the code —
  update them in the same feature commit when behavior changes.

## 7. Dependency updates (bumping the Filament pin)

1. Edit `deps/filament.lock`: new version, commit SHA (from the upstream
   tag), release URL, and recompute SHA-256 + byte size from your download.
2. `./scripts/fetch-filament.sh` (fresh `external/`), then the full §3
   verification matrix. Expect upstream behavior changes to surface here —
   e.g. v1.78 terminates on a missing X server / missing ICD; the next
   release may convert those into error returns, which would let the bridge
   drop its warnings.
3. Update `docs/api-coverage.md` (the pinned release is part of every claim)
   and the version expectations in `tests/native/CMakeLists.txt`
   (`FS_EXPECTED_FILAMENT_VERSION` comes from the lock file automatically).
4. Record the re-verification results in `implementation-plan.md`.

## 8. Packaging the repository for delivery

The delivery contract: a ZIP in the *parent* directory of the repo, containing
the full working tree **including `.git`** and the complete commit history —
never `git archive` (that strips `.git`). The verified recipe:

```bash
# 0. Only package a clean, verified tree.
git status --short            # must be empty
git fsck --full               # history integrity

# 1. Stage: tracked files + .git under a FilamentSharp/ root.
STAGE=$(mktemp -d); DELIVERY_DIR=$(pwd)/..     # zip lands next to the repo
mkdir -p "$STAGE/FilamentSharp"
git ls-files -z | tar --null -T - -cf - | tar -xf - -C "$STAGE/FilamentSharp"
cp -a .git "$STAGE/FilamentSharp/"

# 2. Zip the staged tree (the archive therefore contains FilamentSharp/...).
(cd "$STAGE" && zip -r -q "$DELIVERY_DIR/FilamentSharp-repo.zip" FilamentSharp)
rm -rf "$STAGE"

# 3. Verify the archive by extracting it elsewhere and checking it equals the repo:
#    - git log --oneline            → full history (same commits, same identity)
#    - git status --short           → clean
#    - git ls-files | sort  vs.  unzip -l | extracted listing   → byte-identical sets
#    - re-run the §3 matrix from the extracted copy (this is how every
#      delivery claim in the worklog was made)
```

The invariant: **the zip equals the repo** — history, identity, and every
tracked file — and a stranger with a clean checkout of the *archive* can
reproduce the whole verification matrix from it.

## 9. Common failure modes

| Failure | Cause | Fix |
|---|---|---|
| Linker appears to hang on the SDK archives | GNU ld + static filament archives | Install lld; the CMake glue picks it up automatically and links in ~1 s |
| `cannot find -lc++` / `std::__1` undefined at load | libc++ mismatch with the SDK's ABI | Build the bridge with clang + libc++ (`env.local.sh` template) |
| Native tests abort with `createVkInstance ... error=-9` | No Vulkan ICD visible to ctest | `source scripts/env.sh` (or `setup-headless-vulkan.sh`); ctest inherits the environment from the invoking shell |
| Managed tests cannot find `libFilamentSharp.Native.so` | `build-native.sh` not run yet | The library resolver explains the search order; run the builder first |
| `AOT publish` fails to cross-link | No C linker on PATH | Any clang/gcc works; it links the generated object files |
| Sample writes PPM/output into the repo root | Running samples from the root without an output dir | Ignored (`*.ppm`); prefer passing an output directory like `publish-aot.sh` does |
| Windowed sample exits 1 instantly | No `$DISPLAY` / X server unreachable | Start a display (see `windowed-rendering.md` §2) |
