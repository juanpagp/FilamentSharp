using FilamentSharp;
using Xunit;

namespace FilamentSharp.UnitTests;

/// <summary>Test-infrastructure smoke tests: they verify that the managed
/// library is loadable and the test harness executes correctly. Functional
/// API tests are added together with each feature commit.</summary>
public sealed class ScaffoldTests
{
    [Fact]
    public void LibraryRunsOnDotNetCoreRuntime()
    {
        Assert.StartsWith(".NETCoreApp", FilamentSharpInfo.TargetFramework);
    }

    [Fact]
    public void LibraryReportsAssemblyVersion()
    {
        Assert.Matches(@"^\d+\.\d+\.\d+$", FilamentSharpInfo.Version);
    }
}
