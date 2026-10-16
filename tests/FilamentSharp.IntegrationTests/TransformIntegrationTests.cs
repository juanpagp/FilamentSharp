using System.Numerics;
using FilamentSharp;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// Transform marshalling verified by pixels: System.Numerics matrices are
/// row-vector, Filament's mat4 is column-vector, so the bridge must
/// transpose. This is the regression test for the transposition bug where
/// translations landed in the W row and warped geometry through
/// <c>w' = t*x + 1</c> instead of moving it (invisible to the round-trip
/// unit tests, which are convention-symmetric).
/// </summary>
public sealed class TransformIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public TransformIntegrationTests(ITestOutputHelper output)
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
    public void TranslationMovesContentRightOnScreen()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh).");
            return;
        }

        const uint W = 128, H = 96;
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

            var lightEntity = EntityManager.Create();
            using var light = Light.Create(engine, lightEntity, new LightOptions
            {
                Type = LightType.Directional,
                Color = Vector3.One,
                Intensity = 100_000.0f,
                Direction = Vector3.Normalize(new Vector3(0.0f, 0.0f, -1.0f)),
            });
            scene.AddEntity(lightEntity);

            // Camera fitted to the triangle's bounding box (span -1..3),
            // looking down -Z: +X world maps to screen-right.
            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);
            camera.SetProjection(45.0, W / (double)H, 0.1, 100.0);
            camera.LookAt(new Vector3(1, 1, 5), new Vector3(1, 1, 0));
            view.SetCamera(camera);
            view.SetScene(scene);
            view.SetViewport(0, 0, W, H);
            renderer.SetClearOptions(0.0f, 0.0f, 0.0f, 1.0f);

            int CenterOfMass(Matrix4x4 transform)
            {
                engine.SetTransform(asset.Root, transform);
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
                long sx = 0;
                int n = 0;
                for (int i = 0; i < pixels!.Length; i += 4)
                {
                    float r = pixels[i] / 255f, g = pixels[i + 1] / 255f, b = pixels[i + 2] / 255f;
                    if (r + g + b > 0.15f)
                    {
                        sx += (i / 4) % W;
                        n++;
                    }
                }
                Assert.True(n > 100, $"the triangle should be visible (n={n})");
                return (int)(sx / n);
            }

            int center = CenterOfMass(Matrix4x4.Identity);
            int right = CenterOfMass(Matrix4x4.CreateTranslation(0.5f, 0, 0));
            int closer = CenterOfMass(Matrix4x4.CreateTranslation(0, 0, 1.0f));

            // +0.5 world X at this camera moves the content ~11px right.
            Assert.True(right > center + 4,
                $"a +X translation must move content right (was {center}, became {right})");
            // Moving 1 unit TOWARD the camera (z=+1 from z=0 with the eye at
            // z=5) grows the projected area; the X center moves only through
            // the changed clipping asymmetry, not a world X shift. With the
            // transposition bug, Z translations had no effect at all.
            Assert.True(Math.Abs(closer - center) < 10,
                $"a Z translation must not move the content sideways " +
                $"(was {center}, became {closer})");
            _output.WriteLine($"center of mass: identity={center}, +0.5X={right}, +1Z={closer}");

            EntityManager.Destroy(cameraEntity);
            EntityManager.Destroy(lightEntity);
            foreach (var entity in entities)
            {
                EntityManager.Destroy(entity);
            }
        }
    }

    [Fact]
    public void TranslationChangesApparentSizeThroughPerspective()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh).");
            return;
        }

        const uint W = 128, H = 96;
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

            var lightEntity = EntityManager.Create();
            using var light = Light.Create(engine, lightEntity, new LightOptions
            {
                Type = LightType.Directional,
                Color = Vector3.One,
                Intensity = 100_000.0f,
                Direction = Vector3.Normalize(new Vector3(0.0f, 0.0f, -1.0f)),
            });
            scene.AddEntity(lightEntity);

            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);
            camera.SetProjection(45.0, W / (double)H, 0.1, 100.0);
            camera.LookAt(new Vector3(1, 1, 5), new Vector3(1, 1, 0));
            view.SetCamera(camera);
            view.SetScene(scene);
            view.SetViewport(0, 0, W, H);
            renderer.SetClearOptions(0.0f, 0.0f, 0.0f, 1.0f);

            int LitPixels(Matrix4x4 transform)
            {
                engine.SetTransform(asset.Root, transform);
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
                int n = 0;
                for (int i = 0; i < pixels!.Length; i += 4)
                {
                    if (pixels[i] + pixels[i + 1] + pixels[i + 2] > 38)
                    {
                        n++;
                    }
                }
                return n;
            }

            // At z=0 the triangle is 5 units from the eye (z=5). Moving it to
            // z=+1 brings it to 4 units: the projected area grows by
            // (5/4)^2 = 1.56x. With the transposition bug the count did not
            // change at all (w' = t*z + 1 on the z=0 plane).
            int atZero = LitPixels(Matrix4x4.Identity);
            int atOne = LitPixels(Matrix4x4.CreateTranslation(0, 0, 1.0f));
            _output.WriteLine($"lit pixels: z=0 -> {atZero}, z=+1 -> {atOne}");
            Assert.True(atOne > atZero * 1.3f,
                $"moving 1 unit closer must grow the projected area " +
                $"({atZero} -> {atOne})");

            EntityManager.Destroy(cameraEntity);
            EntityManager.Destroy(lightEntity);
            foreach (var entity in entities)
            {
                EntityManager.Destroy(entity);
            }
        }
    }
}
