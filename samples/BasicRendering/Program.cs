// FilamentSharp BasicRendering sample.
//
// A complete, self-contained headless rendering pipeline:
//   1. Engine start (Vulkan when available, NOOP fallback with a warning)
//   2. Procedural scene: lit textured quad (matc-compiled material, generated
//      checker texture, vertex/index buffers), directional light, camera
//   3. Frame loop with pixel readback and verification, PPM image output
//   4. glTF scene: load assets/models/triangle.glb, render, verify
//   5. Ordered, explicit disposal of every object and the engine
//
// Run from the repository root: dotnet run --project samples/BasicRendering
// Publish as a Native AOT executable: scripts/publish-aot.sh
using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.InteropServices;
using FilamentSharp;

return SampleApp.Run(args);

internal sealed class SampleApp
{
    private const uint Width = 256;
    private const uint Height = 256;

    public static int Run(string[] args)
    {
        var outputDir = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();

        Console.WriteLine("FilamentSharp BasicRendering sample");
        Console.WriteLine($"  ABI version:    {NativeInfo.AbiVersion}");
        Console.WriteLine($"  Filament:       {NativeInfo.FilamentVersion} " +
                          $"({NativeInfo.FilamentCommit[..8]})");

        // --- 1. Engine ------------------------------------------------------
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            Console.WriteLine("  backend:        Vulkan unavailable; falling back to " +
                              "NOOP (frames render, pixels stay zero)");
            if (!Engine.TryCreate(Backend.Noop, out engine))
            {
                Console.WriteLine("ERROR: no usable backend (run " +
                                  "scripts/setup-headless-vulkan.sh for software Vulkan)");
                return 1;
            }
        }
        Console.WriteLine($"  backend:        {engine!.ActiveBackend}");

