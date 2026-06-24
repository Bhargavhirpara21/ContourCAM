using ContourCam.App.Logic;
using ContourCam.Interop;
using Xunit;

namespace ContourCam.Tests;

// Round-trip tests against the real native core. They run only when
// CONTOURCAM_LIB_DIR points at a built core (so the resolver can find the DLL);
// otherwise they no-op so the pure tests still run on a machine without a build.
public class InteropTests
{
    private static readonly string? LibDir = Environment.GetEnvironmentVariable("CONTOURCAM_LIB_DIR");
    private static bool NativeAvailable => !string.IsNullOrEmpty(LibDir);

    private static string SamplePath()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 10 && dir is not null; i++, dir = dir.Parent)
        {
            string candidate = Path.Combine(dir.FullName, "samples", "plate_pocket_holes.dxf");
            if (File.Exists(candidate)) return candidate;
        }
        return Path.Combine("samples", "plate_pocket_holes.dxf");
    }

    [Fact]
    public void LoadSampleAndQuery()
    {
        if (!NativeAvailable) return;
        using Document doc = Document.Load(SamplePath());
        Assert.Equal(1, doc.OuterCount);
        Assert.Equal(4, doc.CircleCount);
        Assert.Equal(6, doc.WireCount);  // outer + pocket + 4 circle-wires
        Assert.All(doc.GetCircles(), c => Assert.Equal(3.0, c.Radius, 6));
        Assert.True(doc.GetWirePoints(0).Count >= 3);
    }

    [Fact]
    public void GenerateAndExportIsDeterministic()
    {
        if (!NativeAvailable) return;
        using Document doc = Document.Load(SamplePath());
        ToolParams tool = new CutParameters().ToTool();
        JobParams job = new CutParameters().ToJob();
        using Toolpath tp = doc.GenerateToolpath(in tool, in job);
        Assert.True(tp.SegmentCount > 1);

        string a = Path.GetTempFileName();
        string b = Path.GetTempFileName();
        var post = new PostParams { Metric = 1, Coolant = 0, ToolNumber = 1 };
        tp.ExportGcode(a, in post);
        tp.ExportGcode(b, in post);
        Assert.Equal(File.ReadAllText(a), File.ReadAllText(b));
        Assert.Contains("M30", File.ReadAllText(a));
        File.Delete(a);
        File.Delete(b);
    }

    [Fact]
    public void MissingFileThrowsParse()
    {
        if (!NativeAvailable) return;
        ContourCamException ex = Assert.Throws<ContourCamException>(
            () => Document.Load("definitely_not_here.dxf"));
        Assert.Equal(CcStatus.Parse, ex.Status);
    }

    [Fact]
    public async Task ViewModelLoadsAndGenerates()
    {
        if (!NativeAvailable) return;
        using var vm = new MainViewModel();
        await vm.LoadAsync(SamplePath());
        Assert.True(vm.CanGenerate);
        Assert.True(vm.Scene.HasGeometry);

        await vm.GenerateAsync();
        Assert.True(vm.CanExport);
        Assert.True(vm.Scene.HasToolpath);
    }
}
