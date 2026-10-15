using System.Numerics;
using FilamentSharp;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// glTF end-to-end on the Vulkan backend: load the generated triangle.glb,
/// add its entities to a scene, render, and verify the pixels really show
/// the glTF material's base color. Skipped with an explicit environmental
/// reason when no Vulkan ICD is available.
/// </summary>
public sealed class GltfRenderingIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public GltfRenderingIntegrationTests(ITestOutputHelper output)
    {
        _output = output;
    }

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
    public void GltfTriangleRendersAndVerifiesByPixels()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh).");
            return;
        }

        const uint W = 128, H = 128;
        using (engine)
        using (var renderer = engine!.CreateRenderer())
        using (var swapChain = engine.CreateHeadlessSwapChain(W, H))
        using (var view = engine.CreateView())
        using (var scene = engine.CreateScene())
        using (var loader = engine.CreateGltfLoader())
        using (var asset = loader.Load(LoadModel()))
        {
            var entities = asset.GetEntities();
            foreach (var entity in entities)
            {
                scene.AddEntity(entity);
            }

            // The glTF PBR material uses lit shading; without a light the
            // triangle would render black. One directional light from the
            // camera direction makes the base color visible.
            var lightEntity = EntityManager.Create();
            using var light = Light.Create(engine, lightEntity, new LightOptions
            {
                Type = LightType.Directional,
                Color = Vector3.One,
                Intensity = 100_000.0f,
                Direction = Vector3.Normalize(new Vector3(0.0f, 0.0f, -1.0f)),
            });
            scene.AddEntity(lightEntity);

            // Camera fitted to the triangle's bounding box (span -1..3).
            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);
            camera.SetProjection(45.0, 1.0, 0.1, 100.0);
            camera.LookAt(new Vector3(1, 1, 5), new Vector3(1, 1, 0));
            view.SetCamera(camera);
            view.SetScene(scene);
            view.SetViewport(0, 0, W, H);

            renderer.SetClearOptions(0.02f, 0.02f, 0.05f, 1.0f);

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

            // Structural + color verification: the image must contain
            // red-dominant pixels (the glTF baseColorFactor [1,0,0,1] under
            // the ubershader lit material, tolerance for lavapipe readback
            // non-ideality) AND background pixels near the clear color.
            int red = 0, background = 0;
            for (int i = 0; i < pixels!.Length; i += 4)
            {
                float r = pixels[i] / 255f, g = pixels[i + 1] / 255f, b = pixels[i + 2] / 255f;
                if (r > 0.3f && r > g + 0.1f && r > b + 0.1f)
                {
                    red++;
                }
                else
                {
                    var luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    if (luma < 0.16f)
                    {
                        background++;
                    }
                }
            }
            var total = (int)(W * H);
            _output.WriteLine(
                $"gltf triangle: red={red} ({100 * red / total}%), " +
                $"background={background} ({100 * background / total}%)");

            Assert.InRange((double)red, (double)(total / 20), (double)(total / 2));
            Assert.InRange((double)background, (double)(total / 4), (double)total);

            EntityManager.Destroy(cameraEntity);
            EntityManager.Destroy(lightEntity);
            foreach (var entity in entities)
            {
                EntityManager.Destroy(entity);
            }
        }
    }
}
