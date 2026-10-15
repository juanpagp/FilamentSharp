#!/usr/bin/env bash
# Build the managed solution and run unit + integration tests.
# The integration tests load the real native bridge: build-native.sh must have
# run, and a working backend runtime (e.g. lavapipe via
# setup-headless-vulkan.sh) determines which rendering tests can run.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

if [ ! -f "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" ]; then
    echo "run-managed-tests: native bridge not found at $FILAMENTSHARP_NATIVE_DIR" >&2
    echo "  - run scripts/build-native.sh first" >&2
    exit 1
fi

cd "$ROOT"
dotnet build FilamentSharp.sln -c "${FILAMENTSHARP_MANAGED_CONFIG:-Debug}"
dotnet test FilamentSharp.sln -c "${FILAMENTSHARP_MANAGED_CONFIG:-Debug}" --no-build \
    --logger "console;verbosity=normal"
