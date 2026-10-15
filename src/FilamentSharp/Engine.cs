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
