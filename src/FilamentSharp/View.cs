namespace FilamentSharp;

/// <summary>A view: binds a scene, a camera and a viewport for rendering.</summary>
public sealed class View : FilamentObject
{
    internal View(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Sets the scene rendered by this view. Passing <c>null</c> detaches
    /// the current scene.
    /// </summary>
    public void SetScene(Scene? scene)
    {
        ThrowIfDisposed();
        scene?.ThrowIfDisposed();
        Native.ViewSetScene(Engine.DangerousGetHandle(), RawHandle,
            scene?.RawHandle ?? IntPtr.Zero).Check(nameof(View.SetScene));
    }

    /// <summary>Sets the camera used to render this view.</summary>
    public void SetCamera(Camera camera)
    {
        ThrowIfDisposed();
        ArgumentNullException.ThrowIfNull(camera);
        camera.ThrowIfDisposed();
        Native.ViewSetCamera(Engine.DangerousGetHandle(), RawHandle, camera.RawHandle)
            .Check(nameof(View.SetCamera));
    }

    /// <summary>Sets the viewport in pixels (origin at the bottom-left corner).</summary>
    public void SetViewport(int left, int bottom, uint width, uint height)
    {
        ThrowIfDisposed();
        Native.ViewSetViewport(Engine.DangerousGetHandle(), RawHandle,
            left, bottom, width, height).Check(nameof(View.SetViewport));
    }
}

/// <summary>A scene: a container of renderable entities.</summary>
public sealed class Scene : FilamentObject
{
    internal Scene(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>Adds an entity to the scene.</summary>
    /// <param name="entity">The entity to add.</param>
    public void AddEntity(Entity entity)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        Native.SceneAddEntity(Engine.DangerousGetHandle(), RawHandle, entity.Id)
            .Check(nameof(Scene.AddEntity));
    }

    /// <summary>Removes an entity from the scene.</summary>
    /// <param name="entity">The entity to remove.</param>
    public void RemoveEntity(Entity entity)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        Native.SceneRemoveEntity(Engine.DangerousGetHandle(), RawHandle, entity.Id)
            .Check(nameof(Scene.RemoveEntity));
    }

    /// <summary>Gets a value indicating whether the scene contains the entity.</summary>
    /// <param name="entity">The entity to look for.</param>
    /// <returns><c>true</c> when the entity is in the scene.</returns>
    public bool HasEntity(Entity entity)
    {
        ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        Native.SceneHasEntity(Engine.DangerousGetHandle(), RawHandle, entity.Id, out int has)
            .Check(nameof(Scene.HasEntity));
        return has != 0;
    }

    /// <summary>Gets the number of entities currently in the scene.</summary>
    public uint EntityCount
    {
        get
        {
            ThrowIfDisposed();
            Native.SceneGetEntityCount(Engine.DangerousGetHandle(), RawHandle, out uint count)
                .Check(nameof(Scene.EntityCount));
            return count;
        }
    }
}
