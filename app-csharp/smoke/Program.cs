using System;
using System.Runtime.InteropServices;

namespace ContourCam.Interop.Smoke;

/// <summary>
/// P/Invoke declarations for the ContourCAM native core (flat C ABI).
/// The library name "contourcam_core" resolves to contourcam_core.dll on
/// Windows and libcontourcam_core.so on Linux; it must sit next to this
/// executable (or on the loader path) at run time.
/// </summary>
internal static class Native
{
    private const string Lib = "contourcam_core";

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr cc_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int cc_add(int a, int b, out int result);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr cc_last_error();

    public static string Version() => Marshal.PtrToStringAnsi(cc_version()) ?? string.Empty;

    public static string LastError() => Marshal.PtrToStringAnsi(cc_last_error()) ?? string.Empty;
}

internal static class Program
{
    private static int Main()
    {
        Console.WriteLine($"[C#] core version: {Native.Version()}");

        int status = Native.cc_add(2, 3, out int sum);
        if (status != 0)
        {
            Console.Error.WriteLine($"[C#] cc_add failed (status {status}): {Native.LastError()}");
            return 1;
        }

        Console.WriteLine($"[C#] cc_add(2, 3) = {sum}");

        if (sum != 5)
        {
            Console.Error.WriteLine($"[C#] FAIL: expected 5, got {sum}");
            return 2;
        }

        Console.WriteLine("[C#] bridge OK");
        return 0;
    }
}
