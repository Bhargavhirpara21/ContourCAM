using Microsoft.Win32.SafeHandles;

namespace ContourCam.Interop;

// SafeHandle wrappers give deterministic disposal (via using/Dispose), a GC
// finalizer backstop if the caller forgets, and guaranteed free-exactly-once --
// the .NET idiom for the "opaque handle freed by an explicit cc_free_*" rule.

internal sealed class DocumentHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public DocumentHandle() : base(true) { }

    public DocumentHandle(IntPtr handle) : base(true) => SetHandle(handle);

    protected override bool ReleaseHandle() => Native.cc_free_document(handle) == 0;
}

internal sealed class ToolpathHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public ToolpathHandle() : base(true) { }

    public ToolpathHandle(IntPtr handle) : base(true) => SetHandle(handle);

    protected override bool ReleaseHandle() => Native.cc_free_toolpath(handle) == 0;
}
