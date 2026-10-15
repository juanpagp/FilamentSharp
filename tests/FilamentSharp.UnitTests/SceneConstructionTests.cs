using System.Numerics;
using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// Geometry buffers, transforms, and scene membership on the NOOP backend.
/// </summary>
public sealed class SceneConstructionTests
{
    [Fact]
    public void VertexBufferLifecycle()
    {
        using var engine = Engine.Create(Backend.Noop);
        var attributes = new VertexAttributeDescription[]
        {
            new(VertexAttribute.Position, ElementType.Float3, 0, 0, 12),
        };
        using var vb = engine.CreateVertexBuffer(3, 1, attributes);

        var vertices = new byte[3 * 12];
        vb.SetBufferAt(0, vertices);
        vb.SetBufferAt(0, vertices); // re-upload is legal

        vb.Dispose();
        Assert.True(vb.IsDisposed);
    }

    [Fact]
    public void VertexBufferValidation()
    {
        using var engine = Engine.Create(Backend.Noop);
        var attributes = new[] { new VertexAttributeDescription(0, ElementType.Float3) };

        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateVertexBuffer(0, 1, attributes));
        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateVertexBuffer(3, 0, attributes));
        Assert.Throws<ArgumentException>(
            () => engine.CreateVertexBuffer(3, 1, ReadOnlySpan<VertexAttributeDescription>.Empty));

        using var vb = engine.CreateVertexBuffer(3, 1, attributes);
        Assert.Throws<ArgumentException>(() => vb.SetBufferAt(0, ReadOnlySpan<byte>.Empty));

        var badAttribute = new[] { new VertexAttributeDescription((VertexAttribute)99, ElementType.Float3) };
        Assert.Throws<FilamentSharpException>(() => engine.CreateVertexBuffer(3, 1, badAttribute));

        var badType = new[] { new VertexAttributeDescription(VertexAttribute.Position, (ElementType)99) };
        Assert.Throws<FilamentSharpException>(() => engine.CreateVertexBuffer(3, 1, badType));

        var badBuffer = new[] { new VertexAttributeDescription(VertexAttribute.Position, ElementType.Float3, 5) };
        Assert.Throws<FilamentSharpException>(() => engine.CreateVertexBuffer(3, 1, badBuffer));
    }

    [Fact]
    public void IndexBufferLifecycle()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var ib16 = engine.CreateIndexBuffer(3, IndexType.UnsignedShort);
        using var ib32 = engine.CreateIndexBuffer(3, IndexType.UnsignedInt);

        ib16.SetBuffer(new byte[3 * 2]);
        ib32.SetBuffer(new byte[3 * 4]);

        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateIndexBuffer(0, IndexType.UnsignedShort));
        Assert.Throws<ArgumentException>(() => ib16.SetBuffer(ReadOnlySpan<byte>.Empty));
    }

    [Fact]
    public void SceneEntityMembership()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var scene = engine.CreateScene();
        var e1 = EntityManager.Create();
        var e2 = EntityManager.Create();

        Assert.Equal(0u, scene.EntityCount);
        Assert.False(scene.HasEntity(e1));

        scene.AddEntity(e1);
        scene.AddEntity(e2);
        Assert.Equal(2u, scene.EntityCount);
        Assert.True(scene.HasEntity(e1));

        scene.RemoveEntity(e1);
        Assert.Equal(1u, scene.EntityCount);
        Assert.False(scene.HasEntity(e1));

        Assert.Throws<ArgumentException>(() => scene.AddEntity(Entity.Null));
        Assert.Throws<ArgumentException>(() => scene.HasEntity(Entity.Null));

        EntityManager.Destroy(e1);
        EntityManager.Destroy(e2);
    }

    [Fact]
    public void TransformRoundTripAndHierarchy()
    {
        using var engine = Engine.Create(Backend.Noop);
        var parent = EntityManager.Create();
        var child = EntityManager.Create();

        var translation = Matrix4x4.CreateTranslation(2f, 0f, 0f);
        engine.SetTransform(parent, translation);
        Assert.Equal(translation, engine.GetWorldTransform(parent));

        engine.SetTransform(child, Matrix4x4.Identity);
        engine.SetParent(child, parent);
        var world = engine.GetWorldTransform(child);
        Assert.Equal(2f, world.Translation.X, 4); // inherits parent translation

        engine.SetParent(child, null); // detach
        world = engine.GetWorldTransform(child);
        Assert.Equal(0f, world.Translation.X, 4);

        // GetWorldTransform on an entity without a transform component fails
        // with a native error surfaced as FilamentSharpException.
        var stranger = EntityManager.Create();
        Assert.Throws<FilamentSharpException>(() => engine.GetWorldTransform(stranger));

        Assert.Throws<ArgumentException>(() => engine.SetTransform(Entity.Null, translation));
        Assert.Throws<ArgumentException>(() => engine.SetParent(Entity.Null, null));

        EntityManager.Destroy(parent);
        EntityManager.Destroy(child);
        EntityManager.Destroy(stranger);
    }

    [Fact]
    public void BuffersInvalidatedByEngineDisposal()
    {
        var engine = Engine.Create(Backend.Noop);
        var attributes = new[] { new VertexAttributeDescription(VertexAttribute.Position, ElementType.Float3) };
        var vb = engine.CreateVertexBuffer(3, 1, attributes);
        var ib = engine.CreateIndexBuffer(3, IndexType.UnsignedShort);
        engine.Dispose();

        Assert.Throws<ObjectDisposedException>(() => vb.SetBufferAt(0, new byte[36]));
        Assert.Throws<ObjectDisposedException>(() => ib.SetBuffer(new byte[6]));

        // Safe no-op disposal path.
        vb.Dispose();
        ib.Dispose();
    }
}
