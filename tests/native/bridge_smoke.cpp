// Minimal native smoke-test executable for the bridge: loads it through the
// normal dynamic linker (no dlopen), calls the foundation entry points, and
// prints a summary. Exit code 0 proves the bridge is loadable and callable
// in a plain native process.
#include <cstdio>

#include "filamentsharp/abi.h"

int main() {
    const unsigned abi = fs_abi_version();
    const char* version = fs_filament_version();
    const char* commit = fs_filament_commit();
    if (version == nullptr || commit == nullptr) {
        std::fprintf(stderr, "bridge_smoke: null version info\n");
        return 1;
    }
    std::printf("bridge_smoke: FilamentSharp ABI %u, Filament %s (%.8s...)\n",
                abi, version, commit);
    std::printf("bridge_smoke: OK\n");
    return 0;
}
