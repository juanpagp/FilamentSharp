# FilamentSharp

Idiomatic, Native-AOT-compatible C# binding for [Google Filament](https://github.com/google/filament),
the real-time physically based rendering engine.

> Status: work in progress — see `docs/implementation-plan.md` for the plan and
> `docs/api-coverage.md` for the currently wrapped API surface.

## What is this?

FilamentSharp lets an ordinary C# application create and drive a Filament
rendering engine:

```csharp
using FilamentSharp;

using var engine = Engine.Create(Backend.OpenGl);
// ... renderer, swap chains, scenes, materials, glTF assets ...
```

The stack is:

```
C# application
  └─ FilamentSharp (net10.0, source-generated P/Invoke, SafeHandle ownership)
       └─ libFilamentSharp.Native.so (stable C ABI bridge, error model, ownership registry)
            └─ Google Filament v1.78.0 (official prebuilt release SDK, Apache-2.0)
                 └─ OpenGL/EGL (or NOOP test backend)
```

## Quick start (Linux x64)

```bash
# 0. Prerequisites: .NET SDK 10, CMake >= 3.20, Ninja, a C++17 compiler.
# 1. Fetch the pinned Filament release SDK (into external/, Git-ignored):
./scripts/fetch-filament.sh
# 2. Build the native bridge (CMake + Ninja) and run native tests:
./scripts/build-native.sh
# 3. Build everything managed and run the managed test suites:
./scripts/run-managed-tests.sh
# 4. Publish + run the sample as a Native AOT executable:
./scripts/publish-aot.sh
./artifacts/publish/linux-x64/BasicRendering
```

Full instructions: `docs/building.md` · architecture: `docs/architecture.md` ·
API coverage: `docs/api-coverage.md` · deployment: `docs/deployment.md`.

## License

FilamentSharp code is MIT licensed (see `LICENSE`). Google Filament is
Apache-2.0; third-party notices are in `docs/third-party-notices.md`.
