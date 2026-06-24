using ContourCam.Interop;

namespace ContourCam.App.Logic;

/// <summary>Builds the render <see cref="SceneModel"/> from a native document/toolpath.</summary>
public static class SceneBuilder
{
    public static SceneModel BuildGeometry(Document doc)
    {
        var scene = new SceneModel();
        int wires = doc.WireCount;
        for (int i = 0; i < wires; i++)
        {
            IReadOnlyList<Point> pts = doc.GetWirePoints(i);
            if (pts.Count == 0) continue;
            var sp = new List<ScenePoint>(pts.Count);
            foreach (Point p in pts) sp.Add(new ScenePoint(p.X, p.Y));
            scene.Wires.Add(new ScenePolyline(sp));
        }
        foreach (Circle c in doc.GetCircles())
        {
            scene.Holes.Add(new SceneCircle(c.X, c.Y, c.Radius));
        }
        return scene;
    }

    /// <summary>Flatten toolpath segments to drawable XY moves (kind preserved for colouring).</summary>
    public static void AddToolpath(SceneModel scene, IReadOnlyList<Segment> segments)
    {
        scene.Moves.Clear();
        bool have = false;
        double lx = 0, ly = 0;
        foreach (Segment s in segments)
        {
            if (have)
            {
                scene.Moves.Add(new SceneMove((SegmentKind)s.Kind, lx, ly, s.X, s.Y));
            }
            lx = s.X;
            ly = s.Y;
            have = true;
        }
    }
}
