using System.Numerics;
using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Base class for managed wrappers of engine-owned Filament objects.
/// Implements the shared lifetime rules:
///   * disposal is idempotent and goes through a <see cref="SafeHandle"/>,
///     so native release also happens from the finalizer thread when the
///     user forgets to dispose;
///   * operations after the wrapper is disposed throw
///     <see cref="ObjectDisposedException"/>;
///   * operations after the owning engine is disposed throw
///     <see cref="ObjectDisposedException"/> (the native bridge would also
///     reject them with FS_ERROR_INVALIDATED; the managed check gives a
///     better error and avoids the interop hop).
/// </summary>
public abstract class FilamentObject : IDisposable
{
    private protected FilamentObject(Engine engine, ChildSafeHandle handle)
    {
        Engine = engine;
        Handle = handle;
    }

    /// <summary>The engine this object was created from.</summary>
    public Engine Engine { get; }

    /// <summary>Gets a value indicating whether this object has been disposed.</summary>
    public bool IsDisposed => Handle.IsClosed;

    internal ChildSafeHandle Handle { get; }

    /// <summary>Releases the native resource. Idempotent.</summary>
    public void Dispose()
    {
        // SafeHandle.Dispose is idempotent; the native destroy is too.
        Handle.Dispose();
        GC.SuppressFinalize(this);
    }

    /// <summary>
    /// Throws when this object or its engine has been disposed. Called before
    /// every operation that talks to native code.
    /// </summary>
    internal void ThrowIfDisposed()
    {
        if (Handle.IsClosed)
        {
            throw new ObjectDisposedException(GetType().Name,
                "This Filament object has been disposed.");
        }
        if (Engine.IsDisposed)
        {
            throw new ObjectDisposedException(nameof(Engine),
                "The owning Filament engine has been disposed; all objects " +
                "created from it are invalidated.");
        }
    }

    /// <summary>The raw native handle, after validating it is usable.</summary>
    internal IntPtr RawHandle
    {
        get
        {
            ThrowIfDisposed();
            return Handle.DangerousGetHandle();
        }
    }
}

/// <summary>
/// Generic SafeHandle for engine-owned child resources. The release delegate
/// receives (engineHandle, resourceHandle) and calls the matching native
/// destroy function; the native side treats "engine already destroyed" as a
/// documented no-op, so releasing after engine disposal is safe.
/// </summary>
internal sealed class ChildSafeHandle : FilamentChildSafeHandle
{
    private readonly Action<IntPtr, IntPtr> _release;

    public ChildSafeHandle(IntPtr handle, IntPtr engine, Action<IntPtr, IntPtr> release)
        : base(handle, engine)
    {
        _release = release;
    }

    protected override bool ReleaseHandle()
    {
        _release(EngineHandle, handle);
        return true;
    }
}

/// <summary>Column-major 4x4 matrix helpers for Filament interop.</summary>
/// <remarks>
/// Filament's <c>mat4</c> is column-vector with column-major storage:
/// <c>mat[col][row]</c> holds the element at (row, col). System.Numerics
/// matrices are row-vector: the equivalent matrix is the transpose, so both
/// conversions below transpose. Feeding the System.Numerics elements through
/// untransposed puts translations into the W row (a perspective shear) —
/// verified by pixel readback on Vulkan (see TransformIntegrationTests):
/// a +0.5 X translation warped the triangle through <c>w' = 0.5x + 1</c>
/// instead of moving it.
/// </remarks>
internal static class MatrixInterop
{
    /// <summary>Builds a System.Numerics matrix from Filament's column-major layout.</summary>
    public static Matrix4x4 FromColumnMajor(ReadOnlySpan<float> m)
    {
        return new Matrix4x4(
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]);
    }

    /// <summary>Writes a System.Numerics matrix into Filament's column-major layout.</summary>
    public static void ToColumnMajor(in Matrix4x4 matrix, Span<float> destination)
    {
        destination[0] = matrix.M11;
        destination[1] = matrix.M12;
        destination[2] = matrix.M13;
        destination[3] = matrix.M14;
        destination[4] = matrix.M21;
        destination[5] = matrix.M22;
        destination[6] = matrix.M23;
        destination[7] = matrix.M24;
        destination[8] = matrix.M31;
        destination[9] = matrix.M32;
        destination[10] = matrix.M33;
        destination[11] = matrix.M34;
        destination[12] = matrix.M41;
        destination[13] = matrix.M42;
        destination[14] = matrix.M43;
        destination[15] = matrix.M44;
    }
}
