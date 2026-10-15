using System.Numerics;
using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// A matc-compiled material package loaded into an engine. Create with
/// <see cref="Engine.CreateMaterial(ReadOnlySpan{byte})"/>.
/// </summary>
public sealed class Material : FilamentObject
{
    internal Material(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>Gets the names of the material's declared parameters.</summary>
    public IReadOnlyList<string> ParameterNames
    {
        get
        {
            ThrowIfDisposed();
            Native.MaterialGetParameterCount(Engine.DangerousGetHandle(), RawHandle,
                out uint count).Check(nameof(Material.ParameterNames));

            var names = new string[count];
            if (count > 0)
            {
                var pointers = new IntPtr[count];
                Native.MaterialGetParameterNames(Engine.DangerousGetHandle(), RawHandle,
                    pointers, count, out uint written).Check(nameof(Material.ParameterNames));
                for (int i = 0; i < written; i++)
                {
                    names[i] = Native.PtrToString(pointers[i]);
                }
            }
            return names;
        }
    }

    /// <summary>
    /// Creates a material instance with its own parameter values from this
    /// material.
    /// </summary>
    public MaterialInstance CreateInstance()
    {
        ThrowIfDisposed();
        var raw = Native.MaterialCreateInstance(Engine.DangerousGetHandle(), RawHandle);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Material.CreateInstance");
        }
        return new MaterialInstance(Engine,
            new ChildSafeHandle(raw, Engine.DangerousGetHandle(),
                (engine, handle) => { _ = Native.MaterialInstanceDestroy(engine, handle); }));
    }
}

/// <summary>
/// A material instance: a specialization of a <see cref="Material"/> with its
/// own parameter values (colors, scalars, texture bindings).
/// </summary>
public sealed class MaterialInstance : FilamentObject
{
    internal MaterialInstance(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    private static void CheckName(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
    }

    /// <summary>Sets a scalar float parameter.</summary>
    /// <param name="name">Parameter name.</param>
    /// <param name="value">New value.</param>
    public void SetFloat(string name, float value)
    {
        ThrowIfDisposed();
        CheckName(name);
        Native.MaterialInstanceSetFloat(Engine.DangerousGetHandle(), RawHandle,
            name, value).Check(nameof(MaterialInstance.SetFloat));
    }

    /// <summary>Sets a float3 parameter from a vector.</summary>
    /// <param name="name">Parameter name.</param>
    /// <param name="value">New value (linear RGB).</param>
    public void SetFloat3(string name, in Vector3 value)
    {
        ThrowIfDisposed();
        CheckName(name);
        Native.MaterialInstanceSetFloat3(Engine.DangerousGetHandle(), RawHandle,
            name, value.X, value.Y, value.Z).Check(nameof(MaterialInstance.SetFloat3));
    }

    /// <summary>Sets a float4 parameter from a color (linear RGBA).</summary>
    /// <param name="name">Parameter name.</param>
    /// <param name="value">New value.</param>
    public void SetFloat4(string name, in Vector4 value)
    {
        ThrowIfDisposed();
        CheckName(name);
        Native.MaterialInstanceSetFloat4(Engine.DangerousGetHandle(), RawHandle,
            name, value.X, value.Y, value.Z, value.W).Check(nameof(MaterialInstance.SetFloat4));
    }

    /// <summary>Sets a boolean parameter.</summary>
    /// <param name="name">Parameter name.</param>
    /// <param name="value">New value.</param>
    public void SetBool(string name, bool value)
    {
        ThrowIfDisposed();
        CheckName(name);
        Native.MaterialInstanceSetBool(Engine.DangerousGetHandle(), RawHandle,
            name, value ? 1 : 0).Check(nameof(MaterialInstance.SetBool));
    }

    /// <summary>Sets an integer parameter.</summary>
    /// <param name="name">Parameter name.</param>
    /// <param name="value">New value.</param>
    public void SetInt(string name, int value)
    {
        ThrowIfDisposed();
        CheckName(name);
        Native.MaterialInstanceSetInt(Engine.DangerousGetHandle(), RawHandle,
            name, value).Check(nameof(MaterialInstance.SetInt));
    }

    /// <summary>
    /// Binds a texture to a sampler parameter with the given filtering and
    /// wrap configuration.
    /// </summary>
    /// <param name="name">Sampler parameter name.</param>
    /// <param name="texture">The texture to bind.</param>
    /// <param name="minFilter">Minification filter.</param>
    /// <param name="magFilter">Magnification filter.</param>
    /// <param name="wrapS">Horizontal wrap mode.</param>
    /// <param name="wrapT">Vertical wrap mode.</param>
    public void SetTexture(string name, Texture texture, TextureMinFilter minFilter,
        TextureMagFilter magFilter, TextureWrap wrapS, TextureWrap wrapT)
    {
        ThrowIfDisposed();
        CheckName(name);
        ArgumentNullException.ThrowIfNull(texture);
        texture.ThrowIfDisposed();
        Native.MaterialInstanceSetTexture(Engine.DangerousGetHandle(), RawHandle,
            name, texture.RawHandle, (int)minFilter, (int)magFilter,
            (int)wrapS, (int)wrapT).Check(nameof(MaterialInstance.SetTexture));
    }
}

/// <summary>Texture internal formats exposed by this binding (subset).</summary>
public enum TextureFormat
{
    /// <summary>Linear 8-bit RGBA.</summary>
    Rgba8 = 30,

