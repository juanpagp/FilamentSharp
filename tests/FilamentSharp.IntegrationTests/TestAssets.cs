namespace FilamentSharp.IntegrationTests;

/// <summary>
/// Locates build-time artifacts (matc-compiled material packages) produced by
/// scripts/build-native.sh.
/// </summary>
internal static class TestAssets
{
    public static byte[] LoadMaterial(string name)
    {
        var baseDir = AppContext.BaseDirectory;
        var dir = baseDir;
        for (int i = 0; i < 8; i++)
        {
            var parent = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
            if (string.IsNullOrEmpty(parent))
            {
                break;
            }
            dir = parent;
            var candidate = Path.Combine(dir, "external", "native-install", "materials",
                name + ".filamat");
            if (File.Exists(candidate))
            {
                return File.ReadAllBytes(candidate);
            }
        }
        throw new FileNotFoundException(
            $"Material package '{name}.filamat' not found. Run scripts/build-native.sh " +
            "to compile the material assets before running the tests.");
    }
}
