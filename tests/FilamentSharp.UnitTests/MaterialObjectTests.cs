using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// Materials, textures, lights, and renderables on the NOOP backend (no GPU
/// required): lifecycle, validation, and parameter plumbing. The material
/// packages come from the native build (matc step).
/// </summary>
public sealed class MaterialObjectTests
{
    private static readonly string[] ExpectedColorParameter = { "color" };

    private static byte[] LoadMaterial(string name)
    {
        var dir = AppContext.BaseDirectory;
        for (int i = 0; i < 8; i++)
        {
            var parent = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
            if (string.IsNullOrEmpty(parent))
            {
                break;
            }
            dir = parent;
            var candidate = Path.Combine(dir, "external", "native-install", "materials",
                name + ".filamat");
            if (File.Exists(candidate))
            {
                return File.ReadAllBytes(candidate);
            }
        }
        throw new FileNotFoundException(
            $"Material '{name}.filamat' not found; run scripts/build-native.sh first.");
    }

    [Fact]
    public void MaterialLifecycleAndParameterIntrospection()
    {
        using var engine = Engine.Create(Backend.Noop);
        var package = LoadMaterial("unlit_color");
        using var material = engine.CreateMaterial(package);

        Assert.Equal(ExpectedColorParameter, material.ParameterNames);

        using var instance = material.CreateInstance();
        instance.SetFloat3("color", new Vector3(1, 0, 0));

        // Unknown parameter name: the pinned Filament release raises a
        // precondition panic; the bridge error model converts it into an
        // error status, which the managed layer surfaces as an exception.
        Assert.Throws<FilamentSharpException>(() => instance.SetFloat("nonexistent", 1.0f));

        instance.Dispose();
        material.Dispose();
        Assert.Throws<ObjectDisposedException>(() => material.CreateInstance());
    }

    [Fact]
    public void InvalidMaterialPackageIsRejected()
    {
        using var engine = Engine.Create(Backend.Noop);
        Assert.Throws<ArgumentException>(() => engine.CreateMaterial(ReadOnlySpan<byte>.Empty));
        Assert.Throws<FilamentSharpException>(() =>
            engine.CreateMaterial(new byte[] { 1, 2, 3, 4 }));
    }

    [Fact]
    public void TextureLifecycleAndValidation()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var texture = engine.CreateTexture2D(4, 4, 1, TextureFormat.Rgba8);

