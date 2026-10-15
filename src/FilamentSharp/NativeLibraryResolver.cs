using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Locates and loads the native bridge shared library
/// (<c>libFilamentSharp.Native.so</c>).
///
/// Search order (all lookups use explicit full paths; the resolver never
/// loads from the current working directory and never relies on OS library
/// search paths, which would be unsafe in the presence of writable
/// directories):
///   1. The directory named by the <c>FILAMENTSHARP_NATIVE_DIR</c>
///      environment variable (exact directory).
///   2. The application base directory (the normal layout for published,
///      self-contained applications).
///   3. Walking up from the application base directory for up to eight
///      levels, looking for <c>external/native-install/lib</c> — the layout
///      of a development checkout after <c>scripts/build-native.sh</c>.
/// </summary>
internal static class NativeLibraryResolver
{
    private const string LibraryName = "FilamentSharp.Native";

    private static volatile bool _loaded;
    private static readonly object LoadLock = new();

    internal static string? ResolvedPath { get; private set; }

    /// <summary>
    /// Ensures the native bridge is loaded into the process. Called lazily by
    /// the static constructor of <see cref="Native"/>. Thread-safe; the first
    /// successful call wins.
    /// </summary>
    /// <exception cref="FilamentSharpException">
    /// Thrown when the library cannot be found in any documented location.
    /// The exception lists every searched path.
    /// </exception>
    public static void EnsureLoaded()
    {
        if (_loaded)
        {
            return;
        }

        lock (LoadLock)
        {
            if (_loaded)
            {
                return;
            }

            var candidates = new List<string>();
            var fileName = GetLibraryFileName();

            // 1. Explicit environment override.
            var envDir = Environment.GetEnvironmentVariable("FILAMENTSHARP_NATIVE_DIR");
            if (!string.IsNullOrEmpty(envDir))
            {
                candidates.Add(Path.Combine(envDir, fileName));
            }

            // 2. Application base directory.
            string baseDir = AppContext.BaseDirectory;
            candidates.Add(Path.Combine(baseDir, fileName));

            // NuGet runtime-specific native asset location.
            candidates.Add(Path.Combine(
                baseDir,
                "runtimes",
                RuntimeInformation.RuntimeIdentifier,
                "native",
                fileName));

            // 3. Development checkout layout, walking up from the base directory.
            var dir = baseDir;
            for (int i = 0; i < 8; i++)
            {
                var parent = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
                if (string.IsNullOrEmpty(parent))
                {
                    break;
                }
                dir = parent;
                candidates.Add(Path.Combine(dir, "external", "native-install", "lib", fileName));
            }

            foreach (var candidate in candidates)
            {
                if (File.Exists(candidate))
                {
                    try
                    {
                        NativeLibrary.Load(candidate);
                        ResolvedPath = candidate;
                        _loaded = true;
                        return;
                    }
                    catch (Exception ex)
                    {
                        // Record the failure reason and keep trying the remaining
                        // candidates; a corrupt file in one location should not
                        // prevent a valid copy elsewhere from loading.
                        Environment.SetEnvironmentVariable(
                            "FILAMENTSHARP_LAST_LOAD_ERROR", ex.Message);
                    }
                }
            }

            throw new FilamentSharpException(
                $"Could not load the FilamentSharp native bridge ({fileName}). " +
                "Searched:\n  " + string.Join("\n  ", candidates) +
                "\nSet FILAMENTSHARP_NATIVE_DIR to the directory containing the bridge, " +
                "or run scripts/build-native.sh in the repository.");
        }
    }

    private static string GetLibraryFileName() => OperatingSystem.IsLinux() || OperatingSystem.IsFreeBSD()
        ? $"lib{LibraryName}.so"
        : OperatingSystem.IsWindows()
            ? $"{LibraryName}.dll"
            : OperatingSystem.IsMacOS()
                ? $"lib{LibraryName}.dylib"
                : throw new FilamentSharpException("Unsupported operating system for the native bridge.");
}
