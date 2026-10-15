using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Base class for SafeHandles wrapping engine-owned native resources. The
/// handle stores the raw engine handle (whose native wrapper is immortal,
/// see abi.h) so the release path works even after the managed
/// <see cref="Engine"/> has been disposed or collected: the native destroy
/// then takes its documented no-op path.
/// </summary>
internal abstract class FilamentChildSafeHandle : SafeHandle
{
    protected FilamentChildSafeHandle(IntPtr handle, IntPtr engine)
        : base(IntPtr.Zero, ownsHandle: true)
    {
        EngineHandle = engine;
        SetHandle(handle);
    }

    /// <summary>Raw handle of the owning engine (never dereferenced here).</summary>
    internal IntPtr EngineHandle { get; }

    public override bool IsInvalid => handle == IntPtr.Zero;
}
