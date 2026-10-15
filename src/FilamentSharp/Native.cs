using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Source-generated P/Invoke bindings for the native bridge. Every entry
/// point declared here must exist in <c>filamentsharp/abi.h</c>; the
/// integration test suite verifies the full set resolves at runtime.
///
/// This type is internal on purpose: the public API is the idiomatic wrapper
/// layer, never raw pointers.
/// </summary>
internal static unsafe partial class Native
{
    static Native()
    {
        // Loading must happen before any P/Invoke call; the static constructor
        // is guaranteed to run first (including under Native AOT).
        NativeLibraryResolver.EnsureLoaded();
    }

    private const string LibraryName = "FilamentSharp.Native";

    // -------------------------------------------------------------- versioning
    //
    // NOTE: functions returning bridge-owned `const char*` are declared as
    // returning IntPtr and copied with Marshal.PtrToStringUTF8. A
    // `[return: MarshalAs(LPUTF8Str)] string` would make the marshaller call
    // CoTaskMemFree on the returned pointer, corrupting the heap, because it
    // assumes the callee allocated the string with CoTaskMemAlloc.

    [LibraryImport(LibraryName, EntryPoint = "fs_abi_version")]
    internal static partial uint AbiVersion();

    [LibraryImport(LibraryName, EntryPoint = "fs_filament_version")]
    internal static partial IntPtr FilamentVersionPtr();

    [LibraryImport(LibraryName, EntryPoint = "fs_filament_commit")]
    internal static partial IntPtr FilamentCommitPtr();

    // ------------------------------------------------------------- error model

    [LibraryImport(LibraryName, EntryPoint = "fs_get_last_error")]
    internal static partial IntPtr GetLastErrorPtr();

    [LibraryImport(LibraryName, EntryPoint = "fs_clear_last_error")]
    internal static partial void ClearLastError();

