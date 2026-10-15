using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.InteropServices;
using FilamentSharp;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// Full-scene rendering integration on the Vulkan backend: real geometry,
/// real matc-compiled materials, real textures, real lights, and pixel
/// verification of the readback. Skipped with an explicit environmental
/// reason when no Vulkan ICD is available (docs/testing.md documents the
/// convention).
/// </summary>
public sealed class SceneRenderingIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public SceneRenderingIntegrationTests(ITestOutputHelper output)
    {
        _output = output;
    }

    private static float Srgb(float c) =>
        c <= 0.0031308f ? c * 12.92f : 1.055f * MathF.Pow(c, 1.0f / 2.4f) - 0.055f;

    [Fact]
    public void UnlitTriangleRendersAndVerifiesByPixels()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh).");
            return;
        }

        const uint W = 128, H = 128;
        var clearLinear = new Vector3(0.02f, 0.02f, 0.05f);
        var triangleColor = new Vector3(1.0f, 0.0f, 1.0f); // linear magenta

        using (engine)
        using (var renderer = engine!.CreateRenderer())
        using (var swapChain = engine.CreateHeadlessSwapChain(W, H))
        using (var view = engine.CreateView())
        using (var scene = engine.CreateScene())
        using (var material = engine.CreateMaterial(TestAssets.LoadMaterial("unlit_color")))
        using (var instance = material.CreateInstance())
        {
            instance.SetFloat3("color", triangleColor);

            // A triangle that covers the whole viewport AFTER the perspective
            // projection: with the camera at z=5 and a 45 degree vertical FOV,
            // the visible half-extent at z=0 is 5*tan(22.5) ~= 2.07, so these
            // vertices project to NDC (-1.06,-1.06), (3.19,-1.06), (-1.06,3.19)
            // - every NDC point (x+y <= 2) is inside.
            var attributes = new VertexAttributeDescription[]
            {
                new(VertexAttribute.Position, ElementType.Float3, 0, 0, 12),
            };
            using var vb = engine.CreateVertexBuffer(3, 1, attributes);
            var vertices = new float[]
            {
                -2.2f, -2.2f, 0.0f,
                 6.6f, -2.2f, 0.0f,
                -2.2f,  6.6f, 0.0f,
            };
            vb.SetBufferAt(0, MemoryMarshal.AsBytes<float>(vertices));

            using var ib = engine.CreateIndexBuffer(3, IndexType.UnsignedShort);
            ib.SetBuffer(MemoryMarshal.AsBytes<ushort>(new ushort[] { 0, 1, 2 }));

            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);
            camera.SetProjection(45.0, 1.0, 0.1, 100.0);
            camera.LookAt(new Vector3(0, 0, 5), Vector3.Zero);
            view.SetCamera(camera);
            view.SetScene(scene);
            view.SetViewport(0, 0, W, H);

            var meshEntity = EntityManager.Create();
            using var renderable = Renderable.Create(engine, meshEntity, new RenderableOptions
            {
                Vertices = vb,
                Indices = ib,
                PrimitiveCount = 3,
                BoundsMin = new Vector3(-1, -1, -1),
                BoundsMax = new Vector3(1, 1, 1),
                Material = instance,
                Culling = false,
            });
            scene.AddEntity(meshEntity);

            renderer.SetClearOptions(clearLinear.X, clearLinear.Y, clearLinear.Z, 1.0f);

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

            // Every sampled pixel must be the material color, not the clear
            // color. Tolerance 0.2 per channel: lavapipe's readback is close
            // to, but not exactly, sRGB (measured crosstalk up to ~0.14 in
            // the green channel for pure magenta input; see docs/testing.md).
            int failures = 0;
            var expected = new Vector3(Srgb(1.0f), Srgb(0.0f), Srgb(1.0f));
            for (int y = 8; y < H - 8 && failures < 4; y += 16)
            {
                for (int x = 8; x < W - 8 && failures < 4; x += 16)
                {
                    int i = (y * (int)W + x) * 4;
                    var pixel = new Vector3(
                        pixels[i] / 255f, pixels[i + 1] / 255f, pixels[i + 2] / 255f);
                    if (MathF.Abs(pixel.X - expected.X) > 0.2f ||
                        MathF.Abs(pixel.Y - expected.Y) > 0.2f ||
                        MathF.Abs(pixel.Z - expected.Z) > 0.2f)
                    {
                        failures++;
                    }
                }
            }
            Assert.True(failures == 0,
                $"full-screen triangle color mismatch at {failures} sampled pixels");

            _output.WriteLine(
                $"unlit triangle verified: {W}x{H} pixels sampled against " +
                $"sRGB({triangleColor.X:0.0},{triangleColor.Y:0.0},{triangleColor.Z:0.0})");
        }
    }

    [Fact]
    public void LitTexturedQuadWithDirectionalLightRenders()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine("ENVIRONMENTAL SKIP: no usable Vulkan ICD.");
            return;
        }

        const uint W = 128, H = 128;
        using (engine)
        using (var renderer = engine!.CreateRenderer())
        using (var swapChain = engine.CreateHeadlessSwapChain(W, H))
        using (var view = engine.CreateView())
        using (var scene = engine.CreateScene())
        using (var material = engine.CreateMaterial(TestAssets.LoadMaterial("lit_textured")))
        using (var instance = material.CreateInstance())
        {
            // 8x8 red/black checker texture.
            const uint T = 8;
            var texels = new byte[T * T * 4];
            for (uint y = 0; y < T; y++)
            {
                for (uint x = 0; x < T; x++)
                {
                    bool red = ((x / 4) + (y / 4)) % 2 == 0;
                    int i = (int)((y * T + x) * 4);
                    texels[i] = red ? byte.MaxValue : byte.MinValue;
                    texels[i + 1] = 0;
                    texels[i + 2] = 0;
                    texels[i + 3] = byte.MaxValue;
                }
            }
            using var texture = engine.CreateTexture2D(T, T, 1, TextureFormat.Srgb8A8);
            texture.SetImageRgba8(0, T, T, texels);

            instance.SetTexture("baseColorMap", texture,
                TextureMinFilter.Linear, TextureMagFilter.Linear,
                TextureWrap.ClampToEdge, TextureWrap.ClampToEdge);
            instance.SetFloat("roughness", 0.5f);
            instance.SetFloat("metallic", 0.0f);

            // Quad spanning the whole view with UVs and per-vertex tangent
            // quaternions. Lit shading requires TANGENTS (filament encodes the
            // TBN frame as a quaternion); SHORT4 is the normalized fixed-point
            // format gltfio uses. Identity quaternion = +Z facing quad.
            var attributes = new VertexAttributeDescription[]
            {
                new(VertexAttribute.Position, ElementType.Float3, 0, 0, 28),
                new(VertexAttribute.Uv0, ElementType.Float2, 0, 12, 28),
                new(VertexAttribute.Tangents, ElementType.Short4, 0, 20, 28),
            };
            using var vb = engine.CreateVertexBuffer(4, 1, attributes);
            // Interleaved 28-byte vertices: float3 position, float2 uv,
            // short4 normalized tangent quaternion (identity = +Z facing).
            var vertexData = new byte[4 * 28];
            var perVertex = new (float X, float Y, float Z, float U, float V)[]
            {
                (-0.8f, -0.8f, 0.0f, 0.0f, 0.0f),
                ( 0.8f, -0.8f, 0.0f, 1.0f, 0.0f),
                (-0.8f,  0.8f, 0.0f, 0.0f, 1.0f),
                ( 0.8f,  0.8f, 0.0f, 1.0f, 1.0f),
            };
            for (int v = 0; v < 4; v++)
            {
                int b = v * 28;
                var (x, y, z, u, vv) = perVertex[v];
                BinaryPrimitives.WriteSingleLittleEndian(vertexData.AsSpan(b), x);
                BinaryPrimitives.WriteSingleLittleEndian(vertexData.AsSpan(b + 4), y);
                BinaryPrimitives.WriteSingleLittleEndian(vertexData.AsSpan(b + 8), z);
                BinaryPrimitives.WriteSingleLittleEndian(vertexData.AsSpan(b + 12), u);
                BinaryPrimitives.WriteSingleLittleEndian(vertexData.AsSpan(b + 16), vv);
                // Quaternion (0,0,0,1) as normalized int16.
                BinaryPrimitives.WriteInt16LittleEndian(vertexData.AsSpan(b + 20), 0);
                BinaryPrimitives.WriteInt16LittleEndian(vertexData.AsSpan(b + 22), 0);
                BinaryPrimitives.WriteInt16LittleEndian(vertexData.AsSpan(b + 24), 0);
                BinaryPrimitives.WriteInt16LittleEndian(vertexData.AsSpan(b + 26), short.MaxValue);
            }
            vb.SetBufferAt(0, vertexData);

            using var ib = engine.CreateIndexBuffer(6, IndexType.UnsignedShort);
            ib.SetBuffer(MemoryMarshal.AsBytes<ushort>(new ushort[] { 0, 1, 2, 2, 1, 3 }));

            var cameraEntity = EntityManager.Create();
            using var camera = engine.CreateCamera(cameraEntity);
            camera.SetProjection(45.0, 1.0, 0.1, 100.0);
            camera.LookAt(new Vector3(0, 0, 4), Vector3.Zero);
            view.SetCamera(camera);
            view.SetScene(scene);
            view.SetViewport(0, 0, W, H);

            // Directional light from the camera direction.
            var lightEntity = EntityManager.Create();
            using var light = Light.Create(engine, lightEntity, new LightOptions
            {
                Type = LightType.Directional,
                Color = new Vector3(1.0f, 1.0f, 1.0f),
                Intensity = 100_000.0f,
                Direction = Vector3.Normalize(new Vector3(0.0f, -0.3f, -1.0f)),
            });
            scene.AddEntity(lightEntity);

            var meshEntity = EntityManager.Create();
            using var renderable = Renderable.Create(engine, meshEntity, new RenderableOptions
            {
                Vertices = vb,
                Indices = ib,
                PrimitiveCount = 6,
                BoundsMin = new Vector3(-1, -1, -1),
                BoundsMax = new Vector3(1, 1, 1),
                Material = instance,
                Culling = false,
            });
            scene.AddEntity(meshEntity);

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

            // Structural verification: the image must contain red-dominant
            // foreground (the lit red checker squares; red has low luma, so
            // the test keys on the red channel, not brightness) AND dark
            // background plus the black checker squares, in reasonable
            // proportions.
            int redCount = 0, darkCount = 0;
            for (int i = 0; i < pixels!.Length; i += 4)
            {
                float r = pixels[i] / 255f, g = pixels[i + 1] / 255f, b = pixels[i + 2] / 255f;
                if (r > 0.25f && r > g + 0.05f && r > b + 0.05f)
                {
                    redCount++;
                }
                else
                {
                    // Background + black checker squares. The clear color's
                    // sRGB-encoded readback luma measures ~0.10-0.11 on
                    // lavapipe, so the threshold sits just above it.
                    var luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    if (luma < 0.16f)
                    {
                        darkCount++;
                    }
                }
            }
            var total = W * H;
            _output.WriteLine(
                $"lit quad: red={redCount} ({100 * redCount / total}%), " +
                $"dark={darkCount} ({100 * darkCount / total}%)");

            Assert.InRange((double)redCount, (double)(total / 20), (double)(total / 2)); // red squares visible
            Assert.InRange((double)darkCount, (double)(total / 4), (double)total);      // background + black squares

            EntityManager.Destroy(lightEntity);
            EntityManager.Destroy(meshEntity);
            EntityManager.Destroy(cameraEntity);
        }
    }
}
