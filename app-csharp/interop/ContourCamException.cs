namespace ContourCam.Interop;

/// <summary>Raised when a native ABI call returns a non-OK status code.</summary>
public sealed class ContourCamException : Exception
{
    public CcStatus Status { get; }

    public ContourCamException(string operation, CcStatus status, string nativeMessage)
        : base($"{operation} failed ({status}): {nativeMessage}")
    {
        Status = status;
    }
}
