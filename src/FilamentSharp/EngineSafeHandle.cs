using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// SafeHandle for the native engine. Release goes through
/// <c>fs_engine_destroy</c>, which is idempotent and internally synchronized,
/// so releasing twice (or from a finalizer thread) is safe. The handle is
/// owned (not wrapped), which is exactly the ownership model of
/// <c>fs_engine_create</c>.
/// </summary>
internal sealed class EngineSafeHandle : SafeHandle
{
    public EngineSafeHandle(IntPtr handle)
        : base(IntPtr.Zero, ownsHandle: true)
    {
        SetHandle(handle);
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    protected override bool ReleaseHandle()
    {
        // fs_engine_destroy accepts null and is idempotent, so both the
        // zero-handle case and a repeated release are harmless.
        Native.EngineDestroy(handle);
        return true;
    }
}
