using System.Numerics;
using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// glTF loading on the NOOP backend (no GPU required): parse, query, and
/// lifetime contracts against the generated triangle.glb test model.
/// </summary>
public sealed class GltfObjectTests
{
    private static byte[] LoadModel()
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
            var candidate = Path.Combine(dir, "assets", "models", "triangle.glb");
            if (File.Exists(candidate))
            {
                return File.ReadAllBytes(candidate);
            }
        }
        throw new FileNotFoundException(
            "triangle.glb not found; run scripts/gen-test-gltf.py first.");
    }

    [Fact]
    public void LoadQueriesAndLifecycle()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var loader = engine.CreateGltfLoader();
        using var asset = loader.Load(LoadModel());

        Assert.True(asset.EntityCount >= 1);
        Assert.Equal(1, asset.RenderableCount);
        Assert.True(asset.Root.IsValid);

        var entities = asset.GetEntities();
        Assert.Equal(asset.EntityCount, entities.Length);
        Assert.All(entities, e => Assert.True(e.IsValid));

        // The generated triangle spans (-1,-1,0)..(3,3,0).
        var (min, max) = asset.BoundingBox;
        AssertEx.ApproximatelyEqual(-1.0f, min.X, 0.01f);
        AssertEx.ApproximatelyEqual(-1.0f, min.Y, 0.01f);
        AssertEx.ApproximatelyEqual(0.0f, min.Z, 0.01f);
        AssertEx.ApproximatelyEqual(3.0f, max.X, 0.01f);
        AssertEx.ApproximatelyEqual(3.0f, max.Y, 0.01f);
        AssertEx.ApproximatelyEqual(0.0f, max.Z, 0.01f);

        // No animations in the generated model; applying one is rejected.
        Assert.Equal(0, asset.AnimationCount);
        Assert.Throws<FilamentSharpException>(() => asset.ApplyAnimation(0, 0.0));

        asset.Dispose();
        Assert.Throws<ObjectDisposedException>(() => asset.GetEntities());
    }

    [Fact]
    public void InvalidContentIsRejected()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var loader = engine.CreateGltfLoader();

        Assert.Throws<ArgumentException>(() => loader.Load(ReadOnlySpan<byte>.Empty));
        Assert.Throws<FilamentSharpException>(() =>
            loader.Load(new byte[] { 1, 2, 3, 4, 5, 6, 7, 8 }));
    }

    [Fact]
    public void MultipleAssetsFromOneLoader()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var loader = engine.CreateGltfLoader();
        var glb = LoadModel();

        using var a = loader.Load(glb);
        using var b = loader.Load(glb);
        Assert.Equal(a.EntityCount, b.EntityCount);

        // Explicit destroy of one asset leaves the other usable.
        a.Dispose();
        Assert.True(b.EntityCount >= 1);
    }

    [Fact]
    public void EngineDisposalInvalidatesLoaderAndAsset()
    {
        var engine = Engine.Create(Backend.Noop);
        var loader = engine.CreateGltfLoader();
        var asset = loader.Load(LoadModel());
        engine.Dispose();

        Assert.Throws<ObjectDisposedException>(() => asset.EntityCount);
        Assert.Throws<ObjectDisposedException>(() => loader.Load(LoadModel()));
    }
}

/// <summary>Small test helpers shared by the unit suite.</summary>
internal static class AssertEx
{
    public static void ApproximatelyEqual(float expected, float actual, float tolerance)
    {
        Assert.True(MathF.Abs(expected - actual) <= tolerance,
            $"expected {expected} +/- {tolerance}, got {actual}");
    }
}