    /// <summary>sRGB-encoded 8-bit RGBA (recommended for color maps).</summary>
    Srgb8A8 = 31,
}

/// <summary>Minification filters (values mirror the native sampler filters).</summary>
public enum TextureMinFilter
{
    /// <summary>Nearest neighbor.</summary>
    Nearest = 0,

    /// <summary>Box filter.</summary>
    Linear = 1,

    /// <summary>Nearest within nearest mip level.</summary>
    NearestMipmapNearest = 2,

    /// <summary>Linear within nearest mip level.</summary>
    LinearMipmapNearest = 3,

    /// <summary>Interpolated mips, nearest within a level.</summary>
    NearestMipmapLinear = 4,

    /// <summary>Trilinear filtering.</summary>
    LinearMipmapLinear = 5,
}

/// <summary>Magnification filters.</summary>
public enum TextureMagFilter
{
    /// <summary>Nearest neighbor.</summary>
    Nearest = 0,

    /// <summary>Box filter.</summary>
    Linear = 1,
}

/// <summary>Texture coordinate wrap modes.</summary>
public enum TextureWrap
{
    /// <summary>Clamp to edge.</summary>
    ClampToEdge = 0,

    /// <summary>Repeat.</summary>
    Repeat = 1,

    /// <summary>Mirrored repeat.</summary>
    MirroredRepeat = 2,
}

/// <summary>
/// A 2D texture. Upload image data with
/// <see cref="SetImageRgba8(uint, uint, uint, ReadOnlySpan{byte})"/>.
/// </summary>
public sealed class Texture : FilamentObject
{
    internal Texture(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Uploads one mip level of RGBA8 data. The native side copies the data;
    /// the span is not retained after the call.
    /// </summary>
    /// <param name="level">Mip level to upload.</param>
    /// <param name="width">Level width in pixels.</param>
    /// <param name="height">Level height in pixels.</param>
    /// <param name="rgba">Exactly width*height*4 bytes.</param>
    public void SetImageRgba8(uint level, uint width, uint height, ReadOnlySpan<byte> rgba)
    {
        ThrowIfDisposed();
        var expected = width * height * 4;
        if (rgba.Length < expected)
        {
            throw new ArgumentException(
                $"Texture data must be at least width*height*4 = {expected} bytes.",
                nameof(rgba));
        }
        Native.TextureSetImageRgba8(Engine.DangerousGetHandle(), RawHandle,
            level, width, height, rgba, (nuint)expected).Check(nameof(Texture.SetImageRgba8));
    }
}

/// <summary>Light types (values mirror filament::LightManager::Type).</summary>
public enum LightType
{
    /// <summary>Directional light that also draws a sun disk in the sky.</summary>
    Sun = 0,

