namespace FilamentSharp;

/// <summary>
/// A Filament rendering engine. The engine owns every other Filament resource
/// (renderer, swap chains, scenes, cameras, materials, buffers, lights...).
/// </summary>
/// <remarks>
/// <para>
/// Create with <see cref="Create(Backend)"/> and dispose when done. All
/// objects created from an engine must be disposed before the engine itself
/// is disposed; disposing the engine invalidates any remaining child objects
/// (their disposal becomes a safe no-op and their use throws).
/// </para>
/// <para>
/// <b>Threading:</b> the engine is not thread-safe. Use it (and everything
/// created from it) from a single thread, or synchronize externally. Disposal
/// is safe from any thread, including finalizers.
/// </para>
/// <para>
/// <b>Backend choice:</b> <see cref="Backend.Vulkan"/> is the primary desktop
/// backend. <see cref="Backend.OpenGl"/> requires a running X server on Linux
/// (upstream Filament terminates the process when no display is available at
/// the pinned release — choose deliberately). <see cref="Backend.Noop"/> runs
/// without a GPU and produces no output, which is useful for tests.
/// </para>
/// </remarks>
public sealed class Engine : IDisposable
{
    private readonly EngineSafeHandle _handle;

    private Engine(EngineSafeHandle handle)
    {
        _handle = handle;
    }

    /// <summary>
    /// Gets a value indicating whether this engine has been disposed.
    /// </summary>
    public bool IsDisposed => _handle.IsClosed;

    /// <summary>
    /// Gets the backend actually in use by this engine. Meaningful after
    /// creation, in particular when the engine was created with
    /// <see cref="Backend.Default"/>.
    /// </summary>
    /// <exception cref="ObjectDisposedException">The engine is disposed.</exception>
    public Backend ActiveBackend
    {
        get
        {
            ThrowIfDisposed();
            var backend = Native.EngineGetBackend(_handle.DangerousGetHandle());
            if (backend == (int)Backend.Default)
            {
                throw ThrowHelper.NativeFailure("Engine.ActiveBackend");
            }
            return (Backend)backend;
        }
    }

    /// <summary>
    /// Creates a new Filament engine.
    /// </summary>
    /// <param name="backend">The backend to use; <see cref="Backend.Default"/> lets Filament choose.</param>
    /// <returns>A new engine. Dispose it when done.</returns>
    /// <exception cref="FilamentSharpException">
    /// The engine could not be created (for example, the requested graphics
    /// backend is not available). The message contains the native error text.
    /// </exception>
    public static Engine Create(Backend backend = Backend.Default)
    {
        var raw = Native.EngineCreate((int)backend);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure($"Engine.Create({backend})");
        }
        return new Engine(new EngineSafeHandle(raw));
    }

    /// <summary>
    /// Attempts to create an engine with the given backend without throwing
    /// when the backend is unavailable.
    /// </summary>
    /// <param name="backend">The backend to use.</param>
    /// <param name="engine">
    /// The created engine, or <c>null</c> when creation failed.
    /// </param>
    /// <returns><c>true</c> when the engine was created.</returns>
    public static bool TryCreate(Backend backend, out Engine? engine)
    {
        try
        {
            engine = Create(backend);
            return true;
        }
        catch (FilamentSharpException)
        {
            engine = null;
            return false;
        }
    }

    /// <summary>
    /// Destroys the engine and every resource it owns. Idempotent. After
    /// disposal, all child objects created from this engine are invalidated.
    /// </summary>
    public void Dispose()
    {
        // SafeHandle.Dispose is idempotent; the native destroy is too.
        _handle.Dispose();
    }

    // ------------------------------------------------------------------
    // Resource factories. Children are registered with the native ownership
    // registry at creation; their SafeHandles release through the matching
    // fs_*_destroy entry point, which is safe even after engine disposal.
    // ------------------------------------------------------------------

    /// <summary>Creates a renderer owned by this engine.</summary>
    public Renderer CreateRenderer()
    {
        ThrowIfDisposed();
        var raw = Native.RendererCreate(DangerousGetHandle());
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateRenderer");
        }
        return new Renderer(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.RendererDestroy(engine, handle); }));
    }

    /// <summary>
    /// Creates a headless (offscreen) swap chain owned by this engine.
    /// </summary>
    /// <param name="width">Width in pixels (non-zero).</param>
    /// <param name="height">Height in pixels (non-zero).</param>
    /// <param name="flags">Optional configuration flags.</param>
    public SwapChain CreateHeadlessSwapChain(uint width, uint height,
        SwapChainConfig flags = SwapChainConfig.None)
    {
        ThrowIfDisposed();
        if (width == 0 || height == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(width),
                "Swap chain dimensions must be non-zero.");
        }
        var raw = Native.SwapChainCreateHeadless(DangerousGetHandle(), width, height,
            (ulong)flags);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateHeadlessSwapChain");
        }
        return new SwapChain(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.SwapChainDestroy(engine, handle); }));
    }

    /// <summary>Creates a view owned by this engine.</summary>
    public View CreateView()
    {
        ThrowIfDisposed();
        var raw = Native.ViewCreate(DangerousGetHandle());
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateView");
        }
        return new View(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.ViewDestroy(engine, handle); }));
    }

    /// <summary>Creates a scene owned by this engine.</summary>
    public Scene CreateScene()
    {
        ThrowIfDisposed();
        var raw = Native.SceneCreate(DangerousGetHandle());
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateScene");
        }
        return new Scene(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.SceneDestroy(engine, handle); }));
    }

    /// <summary>
    /// Creates a camera component on the given entity. The entity must be
    /// valid (see <see cref="EntityManager.Create"/>).
    /// </summary>
    public Camera CreateCamera(Entity entity)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        var raw = Native.CameraCreate(DangerousGetHandle(), entity.Id);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateCamera");
        }
        return new Camera(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.CameraDestroy(engine, handle); }), entity);
    }

    internal IntPtr DangerousGetHandle()
    {
        ThrowIfDisposed();
        return _handle.DangerousGetHandle();
    }

    internal void ThrowIfDisposed()
    {
        if (_handle.IsClosed)
        {
            throw new ObjectDisposedException(nameof(Engine),
                "The Filament engine has been disposed. All objects created " +
                "from it are invalidated and must be recreated with a new engine.");
        }
    }
}

internal static class ThrowHelper
{
    internal static FilamentSharpException NativeFailure(string operation)
    {
        var message = Native.GetLastError();
        if (string.IsNullOrEmpty(message))
        {
            message = "unknown native error";
        }
        return new FilamentSharpException($"{operation} failed: {message}");
    }
}
