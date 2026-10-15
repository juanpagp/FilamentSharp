using System.Reflection;

namespace FilamentSharp;

/// <summary>Build-time information about the FilamentSharp managed library.</summary>
public static class FilamentSharpInfo
{
    /// <summary>Gets the target framework identifier this library was compiled for.</summary>
    public static string TargetFramework { get; } = AppContext.TargetFrameworkName ?? string.Empty;

    /// <summary>Gets the version of the FilamentSharp assembly.</summary>
    public static string Version { get; } = typeof(FilamentSharpInfo).Assembly.GetName().Version?.ToString(3) ?? "0.0.0";
}
