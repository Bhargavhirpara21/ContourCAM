using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using ContourCam.App.Logic;
using ContourCam.Interop;

namespace ContourCam.App.Controls;

/// <summary>
/// Immediate-mode 2D viewport: draws the part geometry (wire polylines + holes)
/// and the colored toolpath overlay, with mouse pan and wheel zoom. Scales to
/// thousands of segments better than a Canvas full of Shape objects (NFR-1).
/// </summary>
public sealed class ViewportControl : FrameworkElement
{
    private SceneModel _scene = new();
    private ViewTransform? _transform;
    private System.Windows.Point _lastMouse;
    private bool _panning;
    private bool _fitPending = true;

    private static readonly Pen WirePen = FrozenPen(Colors.LightGray, 1.4);
    private static readonly Pen FeedPen = FrozenPen(Colors.LimeGreen, 1.6);
    private static readonly Pen ArcPen = FrozenPen(Colors.Cyan, 1.6);
    private static readonly Pen RapidPen = FrozenDashedPen(Colors.OrangeRed, 1.0);
    private static readonly Pen HolePen = FrozenPen(Colors.Magenta, 1.2);
    private static readonly Brush HoleBrush = Frozen(new SolidColorBrush(Color.FromArgb(70, 255, 0, 255)));

    public ViewportControl()
    {
        ClipToBounds = true;
        Focusable = true;
    }

    public void SetScene(SceneModel scene)
    {
        _scene = scene;
        _fitPending = true;  // re-fit whenever a new part/overlay arrives
        InvalidateVisual();
    }

    protected override void OnRender(DrawingContext dc)
    {
        // Transparent backing so the whole surface is hit-test-able for pan/zoom.
        dc.DrawRectangle(Brushes.Transparent, null, new Rect(RenderSize));
        if (ActualWidth <= 0 || ActualHeight <= 0) return;

        if (_fitPending || _transform is null)
        {
            (double minX, double minY, double maxX, double maxY) = _scene.Bounds();
            _transform = ViewTransform.Fit(minX, minY, maxX, maxY, ActualWidth, ActualHeight, 24);
            _fitPending = false;
        }
        ViewTransform t = _transform;

        System.Windows.Point P(double x, double y)
        {
            (double sx, double sy) = t.ToScreen(x, y);
            return new System.Windows.Point(sx, sy);
        }

        // Part geometry (wire polylines).
        foreach (ScenePolyline w in _scene.Wires)
        {
            if (w.Points.Count < 2) continue;
            var geo = new StreamGeometry();
            using (StreamGeometryContext g = geo.Open())
            {
                g.BeginFigure(P(w.Points[0].X, w.Points[0].Y), false, true);
                for (int i = 1; i < w.Points.Count; i++)
                {
                    g.LineTo(P(w.Points[i].X, w.Points[i].Y), true, false);
                }
            }
            geo.Freeze();
            dc.DrawGeometry(null, WirePen, geo);
        }

        // Holes.
        foreach (SceneCircle h in _scene.Holes)
        {
            double r = h.Radius * t.Scale;
            dc.DrawEllipse(HoleBrush, HolePen, P(h.X, h.Y), r, r);
        }

        // Toolpath overlay (distinct colors per move kind).
        foreach (SceneMove m in _scene.Moves)
        {
            Pen pen = m.Kind switch
            {
                SegmentKind.Rapid => RapidPen,
                SegmentKind.ArcCw or SegmentKind.ArcCcw => ArcPen,
                _ => FeedPen,
            };
            dc.DrawLine(pen, P(m.X0, m.Y0), P(m.X1, m.Y1));
        }
    }

    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        if (_transform is null) return;
        System.Windows.Point p = e.GetPosition(this);
        double factor = e.Delta > 0 ? 1.1 : 1.0 / 1.1;
        _transform.ZoomAt(p.X, p.Y, factor);
        InvalidateVisual();
    }

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        _lastMouse = e.GetPosition(this);
        _panning = true;
        CaptureMouse();
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (!_panning || _transform is null) return;
        System.Windows.Point p = e.GetPosition(this);
        _transform.Pan(p.X - _lastMouse.X, p.Y - _lastMouse.Y);
        _lastMouse = p;
        InvalidateVisual();
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        _panning = false;
        ReleaseMouseCapture();
    }

    private static Pen FrozenPen(Color color, double thickness)
    {
        var pen = new Pen(new SolidColorBrush(color), thickness);
        pen.Freeze();
        return pen;
    }

    private static Pen FrozenDashedPen(Color color, double thickness)
    {
        var pen = new Pen(new SolidColorBrush(color), thickness)
        {
            DashStyle = new DashStyle(new double[] { 4, 3 }, 0),
        };
        pen.Freeze();
        return pen;
    }

    private static Brush Frozen(Brush brush)
    {
        brush.Freeze();
        return brush;
    }
}
