# Third-party notices

FilamentSharp is MIT-licensed (see LICENSE). It links against and
redistributes interfaces from the following third-party projects.

## Google Filament — Apache License 2.0

The native bridge is compiled against and statically links the official
Google Filament release SDK (pinned in `deps/filament.lock` to v1.78.0,
commit `c1dc4e444784850d6dc891a1fb38747c30b85463`). Filament is distributed
under the Apache License, Version 2.0:

```
Copyright (C) 2018 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

The full license text ships inside the SDK archive
(`external/filament-sdk/LICENSE` after `scripts/fetch-filament.sh`).

### Filament components used, directly or transitively

| Component | Use |
|---|---|
| `filament`, `backend`, `filabridge`, `filaflat`, `shaders`, `geometry`, `utils`, `ibl` | core engine, backends, math/utils |
| `gltfio` (+ `gltfio_core`) | glTF 2.0 loading, ubershader materials |
| `uberarchive` | precompiled glTF ubershader material archive |
| `stb` (via gltfio) | PNG/JPEG decoding for embedded glTF textures |
| `dracodec` (Draco) | compressed glTF geometry (gltfio dependency) |
| `uberzlib` | ubershader archive parsing |
| `meshoptimizer` | glTF mesh processing (gltfio dependency) |
| `bluegl`, `bluevk`, `smol-v`, `webgpu_dawn`, `zstd` | backend platform layers |

Third-party notices for Filament's own dependencies are contained in the
Filament repository and release archives.

## Mesa / llvmpipe — MIT

`scripts/setup-headless-vulkan.sh` installs a user-local Mesa Vulkan ICD
(llvmpipe, software rasterizer) from the system's Debian packages for
headless rendering. Mesa is distributed under the MIT/X11 license (and, for
some components, LGPL). No Mesa code is linked into FilamentSharp
artifacts; the ICD is loaded at runtime by the Vulkan loader when tests or
the sample request a Vulkan device.

## .NET — MIT

.NET (including Native AOT) is MIT-licensed by the .NET Foundation.
FilamentSharp targets `net10.0` and consumes only BCL APIs.
