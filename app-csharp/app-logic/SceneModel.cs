using ContourCam.Interop;

namespace ContourCam.App.Logic;

public sealed record ScenePoint(double X, double Y);

public sealed record ScenePolyline(IReadOnlyList<ScenePoint> Points);

public sealed record SceneCircle(double X, double Y, double Radius);

public sealed record SceneMove(SegmentKind Kind, double X0, double Y0, double X1, double Y1);

/// <summary>Immutable-ish render snapshot the viewport draws (decoupled from native handles).</summary>
public sealed class SceneModel
{
    public List<ScenePolyline> Wires { get; } = new();
    public List<SceneCircle> Holes { get; } = new();
    public List<SceneMove> Moves { get; } = new();

    public bool HasGeometry => Wires.Count > 0 || Holes.Count > 0;
    public bool HasToolpath => Moves.Count > 0;

    /// <summary>Model-space bounds over geometry (falls back to a unit box if empty).</summary>
    public (double MinX, double MinY, double MaxX, double MaxY) Bounds()
    {
        double minX = double.MaxValue, minY = double.MaxValue;
        double maxX = double.MinValue, maxY = double.MinValue;

        void Acc(double x, double y)
        {
            minX = Math.Min(minX, x);
            minY = Math.Min(minY, y);
            maxX = Math.Max(maxX, x);
            maxY = Math.Max(maxY, y);
        }

        foreach (ScenePolyline w in Wires)
        {
            foreach (ScenePoint p in w.Points) Acc(p.X, p.Y);
        }
        foreach (SceneCircle h in Holes)
        {
            Acc(h.X - h.Radius, h.Y - h.Radius);
            Acc(h.X + h.Radius, h.Y + h.Radius);
        }

        if (minX > maxX) return (0, 0, 1, 1);
        return (minX, minY, maxX, maxY);
    }
}
