using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Source-generated P/Invoke bindings for the native bridge. Every entry
/// point declared here must exist in <c>filamentsharp/abi.h</c>; the
/// integration test suite verifies the full set resolves at runtime.
///
/// This type is internal on purpose: the public API is the idiomatic wrapper
/// layer, never raw pointers.
/// </summary>
internal static unsafe partial class Native
{
    static Native()
    {
        // Loading must happen before any P/Invoke call; the static constructor
        // is guaranteed to run first (including under Native AOT).
        NativeLibraryResolver.EnsureLoaded();
    }

    private const string LibraryName = "FilamentSharp.Native";

    // -------------------------------------------------------------- versioning
    //
    // NOTE: functions returning bridge-owned `const char*` are declared as
    // returning IntPtr and copied with Marshal.PtrToStringUTF8. A
    // `[return: MarshalAs(LPUTF8Str)] string` would make the marshaller call
    // CoTaskMemFree on the returned pointer, corrupting the heap, because it
    // assumes the callee allocated the string with CoTaskMemAlloc.

    [LibraryImport(LibraryName, EntryPoint = "fs_abi_version")]
    internal static partial uint AbiVersion();

    [LibraryImport(LibraryName, EntryPoint = "fs_filament_version")]
    internal static partial IntPtr FilamentVersionPtr();

    [LibraryImport(LibraryName, EntryPoint = "fs_filament_commit")]
    internal static partial IntPtr FilamentCommitPtr();

    // ------------------------------------------------------------- error model

    [LibraryImport(LibraryName, EntryPoint = "fs_get_last_error")]
    internal static partial IntPtr GetLastErrorPtr();

    [LibraryImport(LibraryName, EntryPoint = "fs_clear_last_error")]
    internal static partial void ClearLastError();

    // ----------------------------------------------------------------- engine

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_create")]
    internal static partial IntPtr EngineCreate(int backend);

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_destroy")]
    internal static partial void EngineDestroy(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_get_backend")]
    internal static partial int EngineGetBackend(IntPtr engine);

    // ------------------------------------------------------------ string helpers

    /// <summary>
    /// Copies a bridge-owned UTF-8 string. Never throws, never frees: the
    /// bridge keeps ownership of the buffer (valid until the next bridge call
    /// on the same thread).
    /// </summary>
    internal static string PtrToString(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
        {
            return string.Empty;
        }
        return Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
    }

    internal static string FilamentVersion() => PtrToString(FilamentVersionPtr());

    internal static string FilamentCommit() => PtrToString(FilamentCommitPtr());

    internal static string GetLastError() => PtrToString(GetLastErrorPtr());
}
