namespace ContourCam.Interop;

/// <summary>Status codes returned by every ABI call (mirrors cc_status).</summary>
public enum CcStatus
{
    Ok = 0,
    Unknown = 1,
    InvalidArg = 2,
    Parse = 3,
    BufferTooSmall = 4,
    Io = 5,
}

/// <summary>Tool kind (mirrors cc_tool_type).</summary>
public enum ToolType
{
    EndMill = 0,
    Drill = 1,
}

/// <summary>Cut direction (mirrors cc_cut_direction).</summary>
public enum CutDirection
{
    Climb = 0,
    Conventional = 1,
}

/// <summary>Segment kind (mirrors cc_segment_kind).</summary>
public enum SegmentKind
{
    Rapid = 0,
    Feed = 1,
    ArcCw = 2,
    ArcCcw = 3,
}
