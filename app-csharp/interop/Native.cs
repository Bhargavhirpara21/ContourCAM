using System.Runtime.InteropServices;

namespace ContourCam.Interop;

// The one P/Invoke surface for the whole flat C ABI. Handle-consuming calls take
// the SafeHandle subtype (the marshaller keeps it alive for the call); the raw
// IntPtr free functions are used only by the SafeHandle ReleaseHandle overrides.
internal static class Native
{
    private const string Lib = "contourcam_core";
    private const CallingConvention Cdecl = CallingConvention.Cdecl;

    // ---- Diagnostics ----
    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr cc_version();

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_add(int a, int b, out int result);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr cc_last_error();

    // ---- Document ----
    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern int cc_load_dxf([MarshalAs(UnmanagedType.LPStr)] string path, out IntPtr doc);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_wire_count(DocumentHandle doc, out int count);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_outer_count(DocumentHandle doc, out int count);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_circle_count(DocumentHandle doc, out int count);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_get_circles(DocumentHandle doc, [Out] Circle[] buf,
                                                     int capacity, out int written);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_wire_point_count(DocumentHandle doc, int wireIndex,
                                                          out int count);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_document_get_wire_points(DocumentHandle doc, int wireIndex,
                                                         [Out] Point[] buf, int capacity,
                                                         out int written);

    [DllImport(Lib, CallingConvention = Cdecl)]
    internal static extern int cc_free_document(IntPtr doc);

    // ---- Toolpath + G-code ----
    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_generate_toolpath(DocumentHandle doc, in ToolParams tool,
                                                  in JobParams job, out IntPtr tp);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_toolpath_segment_count(ToolpathHandle tp, out int count);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int cc_toolpath_get_segments(ToolpathHandle tp, [Out] Segment[] buf,
                                                      int capacity, out int written);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern int cc_export_gcode(ToolpathHandle tp,
                                             [MarshalAs(UnmanagedType.LPStr)] string path,
                                             in PostParams post);

    [DllImport(Lib, CallingConvention = Cdecl)]
    internal static extern int cc_free_toolpath(IntPtr tp);

    // ---- Helpers ----
    public static string Version() => Marshal.PtrToStringAnsi(cc_version()) ?? string.Empty;

    public static string LastError() => Marshal.PtrToStringAnsi(cc_last_error()) ?? string.Empty;

    public static void Check(int status, string operation)
    {
        if (status != 0)
        {
            throw new ContourCamException(operation, (CcStatus)status, LastError());
        }
    }
}
