using ContourCam.App.Logic;
using ContourCam.Interop;
using Xunit;

namespace ContourCam.Tests;

public class CutParametersTests
{
    [Fact]
    public void DefaultsMatchSpecSection7()
    {
        var p = new CutParameters();
        Assert.Equal(6.0, p.ToolDiameterMm, 6);
        Assert.Equal(2.0, p.StepDownMm, 6);
        Assert.Equal(0.45, p.StepoverFrac, 6);
        Assert.Equal(600.0, p.Feed, 6);
        Assert.Equal(200.0, p.PlungeFeed, 6);
        Assert.Equal(10000.0, p.SpindleRpm, 6);
        Assert.Equal(5.0, p.SafeZmm, 6);
        Assert.Equal(ToolType.EndMill, p.ToolType);
        Assert.Equal(CutDirection.Climb, p.Direction);
    }

    [Fact]
    public void ProjectsToNativeStructs()
    {
        var p = new CutParameters
        {
            ToolDiameterMm = 8.0,
            ToolType = ToolType.Drill,
            Direction = CutDirection.Conventional,
            Metric = false,
            ToolNumber = 3,
        };
        Assert.Equal(8.0, p.ToTool().DiameterMm, 6);
        Assert.Equal((int)ToolType.Drill, p.ToTool().Type);
        Assert.Equal((int)CutDirection.Conventional, p.ToJob().Direction);
        Assert.Equal(0, p.ToPost().Metric);
        Assert.Equal(3, p.ToPost().ToolNumber);
    }
}

public class SceneModelTests
{
    [Fact]
    public void BoundsCoverWiresAndHoles()
    {
        var s = new SceneModel();
        s.Wires.Add(new ScenePolyline(new[]
        {
            new ScenePoint(0, 0), new ScenePoint(10, 0), new ScenePoint(10, 5),
        }));
        s.Holes.Add(new SceneCircle(20, 20, 3));

        (double minX, double minY, double maxX, double maxY) = s.Bounds();
        Assert.Equal(0, minX, 6);
        Assert.Equal(0, minY, 6);
        Assert.Equal(23, maxX, 6);  // hole centre 20 + radius 3
        Assert.Equal(23, maxY, 6);
    }

    [Fact]
    public void AddToolpathFlattensToMoves()
    {
        var s = new SceneModel();
        var segs = new[]
        {
            new Segment { Kind = (int)SegmentKind.Rapid, X = 0, Y = 0 },
            new Segment { Kind = (int)SegmentKind.Feed, X = 10, Y = 0 },
            new Segment { Kind = (int)SegmentKind.Feed, X = 10, Y = 5 },
        };
        SceneBuilder.AddToolpath(s, segs);
        Assert.Equal(2, s.Moves.Count);  // n-1 moves
        Assert.Equal(SegmentKind.Feed, s.Moves[0].Kind);
        Assert.Equal(10, s.Moves[0].X1, 6);
    }
}

public class MainViewModelStateTests
{
    [Fact]
    public void InitialStateBlocksGenerateAndExport()
    {
        using var vm = new MainViewModel();
        Assert.False(vm.CanGenerate);
        Assert.False(vm.CanExport);
        Assert.Equal(6.0, vm.Parameters.ToolDiameterMm, 6);
        Assert.False(vm.Scene.HasGeometry);
    }
}
