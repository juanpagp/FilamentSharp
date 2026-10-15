using System.Numerics;

namespace FilamentSharp;

/// <summary>Field-of-view direction for perspective projections.</summary>
public enum FovDirection
{
    /// <summary>The angle is defined on the vertical axis (default).</summary>
    Vertical = 0,

    /// <summary>The angle is defined on the horizontal axis.</summary>
    Horizontal = 1,
}

/// <summary>
/// A camera component attached to an entity. Create with
/// <see cref="Engine.CreateCamera(Entity)"/>; the entity can carry a
/// transform (see the transform API) to move the camera.
/// </summary>
public sealed class Camera : FilamentObject
{
    internal Camera(Engine engine, ChildSafeHandle handle, Entity entity)
        : base(engine, handle)
    {
        Entity = entity;
    }

    /// <summary>The entity this camera is attached to.</summary>
    public Entity Entity { get; }

    /// <summary>
    /// Sets a symmetric perspective projection.
    /// </summary>
    /// <param name="fovDegrees">Field of view in degrees (must be positive).</param>
    /// <param name="aspect">Aspect ratio width/height (positive).</param>
    /// <param name="near">Near plane distance (positive).</param>
    /// <param name="far">Far plane distance (must be greater than near).</param>
    /// <param name="direction">Which axis the FOV refers to.</param>
    public void SetProjection(double fovDegrees, double aspect, double near, double far,
        FovDirection direction = FovDirection.Vertical)
    {
        ThrowIfDisposed();
        Native.CameraSetProjection(Engine.DangerousGetHandle(), RawHandle,
            fovDegrees, aspect, near, far, (int)direction).Check(nameof(Camera.SetProjection));
    }

    /// <summary>
    /// Positions and orients the camera. The camera looks from
    /// <paramref name="eye"/> towards <paramref name="center"/>, with
    /// <paramref name="up"/> as the world-space up direction.
    /// </summary>
    public void LookAt(Vector3 eye, Vector3 center, Vector3? up = null)
    {
        ThrowIfDisposed();
        var upValue = up ?? Vector3.UnitY;
        ReadOnlySpan<double> eyeSpan = stackalloc double[3] { eye.X, eye.Y, eye.Z };
        ReadOnlySpan<double> centerSpan = stackalloc double[3] { center.X, center.Y, center.Z };
        ReadOnlySpan<double> upSpan = stackalloc double[3] { upValue.X, upValue.Y, upValue.Z };
        Native.CameraLookAt(Engine.DangerousGetHandle(), RawHandle, eyeSpan, centerSpan, upSpan)
            .Check(nameof(Camera.LookAt));
    }

    /// <summary>
    /// Gets the projection matrix (single precision; Filament computes in
    /// double precision internally).
    /// </summary>
    public Matrix4x4 ProjectionMatrix
    {
        get
        {
            ThrowIfDisposed();
            Span<float> matrix = stackalloc float[16];
            Native.CameraGetProjectionMatrix(Engine.DangerousGetHandle(), RawHandle, matrix)
                .Check(nameof(Camera.ProjectionMatrix));
            return MatrixInterop.FromColumnMajor(matrix);
        }
    }
}