    /// <summary>Directional light.</summary>
    Directional = 1,

    /// <summary>Point light emitting in all directions.</summary>
    Point = 2,

    /// <summary>Physically correct spot light.</summary>
    FocusedSpot = 3,

    /// <summary>Spot light with decoupled cone and illumination.</summary>
    Spot = 4,
}

/// <summary>Options for <see cref="Light.Create(Engine, Entity, in LightOptions)"/>.</summary>
public struct LightOptions
{
    /// <summary>The kind of light to create.</summary>
    public LightType Type { get; set; }

    /// <summary>Linear light color.</summary>
    public Vector3 Color { get; set; }

    /// <summary>Intensity: lux for Sun/Directional, candela for Point/Spot.</summary>
    public float Intensity { get; set; }

    /// <summary>Emission direction (required for Sun/Directional/Spot).</summary>
    public Vector3? Direction { get; set; }

    /// <summary>World position (required for Point/Spot).</summary>
    public Vector3? Position { get; set; }

    /// <summary>Falloff radius in world units (0 = infinite).</summary>
    public float Falloff { get; set; }

    /// <summary>Whether the light casts shadows.</summary>
    public bool CastShadows { get; set; }
}

/// <summary>A light component attached to an entity.</summary>
public sealed class Light : FilamentObject
{
    internal Light(Engine engine, ChildSafeHandle handle, Entity entity)
        : base(engine, handle)
    {
        Entity = entity;
    }

    /// <summary>The entity this light is attached to.</summary>
    public Entity Entity { get; }

    /// <summary>Creates a light component on an entity.</summary>
    /// <param name="engine">Owning engine.</param>
    /// <param name="entity">Entity to attach the light to.</param>
    /// <param name="options">Light configuration.</param>
    /// <returns>The new light.</returns>
    /// <exception cref="FilamentSharpException">
    /// The light configuration is invalid for the chosen light type.
    /// </exception>
    public static Light Create(Engine engine, Entity entity, in LightOptions options)
    {
        ArgumentNullException.ThrowIfNull(engine);
        engine.ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }

        var direction = options.Direction ?? default;
        var position = options.Position ?? default;
        var raw = Native.LightCreate(engine.DangerousGetHandle(), entity.Id,
            (int)options.Type, options.Color.X, options.Color.Y, options.Color.Z,
            options.Intensity,
            options.Direction.HasValue ? 1 : 0, direction.X, direction.Y, direction.Z,
            options.Position.HasValue ? 1 : 0, position.X, position.Y, position.Z,
            options.Falloff, options.CastShadows ? 1 : 0);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure($"Light.Create({options.Type})");
        }
        return new Light(engine, new ChildSafeHandle(raw, engine.DangerousGetHandle(),
            (eng, handle) => { _ = Native.LightDestroy(eng, handle); }), entity);
    }
}

/// <summary>
/// Native mirror of <c>fs_renderable_desc</c> (LayoutKind.Sequential matches
/// the verified native layout: 80 bytes on x64). The runtime inserts the same
/// natural padding as the C compiler (uint entity, then pointer aligned to 8),
/// so NO explicit padding field is needed - adding one would double-pad and
/// shift every following field.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeRenderableDesc
{
    public uint Entity;
    public IntPtr Vertices;
    public IntPtr Indices;
    public uint PrimitiveOffset;
    public uint PrimitiveCount;
    public float MinX;
    public float MinY;
    public float MinZ;
    public float MaxX;
    public float MaxY;
    public float MaxZ;
    public IntPtr Material;
    public int Culling;
    public int CastShadows;
    public int ReceiveShadows;
}

