namespace ContourCam.Interop;

/// <summary>
/// A parsed + classified DXF part. Owns a native handle; dispose with
/// <c>using</c> (a SafeHandle finalizer is the backstop).
/// </summary>
public sealed class Document : IDisposable
{
    private readonly DocumentHandle _handle;

    private Document(DocumentHandle handle) => _handle = handle;

    /// <summary>Parse a DXF file into a document.</summary>
    public static Document Load(string path)
    {
        CoreLibrary.EnsureLoaded();
        Native.Check(Native.cc_load_dxf(path, out IntPtr raw), "cc_load_dxf");
        return new Document(new DocumentHandle(raw));
    }

    /// <summary>Number of closed wires (outer profile + pockets + holes).</summary>
    public int WireCount
    {
        get
        {
            Native.Check(Native.cc_document_wire_count(_handle, out int n), "cc_document_wire_count");
            return n;
        }
    }

    /// <summary>Number of outermost (depth-0) boundaries.</summary>
    public int OuterCount
    {
        get
        {
            Native.Check(Native.cc_document_outer_count(_handle, out int n), "cc_document_outer_count");
            return n;
        }
    }

    /// <summary>Number of detected circles (drill candidates).</summary>
    public int CircleCount
    {
        get
        {
            Native.Check(Native.cc_document_circle_count(_handle, out int n), "cc_document_circle_count");
            return n;
        }
    }

    public IReadOnlyList<Circle> GetCircles()
    {
        int n = CircleCount;
        var buf = new Circle[n];
        Native.Check(Native.cc_document_get_circles(_handle, buf, n, out int written),
                     "cc_document_get_circles");
        return written == n ? buf : buf[..written];
    }

    /// <summary>Polyline points of closed wire <paramref name="wireIndex"/> (0 .. WireCount-1).</summary>
    public IReadOnlyList<Point> GetWirePoints(int wireIndex)
    {
        Native.Check(Native.cc_document_wire_point_count(_handle, wireIndex, out int n),
                     "cc_document_wire_point_count");
        var buf = new Point[n];
        Native.Check(Native.cc_document_get_wire_points(_handle, wireIndex, buf, n, out int written),
                     "cc_document_get_wire_points");
        return written == n ? buf : buf[..written];
    }

    public Toolpath GenerateToolpath(in ToolParams tool, in JobParams job)
    {
        Native.Check(Native.cc_generate_toolpath(_handle, in tool, in job, out IntPtr raw),
                     "cc_generate_toolpath");
        return new Toolpath(new ToolpathHandle(raw));
    }

    public void Dispose() => _handle.Dispose();
}
