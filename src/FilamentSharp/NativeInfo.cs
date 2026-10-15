namespace FilamentSharp;

/// <summary>
/// Version information from the native bridge. Accessing any member loads
/// the native library (see <see cref="NativeLibraryResolver"/> for the search
/// order) and reports the versions of the bridge ABI and of the pinned
/// Filament release it was built against.
/// </summary>
public static class NativeInfo
{
    /// <summary>Gets the version of the FilamentSharp C ABI.</summary>
    public static uint AbiVersion
    {
        get
        {
            EnsureInitialized();
            return Native.AbiVersion();
        }
    }

    /// <summary>
    /// Gets the pinned Google Filament release version (for example
    /// <c>"v1.78.0"</c>).
    /// </summary>
    public static string FilamentVersion
    {
        get
        {
            EnsureInitialized();
            return Native.FilamentVersion();
        }
    }

    /// <summary>
    /// Gets the upstream tag commit (hex SHA-1) of the pinned Filament
    /// release.
    /// </summary>
    public static string FilamentCommit
    {
        get
        {
            EnsureInitialized();
            return Native.FilamentCommit();
        }
    }

    /// <summary>
    /// Gets the full path of the native bridge shared library that was loaded,
    /// or <c>null</c> when it has not been loaded yet.
    /// </summary>
    public static string? NativeLibraryPath => NativeLibraryResolver.ResolvedPath;

    private static void EnsureInitialized()
    {
        NativeLibraryResolver.EnsureLoaded();
    }
}
