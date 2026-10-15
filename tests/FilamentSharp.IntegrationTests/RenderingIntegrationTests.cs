using FilamentSharp;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// Real rendering integration on the Vulkan backend. When no Vulkan ICD is
/// available, tests self-report an environmental skip in their output (see
/// docs/testing.md for the convention); in the reference environment they
/// execute against the Mesa lavapipe software ICD.
/// </summary>
public sealed class RenderingIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public RenderingIntegrationTests(ITestOutputHelper output)
    {
        _output = output;
    }

    private static bool TryCreateVulkanEngine(out Engine? engine) =>
        Engine.TryCreate(Backend.Vulkan, out engine);

    private static float Srgb(float c) =>
        c <= 0.0031308f ? c * 12.92f : 1.055f * MathF.Pow(c, 1.0f / 2.4f) - 0.055f;

    [Fact]
    public void FrameLoopRendersAndReadsBackClearFrame()
    {
        if (!TryCreateVulkanEngine(out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh).");
            return;
        }

        const uint W = 96, H = 96;
        using (engine)
        using (var renderer = engine!.CreateRenderer())
        using (var swapChain = engine.CreateHeadlessSwapChain(W, H))
        using (var view = engine.CreateView())
        using (var scene = engine.CreateScene())
        {
            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);

            camera.SetProjection(45.0, 1.0, 0.1, 100.0);
            camera.LookAt(new System.Numerics.Vector3(0, 0, 5), System.Numerics.Vector3.Zero);
            view.SetScene(scene);
            view.SetCamera(camera);
            view.SetViewport(0, 0, W, H);

            renderer.SetClearOptions(0.10f, 0.20f, 0.30f, 1.0f);

            byte[]? pixels = null;
            for (int frame = 0; frame < 8 && pixels == null; frame++)
            {
                if (renderer.BeginFrame(swapChain))
                {
                    renderer.Render(view);
                    renderer.ReadPixels(0, 0, W, H);
                    renderer.EndFrame();
                    pixels = renderer.CompleteReadPixels();
                }
            }

            Assert.NotNull(pixels);
            Assert.Equal(W * H * 4, (uint)pixels!.Length);

            int center = ((int)(H / 2) * (int)W + (int)(W / 2)) * 4;
            float r = pixels[center + 0] / 255.0f;
            float g = pixels[center + 1] / 255.0f;
            float b = pixels[center + 2] / 255.0f;
            float a = pixels[center + 3] / 255.0f;
            _output.WriteLine(
                $"center pixel: {r:0.000} {g:0.000} {b:0.000} {a:0.000} " +
                $"(sRGB of clear: {Srgb(0.10f):0.000} {Srgb(0.20f):0.000} {Srgb(0.30f):0.000} 1.000)");

            // Tolerance documented in docs/testing.md (driver rounding +
            // dithering + color management under the software rasterizer).
            Assert.InRange(r, Srgb(0.10f) - 0.12f, Srgb(0.10f) + 0.12f);
            Assert.InRange(g, Srgb(0.20f) - 0.12f, Srgb(0.20f) + 0.12f);
            Assert.InRange(b, Srgb(0.30f) - 0.12f, Srgb(0.30f) + 0.12f);
            Assert.InRange(a, 0.88f, 1.0f);

            _output.WriteLine("rendered a real frame on Vulkan and verified the readback");
        }
    }

    [Fact]
    public void EngineDisposalInvalidatesChildrenWithoutCrashing()
    {
        if (!TryCreateVulkanEngine(out var engine))
        {
            _output.WriteLine("ENVIRONMENTAL SKIP: no usable Vulkan ICD.");
            return;
        }

        var renderer = engine!.CreateRenderer();
        var view = engine.CreateView();
        engine.Dispose();

        Assert.Throws<ObjectDisposedException>(() => renderer.EndFrame());
        Assert.Throws<ObjectDisposedException>(() => view.SetViewport(0, 0, 4, 4));

        // Late disposal is a safe no-op at the native level.
        renderer.Dispose();
        view.Dispose();
        _output.WriteLine("children disposed after engine disposal: no crash, correct errors");
    }
}