        using (engine)
        {
            // --- 2. Procedural scene: lit textured quad ---------------------
            Console.WriteLine("scene 1: procedural lit textured quad");
            RenderProceduralScene(engine!, outputDir);

            // --- 4. glTF scene ----------------------------------------------
            Console.WriteLine("scene 2: glTF asset");
            RenderGltfScene(engine!, outputDir);
        }
        Console.WriteLine("engine disposed; sample completed cleanly");
        return 0;
    }

    private static void RenderProceduralScene(Engine engine, string outputDir)
    {
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateHeadlessSwapChain(Width, Height);
        using var view = engine.CreateView();
        using var scene = engine.CreateScene();
        using var material = engine.CreateMaterial(LoadAsset(
            "external", "native-install", "materials", "lit_textured.filamat"));
        using var instance = material.CreateInstance();

        // 8x8 red/black checker texture uploaded as an sRGB texture.
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

        // Interleaved quad: float3 position, float2 uv, short4 tangent
        // quaternion (identity = +Z facing; lit shading requires TANGENTS).
        var attributes = new VertexAttributeDescription[]
        {
            new(VertexAttribute.Position, ElementType.Float3, 0, 0, 28),
            new(VertexAttribute.Uv0, ElementType.Float2, 0, 12, 28),
            new(VertexAttribute.Tangents, ElementType.Short4, 0, 20, 28),
        };
        using var vb = engine.CreateVertexBuffer(4, 1, attributes);
        vb.SetBufferAt(0, BuildQuadVertices());
        using var ib = engine.CreateIndexBuffer(6, IndexType.UnsignedShort);
        ib.SetBuffer(MemoryMarshal.AsBytes<ushort>(new ushort[] { 0, 1, 2, 2, 1, 3 }));

        var cameraEntity = EntityManager.Create();
        using var camera = engine.CreateCamera(cameraEntity);
        camera.SetProjection(45.0, 1.0, 0.1, 100.0);
        camera.LookAt(new Vector3(0, 0, 4), Vector3.Zero);
        view.SetCamera(camera);
        view.SetScene(scene);
        view.SetViewport(0, 0, Width, Height);

        var lightEntity = EntityManager.Create();
        using var light = Light.Create(engine, lightEntity, new LightOptions
        {
            Type = LightType.Directional,
            Color = Vector3.One,
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

        // --- 3. Frame loop + readback + PPM output --------------------------
        var pixels = RenderAndReadBack(renderer, swapChain, view);
        var stats = Analyze(pixels);
        Console.WriteLine($"  rendered {Width}x{Height}: red={stats.Red} " +
                          $"({100 * stats.Red / (Width * Height)}%), " +
                          $"dark={stats.Dark} ({100 * stats.Dark / (Width * Height)}%)");
        WritePpm(Path.Combine(outputDir, "basicrendering-quad.ppm"), pixels);
        Console.WriteLine($"  wrote basicrendering-quad.ppm");

        scene.RemoveEntity(meshEntity);
        scene.RemoveEntity(lightEntity);
        EntityManager.Destroy(meshEntity);
        EntityManager.Destroy(lightEntity);
        EntityManager.Destroy(cameraEntity);
    }

    private static void RenderGltfScene(Engine engine, string outputDir)
    {
        using var renderer = engine.CreateRenderer();
        using var swapChain = engine.CreateHeadlessSwapChain(Width, Height);
        using var view = engine.CreateView();
        using var scene = engine.CreateScene();
        using var loader = engine.CreateGltfLoader();
        using var asset = loader.Load(LoadAsset(
            "assets", "models", "triangle.glb"));

        Console.WriteLine($"  loaded triangle.glb: {asset.EntityCount} entities, " +
                          $"{asset.RenderableCount} renderable(s), " +
                          $"box {asset.BoundingBox.Min:0.0}..{asset.BoundingBox.Max:0.0}");

        foreach (var entity in asset.GetEntities())
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
        camera.SetProjection(45.0, 1.0, 0.1, 100.0);
        camera.LookAt(new Vector3(1, 1, 5), new Vector3(1, 1, 0));
        view.SetCamera(camera);
        view.SetScene(scene);
        view.SetViewport(0, 0, Width, Height);

        renderer.SetClearOptions(0.02f, 0.02f, 0.05f, 1.0f);

        var pixels = RenderAndReadBack(renderer, swapChain, view);
        var stats = Analyze(pixels);
        Console.WriteLine($"  rendered {Width}x{Height}: red={stats.Red} " +
                          $"({100 * stats.Red / (Width * Height)}%), " +
                          $"dark={stats.Dark} ({100 * stats.Dark / (Width * Height)}%)");
        WritePpm(Path.Combine(outputDir, "basicrendering-gltf.ppm"), pixels);
        Console.WriteLine($"  wrote basicrendering-gltf.ppm");

        EntityManager.Destroy(cameraEntity);
        EntityManager.Destroy(lightEntity);
        foreach (var entity in asset.GetEntities())
        {
            EntityManager.Destroy(entity);
        }
    }

    private static byte[] RenderAndReadBack(Renderer renderer, SwapChain swapChain, View view)
    {
        byte[]? pixels = null;
        for (int frame = 0; frame < 8 && pixels == null; frame++)
        {
            if (renderer.BeginFrame(swapChain))
            {
                renderer.Render(view);
                renderer.ReadPixels(0, 0, Width, Height);
                renderer.EndFrame();
                pixels = renderer.CompleteReadPixels();
            }
        }
        return pixels ?? new byte[Width * Height * 4];
    }

    private static (int Red, int Dark) Analyze(byte[] pixels)
    {
        int red = 0, dark = 0;
        for (int i = 0; i < pixels.Length; i += 4)
        {
            float r = pixels[i] / 255f, g = pixels[i + 1] / 255f, b = pixels[i + 2] / 255f;
            if (r > 0.25f && r > g + 0.05f && r > b + 0.05f)
            {
                red++;
            }
            else
            {
                var luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luma < 0.16f)
                {
                    dark++;
                }
            }
        }
        return (red, dark);
    }

    private static byte[] BuildQuadVertices()
    {
        var data = new byte[4 * 28];
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
            BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(b), x);
            BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(b + 4), y);
            BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(b + 8), z);
            BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(b + 12), u);
            BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(b + 16), vv);
            BinaryPrimitives.WriteInt16LittleEndian(data.AsSpan(b + 20), 0);
            BinaryPrimitives.WriteInt16LittleEndian(data.AsSpan(b + 22), 0);
            BinaryPrimitives.WriteInt16LittleEndian(data.AsSpan(b + 24), 0);
            BinaryPrimitives.WriteInt16LittleEndian(data.AsSpan(b + 26), short.MaxValue);
        }
        return data;
    }

    /// <summary>Finds a repo asset by walking up from the executable.</summary>
    private static byte[] LoadAsset(params string[] relative)
    {
        var dir = AppContext.BaseDirectory;
        for (int i = 0; i < 10; i++)
        {
            var candidate = Path.Combine([dir, .. relative]);
            if (File.Exists(candidate))
            {
                return File.ReadAllBytes(candidate);
            }
            var parent = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
            if (string.IsNullOrEmpty(parent))
            {
                break;
            }
            dir = parent;
        }
        throw new FileNotFoundException(
            $"Asset not found: {Path.Combine(relative)}. Run the sample from the " +
            "repository root (or pass an output directory; assets are located " +
            "by walking up from the executable).");
    }

    private static void WritePpm(string path, byte[] rgba)
    {
        using var stream = new FileStream(path, FileMode.Create, FileAccess.Write);
        var header = System.Text.Encoding.ASCII.GetBytes($"P6\n{Width} {Height}\n255\n");
        stream.Write(header);
        Span<byte> rgb = stackalloc byte[3];
        for (int i = 0; i < rgba.Length; i += 4)
        {
            rgb[0] = rgba[i];
            rgb[1] = rgba[i + 1];
            rgb[2] = rgba[i + 2];
            stream.Write(rgb);
        }
    }
}
