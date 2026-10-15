using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// Engine lifecycle semantics. Uses the NOOP backend so no GPU is required.
/// </summary>
public sealed class EngineTests
{
    [Fact]
    public void CreateWithInvalidBackendValueThrows()
    {
        var ex = Assert.Throws<FilamentSharpException>(
            () => Engine.Create((Backend)99));
        Assert.Contains("invalid", ex.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void NoopEngineReportsItsBackend()
    {
        using var engine = Engine.Create(Backend.Noop);
        Assert.Equal(Backend.Noop, engine.ActiveBackend);
        Assert.False(engine.IsDisposed);
    }

    [Fact]
    public void TryCreateNoopSucceeds()
    {
        Assert.True(Engine.TryCreate(Backend.Noop, out var engine));
        Assert.NotNull(engine);
        engine!.Dispose();
    }

    [Fact]
    public void TryCreateInvalidBackendFailsGracefully()
    {
        Assert.False(Engine.TryCreate((Backend)42, out var engine));
        Assert.Null(engine);
    }

    [Fact]
    public void DisposeIsIdempotent()
    {
        var engine = Engine.Create(Backend.Noop);
        engine.Dispose();
        engine.Dispose();
        Assert.True(engine.IsDisposed);
    }

    [Fact]
    public void UseAfterDisposeThrowsObjectDisposed()
    {
        var engine = Engine.Create(Backend.Noop);
        engine.Dispose();
        Assert.Throws<ObjectDisposedException>(() => engine.ActiveBackend);
    }

    [Fact]
    public void DisposingAnEngineDoesNotAffectNewEngines()
    {
        var first = Engine.Create(Backend.Noop);
        first.Dispose();

        using var second = Engine.Create(Backend.Noop);
        Assert.Equal(Backend.Noop, second.ActiveBackend);
    }

    [Fact]
    public void ManySequentialEnginesLifecycleCleanly()
    {
        // Catches state leaks in the native registry across create/destroy
        // cycles (the registry keeps engine wrappers alive forever; the
        // underlying Filament engines must still be destroyed correctly).
        for (int i = 0; i < 8; i++)
        {
            using var engine = Engine.Create(Backend.Noop);
            Assert.Equal(Backend.Noop, engine.ActiveBackend);
        }
    }

    [Fact]
    public async Task DisposeFromAnotherThreadIsSafe()
    {
        // Finalizer-thread safety: the native destroy is internally
        // synchronized, so disposing off-thread must be safe.
        var engine = Engine.Create(Backend.Noop);
        await Task.Run(() => engine.Dispose());
        Assert.True(engine.IsDisposed);
    }
}
