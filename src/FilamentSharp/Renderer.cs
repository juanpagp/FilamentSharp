namespace FilamentSharp;

/// <summary>A renderer: owns a render loop and presents frames to a swap chain.</summary>
/// <remarks>
/// The frame loop is explicit:
/// <code>
/// if (renderer.BeginFrame(swapChain))
/// {
///     renderer.ReadPixels(0, 0, width, height);   // optional, before EndFrame
///     renderer.Render(view);
///     renderer.EndFrame();
/// }
/// byte[]? pixels = renderer.CompleteReadPixels();  // null until ready
/// </code>
/// </remarks>
public sealed class Renderer : FilamentObject
{
    private int _pendingReadbackSize;

    internal Renderer(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Sets the clear options used at the start of each frame.
    /// </summary>
    /// <param name="r">Red component of the clear color (linear).</param>
    /// <param name="g">Green component (linear).</param>
    /// <param name="b">Blue component (linear).</param>
    /// <param name="a">Alpha component (linear).</param>
    /// <param name="clear">Whether to clear at the start of the frame.</param>
    /// <param name="discard">Whether to discard the previous frame's content.</param>
    public void SetClearOptions(float r, float g, float b, float a, bool clear = true,
        bool discard = true)
    {
        ThrowIfDisposed();
        Native.RendererSetClearOptions(Engine.DangerousGetHandle(), RawHandle,
            r, g, b, a, clear ? 1 : 0, discard ? 1 : 0)
            .Check(nameof(Renderer.SetClearOptions));
    }

    /// <summary>
    /// Begins a frame on the given swap chain.
    /// </summary>
    /// <param name="swapChain">The swap chain to render into.</param>
    /// <returns>
    /// <c>true</c> when the frame was acquired (call
    /// <see cref="Render(View)"/> and <see cref="EndFrame"/>);
    /// <c>false</c> when the frame was skipped (backpressure).
    /// </returns>
    public bool BeginFrame(SwapChain swapChain)
    {
        ThrowIfDisposed();
        ArgumentNullException.ThrowIfNull(swapChain);
        swapChain.ThrowIfDisposed();
        Native.RendererBeginFrame(Engine.DangerousGetHandle(), RawHandle,
            swapChain.RawHandle, 0, out int beganFlag).Check(nameof(Renderer.BeginFrame));
        return beganFlag != 0;
    }

    /// <summary>Renders a view into the current frame.</summary>
    /// <param name="view">The view to render.</param>
    public void Render(View view)
    {
        ThrowIfDisposed();
        ArgumentNullException.ThrowIfNull(view);
        view.ThrowIfDisposed();
        Native.RendererRender(Engine.DangerousGetHandle(), RawHandle, view.RawHandle)
            .Check(nameof(Renderer.Render));
    }

    /// <summary>Ends the current frame and presents it.</summary>
    public void EndFrame()
    {
        ThrowIfDisposed();
        Native.RendererEndFrame(Engine.DangerousGetHandle(), RawHandle)
            .Check(nameof(Renderer.EndFrame));
    }

    /// <summary>
    /// Queues an RGBA8 readback of a region of the current swap chain. Call
    /// between <see cref="Render(View)"/> and <see cref="EndFrame"/>. Only
    /// one readback may be pending at a time; retrieve it with
    /// <see cref="CompleteReadPixels"/> after the frame ends.
    /// </summary>
    /// <param name="x">Left edge of the region (pixels).</param>
    /// <param name="y">Bottom edge of the region (pixels).</param>
    /// <param name="width">Width of the region (pixels).</param>
    /// <param name="height">Height of the region (pixels).</param>
    public void ReadPixels(uint x, uint y, uint width, uint height)
    {
        ThrowIfDisposed();
        if (width == 0 || height == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(width),
                "Readback size must be non-zero.");
        }
        Native.RendererReadPixels(Engine.DangerousGetHandle(), RawHandle, x, y, width, height)
            .Check(nameof(Renderer.ReadPixels));
        _pendingReadbackSize = unchecked((int)(width * height * 4));
    }

    /// <summary>
    /// Completes the pending readback (flushing the engine as needed) and
    /// returns the pixels as top-down RGBA rows... note that Filament's
    /// readback is bottom-up: row 0 in the returned buffer is the bottom
    /// row of the image.
    /// </summary>
    /// <returns>
    /// The RGBA8 pixel buffer (<c>width*height*4</c> bytes), or <c>null</c>
    /// when the readback is not ready yet (render another frame and retry).
    /// </returns>
    public byte[]? CompleteReadPixels()
    {
        ThrowIfDisposed();
        if (_pendingReadbackSize == 0)
        {
            throw new InvalidOperationException("No readback is pending; call ReadPixels first.");
        }

        var pixels = new byte[_pendingReadbackSize];
        var status = Native.RendererReadPixelsComplete(
            Engine.DangerousGetHandle(), RawHandle, pixels,
            (nuint)pixels.Length, out nuint written);

        if (status == NativeStatus.NotReady)
        {
            return null;
        }
        status.Check(nameof(Renderer.CompleteReadPixels));

        _pendingReadbackSize = 0;
        return pixels;
    }

    /// <summary>
    /// Convenience: renders frames until the pending readback completes and
    /// returns the pixels.
    /// </summary>
    /// <param name="swapChain">Swap chain to render into.</param>
    /// <param name="view">View to render.</param>
    /// <param name="width">Readback width in pixels (swap chain width).</param>
    /// <param name="height">Readback height in pixels (swap chain height).</param>
    /// <param name="maxFrames">Maximum number of frames to render.</param>
    /// <returns>The RGBA8 pixel buffer, or <c>null</c> when not ready in time.</returns>
    public byte[]? RenderUntilReadbackComplete(SwapChain swapChain, View view,
        uint width, uint height, int maxFrames = 8)
    {
        ArgumentNullException.ThrowIfNull(swapChain);
        ArgumentNullException.ThrowIfNull(view);
        for (int i = 0; i < maxFrames; i++)
        {
            if (BeginFrame(swapChain))
            {
                Render(view);
                ReadPixels(0, 0, width, height);
                EndFrame();
                var pixels = CompleteReadPixels();
                if (pixels != null)
                {
                    return pixels;
                }
            }
        }
        return null;
    }
}
