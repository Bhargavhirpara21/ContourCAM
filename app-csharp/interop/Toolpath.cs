namespace ContourCam.Interop;

/// <summary>A generated toolpath. Owns a native handle; dispose with <c>using</c>.</summary>
public sealed class Toolpath : IDisposable
{
    private readonly ToolpathHandle _handle;

    internal Toolpath(ToolpathHandle handle) => _handle = handle;

    public int SegmentCount
    {
        get
        {
            Native.Check(Native.cc_toolpath_segment_count(_handle, out int n),
                         "cc_toolpath_segment_count");
            return n;
        }
    }

    public IReadOnlyList<Segment> GetSegments()
    {
        int n = SegmentCount;
        var buf = new Segment[n];
        Native.Check(Native.cc_toolpath_get_segments(_handle, buf, n, out int written),
                     "cc_toolpath_get_segments");
        return written == n ? buf : buf[..written];
    }

    public void ExportGcode(string path, in PostParams post)
    {
        Native.Check(Native.cc_export_gcode(_handle, path, in post), "cc_export_gcode");
    }

    public void Dispose() => _handle.Dispose();
}