    // ----------------------------------------------------------------- engine

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_create")]
    internal static partial IntPtr EngineCreate(int backend);

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_destroy")]
    internal static partial void EngineDestroy(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_engine_get_backend")]
    internal static partial int EngineGetBackend(IntPtr engine);

    // --------------------------------------------------------------- entities

    [LibraryImport(LibraryName, EntryPoint = "fs_entity_create")]
    internal static partial uint EntityCreate();

    [LibraryImport(LibraryName, EntryPoint = "fs_entity_destroy")]
    internal static partial void EntityDestroy(uint entity);

    // ---------------------------------------------------------------- renderer

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_create")]
    internal static partial IntPtr RendererCreate(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_destroy")]
    internal static partial int RendererDestroy(IntPtr engine, IntPtr renderer);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_begin_frame")]
    internal static partial int RendererBeginFrame(IntPtr engine, IntPtr renderer,
        IntPtr swapChain, ulong frameTimeNs, out int began);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_render")]
    internal static partial int RendererRender(IntPtr engine, IntPtr renderer, IntPtr view);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_end_frame")]
    internal static partial int RendererEndFrame(IntPtr engine, IntPtr renderer);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_set_clear_options")]
    internal static partial int RendererSetClearOptions(IntPtr engine, IntPtr renderer,
        float r, float g, float b, float a, int clear, int discard);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_read_pixels")]
    internal static partial int RendererReadPixels(IntPtr engine, IntPtr renderer,
        uint x, uint y, uint width, uint height);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderer_read_pixels_complete")]
    internal static partial int RendererReadPixelsComplete(IntPtr engine, IntPtr renderer,
        byte[] rgba, nuint capacity, out nuint bytesWritten);

    // -------------------------------------------------------------- swap chain

    [LibraryImport(LibraryName, EntryPoint = "fs_swap_chain_create_headless")]
    internal static partial IntPtr SwapChainCreateHeadless(IntPtr engine,
        uint width, uint height, ulong flags);

    [LibraryImport(LibraryName, EntryPoint = "fs_swap_chain_destroy")]
    internal static partial int SwapChainDestroy(IntPtr engine, IntPtr swapChain);

    // -------------------------------------------------------------------- view

    [LibraryImport(LibraryName, EntryPoint = "fs_view_create")]
    internal static partial IntPtr ViewCreate(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_view_destroy")]
    internal static partial int ViewDestroy(IntPtr engine, IntPtr view);

    [LibraryImport(LibraryName, EntryPoint = "fs_view_set_scene")]
    internal static partial int ViewSetScene(IntPtr engine, IntPtr view, IntPtr scene);

    [LibraryImport(LibraryName, EntryPoint = "fs_view_set_camera")]
    internal static partial int ViewSetCamera(IntPtr engine, IntPtr view, IntPtr camera);

    [LibraryImport(LibraryName, EntryPoint = "fs_view_set_viewport")]
    internal static partial int ViewSetViewport(IntPtr engine, IntPtr view,
        int left, int bottom, uint width, uint height);

    // ------------------------------------------------------------------- scene

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_create")]
    internal static partial IntPtr SceneCreate(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_destroy")]
    internal static partial int SceneDestroy(IntPtr engine, IntPtr scene);

    // ------------------------------------------------------------------ camera

    [LibraryImport(LibraryName, EntryPoint = "fs_camera_create")]
    internal static partial IntPtr CameraCreate(IntPtr engine, uint entity);

    [LibraryImport(LibraryName, EntryPoint = "fs_camera_destroy")]
    internal static partial int CameraDestroy(IntPtr engine, IntPtr camera);

    [LibraryImport(LibraryName, EntryPoint = "fs_camera_set_projection")]
    internal static partial int CameraSetProjection(IntPtr engine, IntPtr camera,
        double fovDegrees, double aspect, double nearPlane, double farPlane,
        int fovDirection);

    [LibraryImport(LibraryName, EntryPoint = "fs_camera_look_at")]
    internal static partial int CameraLookAt(IntPtr engine, IntPtr camera,
        ReadOnlySpan<double> eye, ReadOnlySpan<double> center, ReadOnlySpan<double> up);

    [LibraryImport(LibraryName, EntryPoint = "fs_camera_get_projection_matrix")]
    internal static partial int CameraGetProjectionMatrix(IntPtr engine, IntPtr camera,
        Span<float> matrix);

    // --------------------------------------------------------- geometry buffers

    [LibraryImport(LibraryName, EntryPoint = "fs_vertex_buffer_create")]
    internal static partial IntPtr VertexBufferCreate(IntPtr engine,
        uint vertexCount, uint bufferCount,
        ReadOnlySpan<VertexAttributeDescription> attributes, uint attributeCount);

    [LibraryImport(LibraryName, EntryPoint = "fs_vertex_buffer_set_buffer")]
    internal static partial int VertexBufferSetBuffer(IntPtr engine, IntPtr vertexBuffer,
        uint bufferIndex, ReadOnlySpan<byte> data, nuint size);

    [LibraryImport(LibraryName, EntryPoint = "fs_vertex_buffer_destroy")]
    internal static partial int VertexBufferDestroy(IntPtr engine, IntPtr vertexBuffer);

    [LibraryImport(LibraryName, EntryPoint = "fs_index_buffer_create")]
    internal static partial IntPtr IndexBufferCreate(IntPtr engine,
        uint indexCount, int indexType);

    [LibraryImport(LibraryName, EntryPoint = "fs_index_buffer_set_buffer")]
    internal static partial int IndexBufferSetBuffer(IntPtr engine, IntPtr indexBuffer,
        ReadOnlySpan<byte> data, nuint size);

    [LibraryImport(LibraryName, EntryPoint = "fs_index_buffer_destroy")]
    internal static partial int IndexBufferDestroy(IntPtr engine, IntPtr indexBuffer);

    // -------------------------------------------------------- scene entities

    [LibraryImport(LibraryName, EntryPoint = "fs_transform_set_matrix")]
    internal static partial int TransformSetMatrix(IntPtr engine, uint entity,
        ReadOnlySpan<float> columnMajor16);

    [LibraryImport(LibraryName, EntryPoint = "fs_transform_get_world_matrix")]
    internal static partial int TransformGetWorldMatrix(IntPtr engine, uint entity,
        Span<float> outMatrix16);

    [LibraryImport(LibraryName, EntryPoint = "fs_transform_set_parent")]
    internal static partial int TransformSetParent(IntPtr engine, uint child, uint parent);

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_add_entity")]
    internal static partial int SceneAddEntity(IntPtr engine, IntPtr scene, uint entity);

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_remove_entity")]
    internal static partial int SceneRemoveEntity(IntPtr engine, IntPtr scene, uint entity);

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_has_entity")]
    internal static partial int SceneHasEntity(IntPtr engine, IntPtr scene, uint entity,
        out int has);

    [LibraryImport(LibraryName, EntryPoint = "fs_scene_get_entity_count")]
    internal static partial int SceneGetEntityCount(IntPtr engine, IntPtr scene,
        out uint count);

    // ---------------------------------------------- materials/textures/lights

    [LibraryImport(LibraryName, EntryPoint = "fs_material_create")]
    internal static partial IntPtr MaterialCreate(IntPtr engine,
        ReadOnlySpan<byte> package, nuint packageSize);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_destroy")]
    internal static partial int MaterialDestroy(IntPtr engine, IntPtr material);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_get_parameter_count")]
    internal static partial int MaterialGetParameterCount(IntPtr engine, IntPtr material,
        out uint count);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_get_parameter_names")]
    internal static partial int MaterialGetParameterNames(IntPtr engine, IntPtr material,
        IntPtr[] names, uint capacity, out uint written);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_create_instance")]
    internal static partial IntPtr MaterialCreateInstance(IntPtr engine, IntPtr material);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_destroy")]
    internal static partial int MaterialInstanceDestroy(IntPtr engine, IntPtr instance);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_float", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetFloat(IntPtr engine, IntPtr instance,
        string name, float value);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_float3", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetFloat3(IntPtr engine, IntPtr instance,
        string name, float x, float y, float z);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_float4", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetFloat4(IntPtr engine, IntPtr instance,
        string name, float x, float y, float z, float w);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_bool", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetBool(IntPtr engine, IntPtr instance,
        string name, int value);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_int", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetInt(IntPtr engine, IntPtr instance,
        string name, int value);

    [LibraryImport(LibraryName, EntryPoint = "fs_material_instance_set_texture", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int MaterialInstanceSetTexture(IntPtr engine, IntPtr instance,
        string name, IntPtr texture, int minFilter, int magFilter, int wrapS, int wrapT);

    [LibraryImport(LibraryName, EntryPoint = "fs_texture_create_2d")]
    internal static partial IntPtr TextureCreate2d(IntPtr engine, uint width, uint height,
        uint levels, int format);

    [LibraryImport(LibraryName, EntryPoint = "fs_texture_set_image_rgba8")]
    internal static partial int TextureSetImageRgba8(IntPtr engine, IntPtr texture,
        uint level, uint width, uint height, ReadOnlySpan<byte> rgba, nuint size);

    [LibraryImport(LibraryName, EntryPoint = "fs_texture_destroy")]
    internal static partial int TextureDestroy(IntPtr engine, IntPtr texture);

    [LibraryImport(LibraryName, EntryPoint = "fs_light_create")]
    internal static partial IntPtr LightCreate(IntPtr engine, uint entity, int type,
        float r, float g, float b, float intensity,
        int hasDirection, double dirX, double dirY, double dirZ,
        int hasPosition, double posX, double posY, double posZ,
        float falloff, int castShadows);

    [LibraryImport(LibraryName, EntryPoint = "fs_light_destroy")]
    internal static partial int LightDestroy(IntPtr engine, IntPtr light);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderable_create")]
    internal static partial IntPtr RenderableCreate(IntPtr engine,
        in NativeRenderableDesc desc);

    [LibraryImport(LibraryName, EntryPoint = "fs_renderable_destroy")]
    internal static partial int RenderableDestroy(IntPtr engine, IntPtr renderable);

    // ----------------------------------------------------------------- gltf

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_loader_create")]
    internal static partial IntPtr GltfLoaderCreate(IntPtr engine);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_loader_destroy")]
    internal static partial int GltfLoaderDestroy(IntPtr engine, IntPtr loader);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_loader_load")]
    internal static partial IntPtr GltfLoaderLoad(IntPtr engine, IntPtr loader,
        ReadOnlySpan<byte> content, nuint size);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_destroy")]
    internal static partial int GltfAssetDestroy(IntPtr engine, IntPtr asset);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_entity_count")]
    internal static partial int GltfAssetGetEntityCount(IntPtr engine, IntPtr asset,
        out uint count);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_entities")]
    internal static partial int GltfAssetGetEntities(IntPtr engine, IntPtr asset,
        uint[] entities, uint capacity, out uint written);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_root")]
    internal static partial int GltfAssetGetRoot(IntPtr engine, IntPtr asset, out uint root);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_renderable_count")]
    internal static partial int GltfAssetGetRenderableCount(IntPtr engine, IntPtr asset,
        out uint count);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_bounding_box")]
    internal static unsafe partial int GltfAssetGetBoundingBox(IntPtr engine, IntPtr asset,
        float* outMin, float* outMax);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_get_animation_count")]
    internal static partial int GltfAssetGetAnimationCount(IntPtr engine, IntPtr asset,
        out uint count);

    [LibraryImport(LibraryName, EntryPoint = "fs_gltf_asset_apply_animation")]
    internal static partial int GltfAssetApplyAnimation(IntPtr engine, IntPtr asset,
        uint animationIndex, double timeSeconds);

    // ------------------------------------------------------------ string helpers

    /// <summary>
    /// Copies a bridge-owned UTF-8 string. Never throws, never frees: the
    /// bridge keeps ownership of the buffer (valid until the next bridge call
    /// on the same thread).
    /// </summary>
    internal static string PtrToString(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
        {
            return string.Empty;
        }
        return Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
    }

    internal static string FilamentVersion() => PtrToString(FilamentVersionPtr());

    internal static string FilamentCommit() => PtrToString(FilamentCommitPtr());

    internal static string GetLastError() => PtrToString(GetLastErrorPtr());
}

/// <summary>Status codes mirroring fs_status in the ABI.</summary>
internal static class NativeStatus
{
    public const int Ok = 0;
    public const int Error = 1;
    public const int InvalidArg = 2;
    public const int Unsupported = 3;
    public const int Invalidated = 4;
    public const int NotReady = 5;
}

internal static class StatusExtensions
{
    /// <summary>
    /// Throws a <see cref="FilamentSharpException"/> with the native error
    /// text when the status is not FS_OK.
    /// </summary>
    public static void Check(this int status, string operation)
    {
        if (status != NativeStatus.Ok)
        {
            throw ThrowHelper.NativeFailure(operation);
        }
    }
}
