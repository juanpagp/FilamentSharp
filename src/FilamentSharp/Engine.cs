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

    /// <summary>Creates a swap chain that presents into an OS window.</summary>
    /// <param name="nativeWindow">
    /// The platform window handle. On Linux this is the X11 <c>Window</c> id
    /// (XID) — the same value XCreateWindow/XCreateSimpleWindow returned,
    /// passed as <see cref="IntPtr"/>. Filament derives the surface from it
    /// (Vulkan: <c>VkSurfaceKHR</c> through XCB; OpenGL: the GLX drawable).
    /// </param>
    /// <param name="flags">Swap chain configuration flags.</param>
    /// <returns>A swap chain bound to the window. Dispose it and create a new
    /// one (same <paramref name="nativeWindow"/>) when the window is resized,
    /// outside any begin/end frame pair.</returns>
    /// <exception cref="ArgumentOutOfRangeException">
    /// <paramref name="nativeWindow"/> is zero.</exception>
    /// <exception cref="FilamentSharpException">Surface creation failed.</exception>
    /// <remarks>
    /// The X server must be reachable through <c>DISPLAY</c>. See the
    /// WindowedRendering sample and docs/windowed-rendering.md.
    /// </remarks>
    public SwapChain CreateNativeSwapChain(IntPtr nativeWindow,
        SwapChainConfig flags = SwapChainConfig.None)
    {
        ThrowIfDisposed();
        if (nativeWindow == IntPtr.Zero)
        {
            throw new ArgumentOutOfRangeException(nameof(nativeWindow),
                "The native window handle must be non-zero (the X11 Window id).");
        }
        var raw = Native.SwapChainCreateNative(DangerousGetHandle(), nativeWindow,
            (ulong)flags);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateNativeSwapChain");
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

    /// <summary>
    /// Creates a vertex buffer with the given attribute declarations.
    /// </summary>
    /// <param name="vertexCount">Number of vertices.</param>
    /// <param name="bufferCount">Number of separate data buffers.</param>
    /// <param name="attributes">Attribute declarations (at least one).</param>
    public VertexBuffer CreateVertexBuffer(uint vertexCount, uint bufferCount,
        ReadOnlySpan<VertexAttributeDescription> attributes)
    {
        ThrowIfDisposed();
        if (vertexCount == 0 || bufferCount == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(vertexCount),
                "Vertex count and buffer count must be non-zero.");
        }
        if (attributes.IsEmpty)
        {
            throw new ArgumentException("At least one attribute is required.",
                nameof(attributes));
        }
        var raw = Native.VertexBufferCreate(DangerousGetHandle(), vertexCount,
            bufferCount, attributes, (uint)attributes.Length);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateVertexBuffer");
        }
        return new VertexBuffer(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.VertexBufferDestroy(engine, handle); }));
    }

    /// <summary>Creates an index buffer.</summary>
    /// <param name="indexCount">Number of indices.</param>
    /// <param name="indexType">16-bit or 32-bit indices.</param>
    public IndexBuffer CreateIndexBuffer(uint indexCount, IndexType indexType)
    {
        ThrowIfDisposed();
        if (indexCount == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(indexCount),
                "Index count must be non-zero.");
        }
        var raw = Native.IndexBufferCreate(DangerousGetHandle(), indexCount,
            (int)indexType);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateIndexBuffer");
        }
        return new IndexBuffer(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.IndexBufferDestroy(engine, handle); }));
    }

    /// <summary>
    /// Sets an entity's local transform. The transform component is created
    /// implicitly when the entity does not have one yet.
    /// </summary>
    /// <param name="entity">The entity to move.</param>
    /// <param name="transform">The local transform (row-vector convention of
    /// System.Numerics; translated to Filament's column-major layout).</param>
    public void SetTransform(Entity entity, in System.Numerics.Matrix4x4 transform)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        Span<float> columnMajor = stackalloc float[16];
        MatrixInterop.ToColumnMajor(transform, columnMajor);
        Native.TransformSetMatrix(DangerousGetHandle(), entity.Id, columnMajor)
            .Check(nameof(Engine.SetTransform));
    }

    /// <summary>Gets an entity's world (parent-composed) transform.</summary>
    /// <param name="entity">The entity.</param>
    /// <returns>The world transform.</returns>
    /// <exception cref="FilamentSharpException">
    /// The entity has no transform component (create one with
    /// <see cref="SetTransform"/> first).
    /// </exception>
    public System.Numerics.Matrix4x4 GetWorldTransform(Entity entity)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        Span<float> columnMajor = stackalloc float[16];
        Native.TransformGetWorldMatrix(DangerousGetHandle(), entity.Id, columnMajor)
            .Check(nameof(Engine.GetWorldTransform));
        return MatrixInterop.FromColumnMajor(columnMajor);
    }

    /// <summary>
    /// Parents one entity under another. Pass <c>null</c> to detach the child
    /// from the hierarchy.
    /// </summary>
    /// <param name="child">The child entity (must have a transform).</param>
    /// <param name="parent">The parent entity, or <c>null</c> to detach.</param>
    public void SetParent(Entity child, Entity? parent)
    {
        ThrowIfDisposed();
        if (!child.IsValid)
        {
            throw new ArgumentException("A valid child entity is required.", nameof(child));
        }
        if (parent is { } p && !p.IsValid)
        {
            throw new ArgumentException("Parent entity is invalid.", nameof(parent));
        }
        Native.TransformSetParent(DangerousGetHandle(), child.Id,
            parent?.Id ?? 0).Check(nameof(Engine.SetParent));
    }

    /// <summary>
    /// Loads a matc-compiled material package (.filamat bytes) into the engine.
    /// </summary>
    /// <param name="package">The .filamat package bytes.</param>
    /// <returns>The loaded material.</returns>
    public Material CreateMaterial(ReadOnlySpan<byte> package)
    {
        ThrowIfDisposed();
        if (package.IsEmpty)
        {
            throw new ArgumentException("Material package must not be empty.", nameof(package));
        }
        var raw = Native.MaterialCreate(DangerousGetHandle(), package, (nuint)package.Length);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateMaterial");
        }
        return new Material(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.MaterialDestroy(engine, handle); }));
    }

    /// <summary>Creates a 2D texture.</summary>
    /// <param name="width">Width in pixels.</param>
    /// <param name="height">Height in pixels.</param>
    /// <param name="levels">Number of mip levels (1 for no mipmaps).</param>
    /// <param name="format">Internal format.</param>
    /// <returns>The new texture.</returns>
    public Texture CreateTexture2D(uint width, uint height, uint levels, TextureFormat format)
    {
        ThrowIfDisposed();
        if (width == 0 || height == 0 || levels == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(width),
                "Texture dimensions and levels must be non-zero.");
        }
        var raw = Native.TextureCreate2d(DangerousGetHandle(), width, height, levels,
            (int)format);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateTexture2D");
        }
        return new Texture(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.TextureDestroy(engine, handle); }));
    }

    /// <summary>
    /// Creates a glTF asset loader. Materials come from Filament's
    /// precompiled ubershader archive (no runtime shader generation).
    /// </summary>
    /// <returns>The new loader.</returns>
    public GltfLoader CreateGltfLoader()
    {
        ThrowIfDisposed();
        var raw = Native.GltfLoaderCreate(DangerousGetHandle());
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Engine.CreateGltfLoader");
        }
        return new GltfLoader(this, new ChildSafeHandle(raw, DangerousGetHandle(),
            (engine, handle) => { _ = Native.GltfLoaderDestroy(engine, handle); }));
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
