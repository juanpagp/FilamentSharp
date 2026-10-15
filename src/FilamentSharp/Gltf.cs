using System.Numerics;

namespace FilamentSharp;

/// <summary>
/// Loads glTF 2.0 content (self-contained GLB files with embedded buffers)
/// into <see cref="GltfAsset"/> objects. Materials come from Filament's
/// precompiled ubershader archive, so loading performs no runtime shader
/// generation. Create with <see cref="Engine.CreateGltfLoader"/>.
/// </summary>
public sealed class GltfLoader : FilamentObject
{
    internal GltfLoader(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Parses and loads a self-contained GLB (or a JSON glTF with embedded
    /// data-URI buffers). The buffer is copied by the bridge; the caller may
    /// reuse it afterwards.
    /// </summary>
    /// <param name="content">The glTF/GLB bytes.</param>
    /// <returns>The loaded asset.</returns>
    /// <exception cref="ArgumentException"><paramref name="content"/> is empty.</exception>
    /// <exception cref="FilamentSharpException">
    /// The content is not valid glTF 2.0, or its resource data is missing or
    /// corrupt.
    /// </exception>
    public GltfAsset Load(ReadOnlySpan<byte> content)
    {
        ThrowIfDisposed();
        if (content.IsEmpty)
        {
            throw new ArgumentException("glTF content must not be empty.", nameof(content));
        }
        var raw = Native.GltfLoaderLoad(Engine.DangerousGetHandle(), RawHandle,
            content, (nuint)content.Length);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("GltfLoader.Load");
        }
        // The loader outlives the asset in native teardown order (LIFO), so
        // the asset's release can safely call into the loader.
        return new GltfAsset(Engine,
            new ChildSafeHandle(raw, Engine.DangerousGetHandle(),
                (engine, handle) => { _ = Native.GltfAssetDestroy(engine, handle); }));
    }
}

/// <summary>
/// A loaded glTF scene: entities (transform hierarchy + renderables) that can
/// be added to a <see cref="Scene"/>. Create with
/// <see cref="GltfLoader.Load(ReadOnlySpan{byte})"/>.
/// </summary>
public sealed class GltfAsset : FilamentObject
{
    internal GltfAsset(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>Gets the number of entities in the asset's hierarchy.</summary>
    public int EntityCount
    {
        get
        {
            ThrowIfDisposed();
            Native.GltfAssetGetEntityCount(Engine.DangerousGetHandle(), RawHandle,
                out uint count).Check(nameof(GltfAsset.EntityCount));
            return checked((int)count);
        }
    }

    /// <summary>
    /// Gets the asset's entities (creation order). Add them to a scene with
    /// <see cref="Scene.AddEntity"/>.
    /// </summary>
    public Entity[] GetEntities()
    {
        ThrowIfDisposed();
        Native.GltfAssetGetEntityCount(Engine.DangerousGetHandle(), RawHandle,
            out uint count).Check(nameof(GltfAsset.GetEntities));
        var ids = new uint[count];
        if (count > 0)
        {
            Native.GltfAssetGetEntities(Engine.DangerousGetHandle(), RawHandle,
                ids, count, out uint written).Check(nameof(GltfAsset.GetEntities));
            if (written != count)
            {
                throw ThrowHelper.NativeFailure("GltfAsset.GetEntities");
            }
        }
        var entities = new Entity[count];
        for (int i = 0; i < ids.Length; i++)
        {
            entities[i] = new Entity(ids[i]);
        }
        return entities;
    }

    /// <summary>Gets the scene-graph root entity of the asset.</summary>
    public Entity Root
    {
        get
        {
            ThrowIfDisposed();
            Native.GltfAssetGetRoot(Engine.DangerousGetHandle(), RawHandle,
                out uint root).Check(nameof(GltfAsset.Root));
            return new Entity(root);
        }
    }

    /// <summary>Gets the number of renderable entities in the asset.</summary>
    public int RenderableCount
    {
        get
        {
            ThrowIfDisposed();
            Native.GltfAssetGetRenderableCount(Engine.DangerousGetHandle(),
                RawHandle, out uint count).Check(nameof(GltfAsset.RenderableCount));
            return checked((int)count);
        }
    }

    /// <summary>
    /// Gets the asset's axis-aligned bounding box (world space of the loaded
    /// hierarchy, before any transform is applied to the root).
    /// </summary>
    public (Vector3 Min, Vector3 Max) BoundingBox
    {
        get
        {
            ThrowIfDisposed();
            unsafe
            {
                float* min = stackalloc float[3];
                float* max = stackalloc float[3];
                Native.GltfAssetGetBoundingBox(Engine.DangerousGetHandle(), RawHandle,
                    min, max).Check(nameof(GltfAsset.BoundingBox));
                return (new Vector3(min[0], min[1], min[2]),
                        new Vector3(max[0], max[1], max[2]));
            }
        }
    }

    /// <summary>Gets the number of animations in the asset.</summary>
    public int AnimationCount
    {
        get
        {
            ThrowIfDisposed();
            Native.GltfAssetGetAnimationCount(Engine.DangerousGetHandle(),
                RawHandle, out uint count).Check(nameof(GltfAsset.AnimationCount));
            return checked((int)count);
        }
    }

    /// <summary>
    /// Applies an animation at the given time and updates bone matrices.
    /// </summary>
    /// <param name="animationIndex">Zero-based animation index.</param>
    /// <param name="timeSeconds">Animation time in seconds.</param>
    public void ApplyAnimation(int animationIndex, double timeSeconds)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfNegative(animationIndex);
        Native.GltfAssetApplyAnimation(Engine.DangerousGetHandle(), RawHandle,
            (uint)animationIndex, timeSeconds).Check(nameof(GltfAsset.ApplyAnimation));
    }
}
