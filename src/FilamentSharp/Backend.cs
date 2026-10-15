namespace FilamentSharp;

/// <summary>
/// Rendering backend selection. Values mirror the native <c>fs_backend</c>
/// enum, which mirrors <c>filament::backend::Backend</c> at the pinned
/// Filament release. The numeric values are part of the ABI contract.
/// </summary>
public enum Backend
{
    /// <summary>Let Filament choose (Vulkan on Linux at the pinned release).</summary>
    Default = 0,

    /// <summary>OpenGL backend. On Linux this requires a running X server (GLX).</summary>
    OpenGl = 1,

    /// <summary>Vulkan backend (primary desktop backend).</summary>
    Vulkan = 2,

    /// <summary>Metal backend (macOS/iOS only; not supported by this binding).</summary>
    Metal = 3,

    /// <summary>WebGPU backend (not supported by this binding).</summary>
    WebGpu = 4,

    /// <summary>No-op backend for testing: works without a GPU, produces no output.</summary>
    Noop = 5,
}
