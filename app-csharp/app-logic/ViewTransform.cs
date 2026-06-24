namespace ContourCam.App.Logic;

/// <summary>
/// Model(mm, Y-up) &lt;-&gt; screen(px, Y-down) mapping: a uniform scale plus an
/// offset, with the Y axis flipped. Pure math (no WPF) so it is unit-testable.
/// </summary>
public sealed class ViewTransform
{
    public double Scale { get; private set; } = 1.0;
    public double OffsetX { get; private set; }
    public double OffsetY { get; private set; }

    /// <summary>Fit a model bounding box into a width x height viewport with a px margin.</summary>
    public static ViewTransform Fit(double minX, double minY, double maxX, double maxY,
                                    double width, double height, double margin)
    {
        double bw = Math.Max(maxX - minX, 1e-9);
        double bh = Math.Max(maxY - minY, 1e-9);
        double sx = (width - 2 * margin) / bw;
        double sy = (height - 2 * margin) / bh;
        double s = Math.Min(sx, sy);
        if (!(s > 0) || double.IsInfinity(s)) s = 1.0;

        double cx = (minX + maxX) / 2.0;
        double cy = (minY + maxY) / 2.0;
        return new ViewTransform
        {
            Scale = s,
            OffsetX = width / 2.0 - s * cx,
            OffsetY = height / 2.0 + s * cy,  // + because of the Y flip
        };
    }

    public (double X, double Y) ToScreen(double modelX, double modelY)
        => (Scale * modelX + OffsetX, -Scale * modelY + OffsetY);

    public (double X, double Y) ToModel(double screenX, double screenY)
        => ((screenX - OffsetX) / Scale, (OffsetY - screenY) / Scale);

    public void Pan(double dxPixels, double dyPixels)
    {
        OffsetX += dxPixels;
        OffsetY += dyPixels;
    }

    /// <summary>Zoom by <paramref name="factor"/> keeping the model point under the cursor fixed.</summary>
    public void ZoomAt(double screenX, double screenY, double factor)
    {
        (double mx, double my) = ToModel(screenX, screenY);
        Scale *= factor;
        OffsetX = screenX - Scale * mx;
        OffsetY = screenY + Scale * my;
    }
}
