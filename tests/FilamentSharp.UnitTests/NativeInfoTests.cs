using System.Text.RegularExpressions;
using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>
/// Version contracts against the real native bridge. These tests load the
/// compiled bridge (no GPU required) and pin the dependency identity.
/// </summary>
public sealed partial class NativeInfoTests
{
    [Fact]
    public void AbiVersionMatchesManagedContract()
    {
        // The managed layer is written against ABI version 1. Bump this test
        // together with FILAMENTSHARP_ABI_VERSION when the ABI changes.
        Assert.Equal(1u, NativeInfo.AbiVersion);
    }

    [Fact]
    public void FilamentVersionMatchesPinnedRelease()
    {
        // Single source of truth: deps/filament.lock. A change of the pinned
        // release must be visible here.
        Assert.Equal("v1.78.0", NativeInfo.FilamentVersion);
    }

    [Fact]
    public void FilamentCommitIsFullSha1()
    {
        Assert.Matches("^[0-9a-f]{40}$", NativeInfo.FilamentCommit);
    }

    [Fact]
    public void NativeLibraryPathIsResolved()
    {
        Assert.NotNull(NativeInfo.NativeLibraryPath);
        Assert.EndsWith("libFilamentSharp.Native.so", NativeInfo.NativeLibraryPath);
        Assert.True(File.Exists(NativeInfo.NativeLibraryPath));
    }

    [Fact]
    public void BackendEnumValuesMatchAbi()
    {
        // The numeric values are part of the ABI contract (see abi.h).
        Assert.Equal(0, (int)Backend.Default);
        Assert.Equal(1, (int)Backend.OpenGl);
        Assert.Equal(2, (int)Backend.Vulkan);
        Assert.Equal(3, (int)Backend.Metal);
        Assert.Equal(4, (int)Backend.WebGpu);
        Assert.Equal(5, (int)Backend.Noop);
    }
}
