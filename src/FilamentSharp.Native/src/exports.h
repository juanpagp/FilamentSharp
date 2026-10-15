// Symbol visibility control for the bridge. Everything is hidden by default
// (CMAKE_CXX_VISIBILITY_PRESET hidden + -Wl,--exclude-libs,ALL); only symbols
// marked FILAMENTSHARP_API are exported, keeping the ABI surface exactly the
// documented fs_* functions.
#ifndef FILAMENTSHARP_EXPORTS_H
#define FILAMENTSHARP_EXPORTS_H

#define FILAMENTSHARP_BUILDING 1
#include "filamentsharp/abi.h"

#endif // FILAMENTSHARP_EXPORTS_H
