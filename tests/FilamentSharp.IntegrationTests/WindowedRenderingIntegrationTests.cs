using System.Diagnostics;
using Xunit;
using Xunit.Abstractions;

namespace FilamentSharp.IntegrationTests;

/// <summary>
/// End-to-end smoke of the WindowedRendering sample: a real X11 window with a
/// native-window swap chain, a finite frame budget, and a clean shutdown.
///
/// Gated on the environment: the sample needs a reachable X server (DISPLAY)
/// and a usable Vulkan ICD. On headless machines without DISPLAY the test
/// self-reports an environmental skip in its output (convention documented in
/// docs/testing.md) — on machines *with* a display it executes for real and
/// any failure is a regression, not a skip.
/// </summary>
public sealed class WindowedRenderingIntegrationTests
{
    private readonly ITestOutputHelper _output;

    public WindowedRenderingIntegrationTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void WindowedRenderingSampleRunsAndShutsDownCleanly()
    {
        if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("DISPLAY")))
        {
            _output.WriteLine(
                "ENVIRONMENTAL SKIP: no DISPLAY (headless machine). Windowed rendering " +
                "needs a reachable X server - a real desktop session or Xvfb " +
                "(recipe in docs/windowed-rendering.md).");
            return;
        }

        var root = RepoRoot();
        var psi = new ProcessStartInfo
        {
            FileName = "dotnet",
            Arguments = "run --no-build --project samples/WindowedRendering -- " +
                        "--frames 5 --width 128 --height 96 --fps 60",
            WorkingDirectory = root,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        _output.WriteLine($"running: dotnet {psi.Arguments} (in {root})");

        using var process = Process.Start(psi)!;
        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        bool exited = process.WaitForExit(120_000);
        if (!exited)
        {
            process.Kill(true);
            Assert.Fail("WindowedRendering sample did not exit within 120s.");
        }

        _output.WriteLine(stdout);
        if (!string.IsNullOrWhiteSpace(stderr))
        {
            _output.WriteLine("[stderr] " + stderr);
        }

        Assert.Equal(0, process.ExitCode);
        Assert.Contains("rendered 5 frame(s)", stdout);
        Assert.Contains("completed cleanly", stdout);
        _output.WriteLine("windowed sample rendered and shut down cleanly on a real display");
    }

    private static string RepoRoot()
    {
        var dir = AppContext.BaseDirectory;
        for (int i = 0; i < 8; i++)
        {
            if (File.Exists(Path.Combine(dir, "FilamentSharp.sln")))
            {
                return dir;
            }
            var parent = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
            if (string.IsNullOrEmpty(parent))
            {
                break;
            }
            dir = parent;
        }
        throw new InvalidOperationException("Repository root (FilamentSharp.sln) not found.");
    }
}
