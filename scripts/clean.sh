#!/usr/bin/env bash
# Remove all generated build outputs (native + managed). Downloaded
# dependencies under external/ (Filament SDK, Vulkan runtime) are kept;
# pass --all to remove those too.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

rm -rf "$ROOT/external/native-build" \
       "$ROOT/external/native-install" \
       "$ROOT/external/verify-sdk"

if [ "${1:-}" = "--all" ]; then
    rm -rf "$ROOT/external"
    mkdir -p "$ROOT/external"
    touch "$ROOT/external/.gitkeep"
    echo "clean: removed all external dependencies and build outputs"
else
    echo "clean: removed native build outputs (kept external dependencies)"
fi

dotnet clean "$ROOT/FilamentSharp.sln" >/dev/null 2>&1 || true
echo "clean: done"
