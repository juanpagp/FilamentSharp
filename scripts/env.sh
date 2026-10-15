#!/usr/bin/env bash
# Central environment helper for FilamentSharp scripts.
# Source this file (do not execute):  source scripts/env.sh
#
# Resolution order for every override:
#   1. Already-set environment variable (honored as-is).
#   2. scripts/env.local.sh (Git-ignored, machine-specific; see env.local.sh.example).
#   3. Repo-relative defaults under external/ (Git-ignored, populated by scripts).

FILAMENTSHARP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Machine-specific overrides (optional).
if [ -z "${FILAMENTSHARP_NO_LOCAL_ENV:-}" ] && [ -f "$FILAMENTSHARP_ROOT/scripts/env.local.sh" ]; then
    # shellcheck disable=SC1091
    source "$FILAMENTSHARP_ROOT/scripts/env.local.sh"
fi

# Location of the pinned Filament release SDK (populated by fetch-filament.sh).
export FILAMENT_SDK_DIR="${FILAMENT_SDK_DIR:-$FILAMENTSHARP_ROOT/external/filament-sdk}"

# Location of the built native bridge shared library (populated by build-native.sh).
export FILAMENTSHARP_NATIVE_DIR="${FILAMENTSHARP_NATIVE_DIR:-$FILAMENTSHARP_ROOT/external/native-install/lib}"

# Headless software-Vulkan runtime (optional; populated by setup-headless-vulkan.sh
# for machines without a GPU/Vulkan ICD). Only applied when the user has not
# already configured a Vulkan driver.
if [ -z "${VK_ICD_FILENAMES:-}" ] && [ -f "$FILAMENTSHARP_ROOT/external/vulkan-runtime/lvp_icd_local.json" ]; then
    export VK_ICD_FILENAMES="$FILAMENTSHARP_ROOT/external/vulkan-runtime/lvp_icd_local.json"
    export LD_LIBRARY_PATH="$FILAMENTSHARP_ROOT/external/vulkan-runtime/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# C++ toolchain for the native bridge. The Filament release SDK is built with
# libc++, so a clang toolchain with libc++ is required. Override CXX (and,
# if needed, the libc++ include/library dirs) in scripts/env.local.sh when
# they are not on the default search paths.
export CXX="${CXX:-clang++}"

# Extra runtime library path (e.g. user-local libc++), applied after the
# optional Vulkan runtime above so local runtimes take precedence.
if [ -n "${FILAMENTSHARP_EXTRA_LD_LIBRARY_PATH:-}" ]; then
    export LD_LIBRARY_PATH="$FILAMENTSHARP_EXTRA_LD_LIBRARY_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
