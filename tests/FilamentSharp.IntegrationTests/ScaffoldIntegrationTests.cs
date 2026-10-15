using FilamentSharp;
using Xunit;

namespace FilamentSharp.IntegrationTests;

/// <summary>Placeholder integration suite. Real interop integration tests
/// (native bridge loading, entry-point resolution, engine lifecycle against
/// the compiled bridge) are added with the interop feature commits; see
/// docs/testing.md for the current state.</summary>
public sealed class ScaffoldIntegrationTests
{
    [Fact]
    public void ManagedLibraryIsLoadableInTestProcess()
    {
        Assert.False(string.IsNullOrEmpty(FilamentSharpInfo.Version));
    }
}
