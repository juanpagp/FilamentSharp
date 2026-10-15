namespace FilamentSharp;

/// <summary>Swap chain configuration flags (bitmask).</summary>
/// <remarks>
/// Numeric values mirror <c>backend::SWAP_CHAIN_CONFIG_*</c> at the pinned
/// Filament release and are part of the ABI contract.
/// </remarks>
[Flags]
public enum SwapChainConfig : ulong
{
    /// <summary>No flags.</summary>
    None = 0,

    /// <summary>Requests an alpha channel in the swap chain.</summary>
    Transparent = 0x1,

    /// <summary>The swap chain can be used as a source for readback/blit.</summary>
    Readable = 0x2,

    /// <summary>Enables XCB usage on Linux (windowed swap chains).</summary>
    EnableXcb = 0x4,

    /// <summary>Apple CVPixelBuffer-backed swap chain (not supported here).</summary>
    AppleCvPixelBuffer = 0x8,

    /// <summary>Uses the sRGB color space for the swap chain.</summary>
    SrgbColorSpace = 0x10,

    /// <summary>Allocates a stencil buffer.</summary>
    HasStencilBuffer = 0x20,

    /// <summary>Protected (DRM) content (not supported here).</summary>
    ProtectedContent = 0x40,

    /// <summary>4x multisample antialiasing.</summary>
    Msaa4Samples = 0x80,
}

/// <summary>
/// A swap chain: the presentable surface a <see cref="Renderer"/> renders
/// into. Two flavors exist:
/// <list type="bullet">
/// <item><b>Headless</b> (offscreen), created with
/// <see cref="Engine.CreateHeadlessSwapChain"/>: no window system is
/// involved; retrieve the result with
/// <see cref="Renderer.ReadPixels(uint, uint, uint, uint)"/>.</item>
/// <item><b>Native</b> (windowed), created with
/// <see cref="Engine.CreateNativeSwapChain"/>: bound to an OS window handle
/// (on Linux the X11 <c>Window</c> id). Frames present into that window; the
/// application owns the window and its event loop — see the
/// WindowedRendering sample and docs/windowed-rendering.md.</item>
/// </list>
/// </summary>
public sealed class SwapChain : FilamentObject
{
    internal SwapChain(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }
}
