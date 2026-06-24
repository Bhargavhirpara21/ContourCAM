using System.Runtime.InteropServices;

namespace ContourCam.Interop;

// POD mirrors of the C ABI structs. LayoutKind.Sequential with DEFAULT packing
// matches the C compiler's natural alignment -- do NOT set Pack (e.g. cc_segment
// is int32 + 6 doubles, which has 4 bytes of padding after the int).

[StructLayout(LayoutKind.Sequential)]
public struct Circle
{
    public double X;
    public double Y;
    public double Radius;
}

[StructLayout(LayoutKind.Sequential)]
public struct Point
{
    public double X;
    public double Y;
}

[StructLayout(LayoutKind.Sequential)]
public struct Segment
{
    public int Kind;  // SegmentKind
    public double X;
    public double Y;
    public double Z;
    public double I;
    public double J;
    public double Feed;
}

[StructLayout(LayoutKind.Sequential)]
public struct ToolParams
{
    public double DiameterMm;
    public int Flutes;
    public int Type;  // ToolType
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
    public int Direction;  // CutDirection
    public double PocketDepthMm;  // pocket floor depth; 0 => use TargetDepthMm
}

[StructLayout(LayoutKind.Sequential)]
public struct PostParams
{
    public int Metric;       // 1 = G21 (mm), 0 = G20 (inch)
    public int Coolant;      // 1 = emit M8/M9
    public int ToolNumber;
}
