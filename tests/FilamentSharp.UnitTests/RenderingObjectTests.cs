using System.Numerics;
using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// Rendering-infrastructure semantics on the NOOP backend (no GPU needed):
/// factories, disposal, invalidation, and argument validation.
/// </summary>
public sealed class RenderingObjectTests
{
    [Fact]
    public void FactoriesCreateUsableObjects()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateHeadlessSwapChain(32, 32);
        using var view = engine.CreateView();
        using var scene = engine.CreateScene();
        var entity = EntityManager.Create();
        using var camera = engine.CreateCamera(entity);

        Assert.False(renderer.IsDisposed);
        Assert.False(swapChain.IsDisposed);
        Assert.False(view.IsDisposed);
        Assert.False(scene.IsDisposed);
        Assert.False(camera.IsDisposed);
        Assert.Equal(entity, camera.Entity);

        EntityManager.Destroy(entity);
    }

    [Fact]
    public void SwapChainRequiresNonZeroSize()
    {
        using var engine = Engine.Create(Backend.Noop);
        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateHeadlessSwapChain(0, 32));
        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateHeadlessSwapChain(32, 0));
    }

    [Fact]
    public void NativeSwapChainRequiresNonZeroWindow()
    {
        using var engine = Engine.Create(Backend.Noop);
        Assert.Throws<ArgumentOutOfRangeException>(
            () => engine.CreateNativeSwapChain(IntPtr.Zero));
    }

    [Fact]
    public void NativeSwapChainLifecycleOnNoop()
    {
        // The NOOP backend's platform layer ignores the window handle, so a
        // fake XID exercises the full managed lifecycle (create -> frame loop
        // -> dispose) without a display. Real windowed rendering on Vulkan is
        // covered by the WindowedRendering sample (docs/windowed-rendering.md).
        using var engine = Engine.Create(Backend.Noop);
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateNativeSwapChain((IntPtr)0x1234);
        using var view = engine.CreateView();

        var entity = EntityManager.Create();
        using var camera = engine.CreateCamera(entity);
        camera.SetProjection(45.0, 1.0, 0.1, 100.0);
        camera.LookAt(new Vector3(0, 0, 5), Vector3.Zero);
        view.SetCamera(camera);
        view.SetViewport(0, 0, 32, 32);

        Assert.True(renderer.BeginFrame(swapChain));
        renderer.Render(view);
        renderer.EndFrame();

        EntityManager.Destroy(entity);
    }

    [Fact]
    public void NativeSwapChainFactoryThrowsAfterEngineDisposal()
    {
        var engine = Engine.Create(Backend.Noop);
        engine.Dispose();
        Assert.Throws<ObjectDisposedException>(
            () => engine.CreateNativeSwapChain((IntPtr)0x1234));
    }

    [Fact]
    public void CameraRequiresValidEntity()
    {
        using var engine = Engine.Create(Backend.Noop);
        Assert.Throws<ArgumentException>(() => engine.CreateCamera(Entity.Null));
    }

    [Fact]
    public void FactoriesThrowAfterEngineDisposal()
    {
        var engine = Engine.Create(Backend.Noop);
        engine.Dispose();
        Assert.Throws<ObjectDisposedException>(() => engine.CreateRenderer());
        Assert.Throws<ObjectDisposedException>(() => engine.CreateView());
        Assert.Throws<ObjectDisposedException>(() => engine.CreateScene());
        Assert.Throws<ObjectDisposedException>(() => engine.CreateHeadlessSwapChain(8, 8));
    }

    [Fact]
    public void ChildOperationsThrowAfterEngineDisposal()
    {
        var engine = Engine.Create(Backend.Noop);
        var renderer = engine.CreateRenderer();
        var view = engine.CreateView();
        engine.Dispose();

        Assert.Throws<ObjectDisposedException>(() => renderer.BeginFrame(null!));
        Assert.Throws<ObjectDisposedException>(() => renderer.EndFrame());
        Assert.Throws<ObjectDisposedException>(() => view.SetViewport(0, 0, 8, 8));

        // Disposal after engine disposal is a safe no-op (documented contract).
        renderer.Dispose();
        view.Dispose();
    }

    [Fact]
    public void DisposalIsIdempotent()
    {
        using var engine = Engine.Create(Backend.Noop);
        var renderer = engine.CreateRenderer();
        renderer.Dispose();
        renderer.Dispose();
        Assert.True(renderer.IsDisposed);
    }

    [Fact]
    public void RendererArgumentValidation()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateHeadlessSwapChain(16, 16);
        using var view = engine.CreateView();

        Assert.Throws<ArgumentNullException>(() => renderer.BeginFrame(null!));
        Assert.Throws<ArgumentOutOfRangeException>(() => renderer.ReadPixels(0, 0, 0, 8));
        Assert.Throws<ArgumentOutOfRangeException>(() => renderer.ReadPixels(0, 0, 8, 0));
        Assert.Throws<InvalidOperationException>(() => renderer.CompleteReadPixels());
        Assert.Throws<ArgumentNullException>(() => renderer.Render(null!));
    }

    [Fact]
    public void ViewWiringAcceptsNullScene()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var view = engine.CreateView();
        using var scene = engine.CreateScene();
        var entity = EntityManager.Create();
        using var camera = engine.CreateCamera(entity);

        view.SetScene(scene);
        view.SetScene(null); // detach is legal
        view.SetScene(scene);
        view.SetCamera(camera);
        view.SetViewport(0, 0, 16, 16);

        Assert.Throws<ArgumentNullException>(() => view.SetCamera(null!));
        EntityManager.Destroy(entity);
    }

    [Fact]
    public void CameraProjectionValidationAndMatrix()
    {
        using var engine = Engine.Create(Backend.Noop);
        var entity = EntityManager.Create();
        using var camera = engine.CreateCamera(entity);

        Assert.Throws<FilamentSharpException>(() =>
            camera.SetProjection(-45.0, 1.0, 0.1, 100.0));
        Assert.Throws<FilamentSharpException>(() =>
            camera.SetProjection(45.0, 1.0, 100.0, 0.1));

        camera.SetProjection(60.0, 1.5, 0.1, 100.0, FovDirection.Vertical);
        camera.LookAt(new Vector3(0, 0, 5), Vector3.Zero);

        var projection = camera.ProjectionMatrix;
        // GL-style perspective matrix in System.Numerics row-vector layout:
        // w' = -z comes from M34 (v' = v*M), the depth offset from M43.
        Assert.Equal(-1.0f, projection.M34, 5);
        Assert.Equal(0.0f, projection.M44, 5);
        Assert.True(projection.M11 > 0.5f && projection.M11 < 5.0f);

        EntityManager.Destroy(entity);
    }

    [Fact]
    public void NoopFrameLoopRuns()
    {
        using var engine = Engine.Create(Backend.Noop);
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateHeadlessSwapChain(16, 16);
        using var view = engine.CreateView();
        using var scene = engine.CreateScene();
        var entity = EntityManager.Create();
        using var camera = engine.CreateCamera(entity);

        camera.SetProjection(45.0, 1.0, 0.1, 100.0);
        camera.LookAt(new Vector3(0, 0, 5), Vector3.Zero);
        view.SetScene(scene);
        view.SetCamera(camera);
        view.SetViewport(0, 0, 16, 16);
        renderer.SetClearOptions(0.1f, 0.2f, 0.3f, 1.0f);

        for (int i = 0; i < 3; i++)
        {
            if (renderer.BeginFrame(swapChain))
            {
                renderer.Render(view);
                renderer.EndFrame();
            }
        }

        EntityManager.Destroy(entity);
    }
}
