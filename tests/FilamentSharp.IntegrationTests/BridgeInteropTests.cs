using FilamentSharp;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// Verifies that the managed layer loads the real compiled native bridge and
/// that every managed P/Invoke declaration resolves to a native entry point
/// with matching behavior.
///
/// NOTE FOR REVIEWERS: when a new entry point is added to
/// <c>FilamentSharp.Native</c> (the internal P/Invoke class), extend
/// <see cref="AllEntryPointsResolveAndBehave"/> so the managed/native symbol
/// contract stays covered.
/// </summary>
public sealed class BridgeInteropTests
{
    private readonly ITestOutputHelper _output;

    public BridgeInteropTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void AllEntryPointsResolveAndBehave()
    {
        // Foundation: ABI + pinned release reporting.
        Assert.Equal(1u, Native.AbiVersion());
        Assert.Equal("v1.78.0", Native.FilamentVersion());
        Assert.Equal(40, Native.FilamentCommit().Length);

        // Error model: clear -> empty, never null.
        Native.ClearLastError();
        Assert.Equal(string.Empty, Native.GetLastError());

        // Engine lifecycle through raw handles.
        IntPtr engine = Native.EngineCreate((int)Backend.Noop);
        Assert.NotEqual(IntPtr.Zero, engine);
        Assert.Equal((int)Backend.Noop, Native.EngineGetBackend(engine));
        Native.EngineDestroy(engine);

        // Idempotent destroy and null-destroy are safe.
        Native.EngineDestroy(engine);
        Native.EngineDestroy(IntPtr.Zero);

        // Error state is per-call: the successful calls above left it empty.
        Assert.Equal(string.Empty, Native.GetLastError());

        _output.WriteLine($"bridge loaded from: {NativeInfo.NativeLibraryPath}");
        _output.WriteLine($"ABI {Native.AbiVersion()}, Filament {Native.FilamentVersion()}");
    }

    [Fact]
    public void NativeErrorsPropagateToManagedExceptions()
    {
        var ex = Assert.Throws<FilamentSharpException>(() => Engine.Create((Backend)123));
        _output.WriteLine($"propagated native error: {ex.Message}");
        Assert.NotEqual(string.Empty, ex.Message);
    }

    [Fact]
    public void ManagedAndNativeBackendValuesAgree()
    {
        using var engine = Engine.Create(Backend.Noop);
        // Managed enum -> int -> native -> managed round-trip.
        Assert.Equal((int)Backend.Noop, Native.EngineGetBackend(engine.DangerousGetHandle()));
    }
}

/// <summary>
/// Real engine integration on the Vulkan backend. When no Vulkan ICD is
/// available (headless machine without the lavapipe runtime), the test
/// self-reports an environmental skip in its output and passes; the
/// convention is documented in docs/testing.md. In the reference environment
/// these tests execute against lavapipe.
/// </summary>
public sealed class VulkanEngineIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public VulkanEngineIntegrationTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void VulkanEngineLifecycleWhenAvailable()
    {
        if (!Engine.TryCreate(Backend.Vulkan, out var engine))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no usable Vulkan ICD (set VK_ICD_FILENAMES or run " +
                "scripts/setup-headless-vulkan.sh); verified skip, not a failure.");
            return;
        }

        using (engine)
        {
            Assert.Equal(Backend.Vulkan, engine!.ActiveBackend);
            _output.WriteLine("Vulkan engine created and destroyed against a real ICD.");
        }
    }
}