/// <summary>Options for <see cref="Renderable.Create(Engine, Entity, in RenderableOptions)"/>.</summary>
public struct RenderableOptions
{
    /// <summary>Vertex buffer with the geometry.</summary>
    public VertexBuffer? Vertices { get; set; }

    /// <summary>Index buffer with the geometry.</summary>
    public IndexBuffer? Indices { get; set; }

    /// <summary>Index offset of the first primitive index.</summary>
    public uint PrimitiveOffset { get; set; }

    /// <summary>Number of indices forming the primitive.</summary>
    public uint PrimitiveCount { get; set; }

    /// <summary>Axis-aligned bounding box minimum.</summary>
    public Vector3 BoundsMin { get; set; }

    /// <summary>Axis-aligned bounding box maximum.</summary>
    public Vector3 BoundsMax { get; set; }

    /// <summary>Material instance for the primitive.</summary>
    public MaterialInstance? Material { get; set; }

    /// <summary>Whether frustum culling is enabled.</summary>
    public bool Culling { get; set; }

    /// <summary>Whether the renderable casts shadows.</summary>
    public bool CastShadows { get; set; }

    /// <summary>Whether the renderable receives shadows.</summary>
    public bool ReceiveShadows { get; set; }
}

/// <summary>A renderable component (geometry + material) on an entity.</summary>
public sealed class Renderable : FilamentObject
{
    internal Renderable(Engine engine, ChildSafeHandle handle, Entity entity)
        : base(engine, handle)
    {
        Entity = entity;
    }

    /// <summary>The entity this renderable is attached to.</summary>
    public Entity Entity { get; }

    /// <summary>Creates a renderable (single primitive) on an entity.</summary>
    /// <param name="engine">Owning engine.</param>
    /// <param name="entity">Entity to attach the renderable to.</param>
    /// <param name="options">Geometry/material configuration.</param>
    /// <returns>The new renderable.</returns>
    public static Renderable Create(Engine engine, Entity entity, in RenderableOptions options)
    {
        ArgumentNullException.ThrowIfNull(engine);
        engine.ThrowIfDisposed();
        ArgumentNullException.ThrowIfNull(options.Vertices);
        ArgumentNullException.ThrowIfNull(options.Indices);
        ArgumentNullException.ThrowIfNull(options.Material);
        var vertices = options.Vertices!;
        var indices = options.Indices!;
        var material = options.Material!;
        vertices.ThrowIfDisposed();
        indices.ThrowIfDisposed();
        material.ThrowIfDisposed();
        if (!entity.IsValid)
        {
            throw new ArgumentException("A valid entity is required.", nameof(entity));
        }
        if (options.PrimitiveCount == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(options),
                "PrimitiveCount must be non-zero.");
        }

        var desc = new NativeRenderableDesc
        {
            Entity = entity.Id,
            Vertices = vertices.RawHandle,
            Indices = indices.RawHandle,
            PrimitiveOffset = options.PrimitiveOffset,
            PrimitiveCount = options.PrimitiveCount,
            MinX = options.BoundsMin.X,
            MinY = options.BoundsMin.Y,
            MinZ = options.BoundsMin.Z,
            MaxX = options.BoundsMax.X,
            MaxY = options.BoundsMax.Y,
            MaxZ = options.BoundsMax.Z,
            Material = material.RawHandle,
            Culling = options.Culling ? 1 : 0,
            CastShadows = options.CastShadows ? 1 : 0,
            ReceiveShadows = options.ReceiveShadows ? 1 : 0,
        };

        var raw = Native.RenderableCreate(engine.DangerousGetHandle(), in desc);
        if (raw == IntPtr.Zero)
        {
            throw ThrowHelper.NativeFailure("Renderable.Create");
        }
        return new Renderable(engine, new ChildSafeHandle(raw, engine.DangerousGetHandle(),
            (eng, handle) => { _ = Native.RenderableDestroy(eng, handle); }), entity);
    }
}
