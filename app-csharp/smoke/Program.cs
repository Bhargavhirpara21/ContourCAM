using System;
using System.Runtime.InteropServices;

namespace ContourCam.Interop.Smoke;

/// <summary>
/// P/Invoke declarations for the ContourCAM native core (flat C ABI). The
/// library name "contourcam_core" resolves to contourcam_core.dll on Windows and
/// libcontourcam_core.so on Linux; it must sit next to this executable (or on the
/// loader path) at run time. x64 on both sides (interop hard rule).
/// </summary>
internal static class Native
{
    private const string Lib = "contourcam_core";

    // cc_circle POD struct -- layout must match the C header exactly.
    [StructLayout(LayoutKind.Sequential)]
    public struct Circle
    {
        public double X;
        public double Y;
        public double Radius;
    }

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr cc_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_add(int a, int b, out int result);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr cc_last_error();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int cc_load_dxf([MarshalAs(UnmanagedType.LPStr)] string path, out IntPtr doc);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_document_outer_count(IntPtr doc, out int count);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_document_circle_count(IntPtr doc, out int count);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_document_get_circles(IntPtr doc, [Out] Circle[] buf, int capacity,
                                                     out int written);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_free_document(IntPtr doc);

    [StructLayout(LayoutKind.Sequential)]
    public struct ToolParams
    {
        public double DiameterMm;
        public int Flutes;
        public int Type;  // 0 = end mill, 1 = drill
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct JobParams
    {
        public double TargetDepthMm;
        public double StepDownMm;
        public double StepoverFrac;
        public double Feed;
        public double PlungeFeed;
        public double SpindleRpm;
        public double SafeZmm;
        public int Direction;  // 0 = climb, 1 = conventional
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PostParams
    {
        public int Metric;       // 1 = G21 mm
        public int Coolant;
        public int ToolNumber;
    }

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_generate_toolpath(IntPtr doc, in ToolParams tool, in JobParams job,
                                                  out IntPtr tp);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_toolpath_segment_count(IntPtr tp, out int count);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int cc_export_gcode(IntPtr tp, [MarshalAs(UnmanagedType.LPStr)] string path,
                                             in PostParams post);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_free_toolpath(IntPtr tp);

    public static string Version() => Marshal.PtrToStringAnsi(cc_version()) ?? string.Empty;

    public static string LastError() => Marshal.PtrToStringAnsi(cc_last_error()) ?? string.Empty;
}

internal static class Program
{
    private static int Main(string[] args)
    {
        Console.WriteLine($"[C#] core version: {Native.Version()}");

        // 1. Bridge liveness check.
        if (Native.cc_add(2, 3, out int sum) != 0 || sum != 5)
        {
            Console.Error.WriteLine($"[C#] FAIL: cc_add(2,3) = {sum}: {Native.LastError()}");
            return 1;
        }
        Console.WriteLine($"[C#] cc_add(2, 3) = {sum}");

        // 2. Real geometry through the ABI: load the sample part and query it.
        string dxf = args.Length > 0 ? args[0] : "samples/plate_pocket_holes.dxf";

        if (Native.cc_load_dxf(dxf, out IntPtr doc) != 0)
        {
            Console.Error.WriteLine($"[C#] FAIL: cc_load_dxf('{dxf}'): {Native.LastError()}");
            return 2;
        }

        try
        {
            // Check the status of every ABI call before trusting its out-value:
            // a failed count must not be used to size the circle buffer.
            if (Native.cc_document_outer_count(doc, out int outers) != 0)
            {
                Console.Error.WriteLine($"[C#] FAIL: outer_count: {Native.LastError()}");
                return 3;
            }
            if (Native.cc_document_circle_count(doc, out int circleCount) != 0)
            {
                Console.Error.WriteLine($"[C#] FAIL: circle_count: {Native.LastError()}");
                return 3;
            }
            Console.WriteLine($"[C#] {dxf}: outer={outers}, circles={circleCount}");

            var circles = new Native.Circle[circleCount];
            if (Native.cc_document_get_circles(doc, circles, circles.Length, out int written) != 0)
            {
                Console.Error.WriteLine($"[C#] FAIL: get_circles: {Native.LastError()}");
                return 4;
            }
            for (int i = 0; i < written; i++)  // only the validly-copied prefix
            {
                Native.Circle c = circles[i];
                Console.WriteLine($"[C#]   hole @ ({c.X:0.#}, {c.Y:0.#}) r={c.Radius:0.#}");
            }

            if (outers != 1 || circleCount != 4 || written != 4)
            {
                Console.Error.WriteLine($"[C#] FAIL: expected outer=1, circles=4 (got {outers}, {circleCount}, written {written})");
                return 5;
            }

            // 3. Generate an outer-contour toolpath and export ISO G-code.
            var tool = new Native.ToolParams { DiameterMm = 6.0, Flutes = 2, Type = 0 };
            var job = new Native.JobParams
            {
                TargetDepthMm = 6.0, StepDownMm = 2.0, StepoverFrac = 0.45,
                Feed = 600, PlungeFeed = 200, SpindleRpm = 10000, SafeZmm = 5.0, Direction = 0,
            };
            if (Native.cc_generate_toolpath(doc, tool, job, out IntPtr tp) != 0)
            {
                Console.Error.WriteLine($"[C#] FAIL: generate_toolpath: {Native.LastError()}");
                return 6;
            }
            try
            {
                if (Native.cc_toolpath_segment_count(tp, out int segCount) != 0)
                {
                    Console.Error.WriteLine($"[C#] FAIL: segment_count: {Native.LastError()}");
                    return 7;
                }
                var post = new Native.PostParams { Metric = 1, Coolant = 0, ToolNumber = 1 };
                string gpath = args.Length > 1 ? args[1] : "contourcam_csharp.gcode";
                if (Native.cc_export_gcode(tp, gpath, post) != 0)
                {
                    Console.Error.WriteLine($"[C#] FAIL: export_gcode: {Native.LastError()}");
                    return 8;
                }
                Console.WriteLine($"[C#] contour toolpath: {segCount} segments -> {gpath}");
            }
            finally
            {
                Native.cc_free_toolpath(tp);
            }
        }
        finally
        {
            if (Native.cc_free_document(doc) != 0)
            {
                Console.Error.WriteLine($"[C#] warn: cc_free_document: {Native.LastError()}");
            }
        }

        Console.WriteLine("[C#] geometry bridge OK");
        return 0;
    }
}
