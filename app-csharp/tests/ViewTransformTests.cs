using ContourCam.App.Logic;
using Xunit;

namespace ContourCam.Tests;

public class ViewTransformTests
{
    private static ViewTransform Fit() => ViewTransform.Fit(0, 0, 100, 80, 400, 300, 10);

    [Fact]
    public void FitCentersModelBox()
    {
        var (sx, sy) = Fit().ToScreen(50, 40);  // model centre -> viewport centre
        Assert.Equal(200, sx, 6);
        Assert.Equal(150, sy, 6);
    }

    [Fact]
    public void FitIsUniformAndLetterboxed()
    {
        // width gives (400-20)/100 = 3.8, height gives (300-20)/80 = 3.5 -> min = 3.5
        Assert.Equal(3.5, Fit().Scale, 6);
    }

    [Fact]
    public void ScreenModelRoundTrips()
    {
        ViewTransform t = Fit();
        var (sx, sy) = t.ToScreen(30, 25);
        var (mx, my) = t.ToModel(sx, sy);
        Assert.Equal(30, mx, 6);
        Assert.Equal(25, my, 6);
    }

    [Fact]
    public void YAxisIsFlipped()
    {
        ViewTransform t = Fit();
        var (_, yLow) = t.ToScreen(50, 0);
        var (_, yHigh) = t.ToScreen(50, 80);
        Assert.True(yHigh < yLow);  // larger model Y -> smaller screen Y
    }

    [Fact]
    public void ZoomAboutCursorKeepsModelPointFixed()
    {
        ViewTransform t = Fit();
        const double sx = 123, sy = 77;
        var (mx0, my0) = t.ToModel(sx, sy);
        t.ZoomAt(sx, sy, 1.5);
        var (mx1, my1) = t.ToModel(sx, sy);
        Assert.Equal(mx0, mx1, 6);
        Assert.Equal(my0, my1, 6);
    }

    [Fact]
    public void MarginIsRespected()
    {
        ViewTransform t = Fit();
        var (sx, sy) = t.ToScreen(0, 80);  // top-left model corner
        Assert.True(sx >= 10 - 1e-6);
        Assert.True(sy >= 10 - 1e-6);
    }
}