        var texels = new byte[4 * 4 * 4];
        texture.SetImageRgba8(0, 4, 4, texels);

        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateTexture2D(0, 4, 1, TextureFormat.Rgba8));
        Assert.Throws<ArgumentException>(() =>
            texture.SetImageRgba8(0, 4, 4, new byte[8])); // too small
    }

    [Fact]
    public void LightCreationValidation()
    {
        using var engine = Engine.Create(Backend.Noop);
        var entity = EntityManager.Create();

        var light = Light.Create(engine, entity, new LightOptions
        {
            Type = LightType.Directional,
            Color = Vector3.One,
            Intensity = 100_000.0f,
            Direction = new Vector3(0, -1, 0),
        });
        Assert.Equal(entity, light.Entity);
        light.Dispose();

        // Directional without direction is rejected by the native validation.
        Assert.Throws<FilamentSharpException>(() => Light.Create(engine, entity,
            new LightOptions { Type = LightType.Directional, Intensity = 100 }));
        Assert.Throws<FilamentSharpException>(() => Light.Create(engine, entity,
            new LightOptions { Type = LightType.Point, Intensity = 100 }));

        Assert.Throws<ArgumentException>(() => Light.Create(engine, Entity.Null,
            new LightOptions { Type = LightType.Sun, Direction = Vector3.UnitY }));

        EntityManager.Destroy(entity);
    }

    [Fact]
    public void OutOfOrderMaterialDisposeIsSafeAndEngineReclaimsIt()
    {
        // Filament refuses to destroy a material while instances are alive.
        // The bridge maps the refusal to an error status; the managed
        // Dispose() (SafeHandle semantics: never throws) ignores it, the
        // native material stays alive, and engine teardown reclaims it via
        // the documented LIFO child destruction - no crash, no use-after-free.
        using var engine = Engine.Create(Backend.Noop);
        var material = engine.CreateMaterial(LoadMaterial("unlit_color"));
        using var instance = material.CreateInstance();

        material.Dispose(); // out of order: instance still alive

        // The native material is still alive, so the instance remains usable.
        instance.SetFloat3("color", new Vector3(0, 1, 0));

        engine.Dispose(); // must reclaim everything without crashing
    }

    [Fact]
    public void EngineDisposalTearsDownLiveMaterialAndInstanceSafely()
    {
        // Reproduces the crash fixed by LIFO child teardown: destroying the
        // engine with a live material + instance used to abort the process
        // inside FMaterial's destructor ("instances still alive").
        var engine = Engine.Create(Backend.Noop);
        var material = engine.CreateMaterial(LoadMaterial("unlit_color"));
        var instance = material.CreateInstance();
        instance.SetFloat3("color", new Vector3(0, 1, 0));

        engine.Dispose(); // must not crash

        Assert.Throws<ObjectDisposedException>(() => instance.SetFloat3("color", Vector3.One));
    }

    [Fact]
    public void NativeRenderableDescLayoutMatchesAbi()
    {
        // Managed mirror of the compile-time static_asserts in
        // tests/native/test_materials.cpp: the C# Sequential layout must
        // byte-match fs_renderable_desc (x64). A mismatch (e.g. an extra
        // padding field) shifts every following field and the native side
        // reads garbage pointers.
        Assert.Equal(80, Unsafe.SizeOf<NativeRenderableDesc>());
        Assert.Equal(0, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.Entity)));
        Assert.Equal(8, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.Vertices)));
        Assert.Equal(16, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.Indices)));
        Assert.Equal(24, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.PrimitiveOffset)));
        Assert.Equal(28, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.PrimitiveCount)));
        Assert.Equal(32, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.MinX)));
        Assert.Equal(44, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.MaxX)));
        Assert.Equal(56, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.Material)));
        Assert.Equal(64, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.Culling)));
        Assert.Equal(68, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.CastShadows)));
        Assert.Equal(72, (nint)Marshal.OffsetOf<NativeRenderableDesc>(nameof(NativeRenderableDesc.ReceiveShadows)));
    }

    [Fact]
    public void RenderableCreationValidation()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var material = engine.CreateMaterial(LoadMaterial("unlit_color"));
        using var instance = material.CreateInstance();

        var attributes = new VertexAttributeDescription[]
        {
            new(VertexAttribute.Position, ElementType.Float3, 0, 0, 12),
        };
        using var vb = engine.CreateVertexBuffer(3, 1, attributes);
        using var ib = engine.CreateIndexBuffer(3, IndexType.UnsignedShort);

        var entity = EntityManager.Create();
        var options = new RenderableOptions
        {
            Vertices = vb,
            Indices = ib,
            PrimitiveCount = 3,
            BoundsMin = new Vector3(-1),
            BoundsMax = new Vector3(1),
            Material = instance,
        };

        using var renderable = Renderable.Create(engine, entity, options);
        Assert.Equal(entity, renderable.Entity);

        Assert.Throws<ArgumentException>(() =>
            Renderable.Create(engine, Entity.Null, options));
        var zeroCount = options;
        zeroCount.PrimitiveCount = 0;
        Assert.Throws<ArgumentOutOfRangeException>(
            () => Renderable.Create(engine, entity, zeroCount));
        var noMaterial = options;
        noMaterial.Material = null!;
        Assert.Throws<ArgumentNullException>(
            () => Renderable.Create(engine, entity, noMaterial));

        EntityManager.Destroy(entity);
    }

    [Fact]
    public void RenderableMaterialAndBuffersInvalidatedByEngineDisposal()
    {
        var engine = Engine.Create(Backend.Noop);
        using var material = engine.CreateMaterial(LoadMaterial("unlit_color"));
        using var instance = material.CreateInstance();
        engine.Dispose();

        Assert.Throws<ObjectDisposedException>(() => instance.SetFloat3("color", Vector3.One));
        Assert.Throws<ObjectDisposedException>(() => material.CreateInstance());
    }
}
