using System.Reflection;
using System.Runtime.InteropServices;

namespace ContourCam.Interop;

/// <summary>
/// Loads and locates the native core. Registers a DllImport resolver so the
/// shared library is found next to the executable, via CONTOURCAM_LIB_DIR, or in
/// a build/bin[/Release] directory walking up from the app base (dev convenience).
/// </summary>
public static class CoreLibrary
{
    private static bool _registered;
    private static readonly object Gate = new();

    public static void EnsureLoaded()
    {
        if (_registered) return;
        lock (Gate)
        {
            if (_registered) return;
            NativeLibrary.SetDllImportResolver(typeof(CoreLibrary).Assembly, Resolve);
            _registered = true;
        }
    }

    public static string Version()
    {
        EnsureLoaded();
        return Native.Version();
    }

    public static int Add(int a, int b)
    {
        EnsureLoaded();
        Native.Check(Native.cc_add(a, b, out int result), "cc_add");
        return result;
    }

    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != "contourcam_core") return IntPtr.Zero;

        string file = NativeFileName();
        foreach (string dir in CandidateDirs())
        {
            string path = Path.Combine(dir, file);
            if (File.Exists(path) && NativeLibrary.TryLoad(path, out IntPtr handle))
            {
                return handle;
            }
        }
        return NativeLibrary.TryLoad(libraryName, assembly, searchPath, out IntPtr fallback)
            ? fallback
            : IntPtr.Zero;
    }

    private static string NativeFileName()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) return "contourcam_core.dll";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX)) return "libcontourcam_core.dylib";
        return "libcontourcam_core.so";
    }

    private static IEnumerable<string> CandidateDirs()
    {
        yield return AppContext.BaseDirectory;

        string? env = Environment.GetEnvironmentVariable("CONTOURCAM_LIB_DIR");
        if (!string.IsNullOrEmpty(env)) yield return env;

        // Walk up from the app base to find the repo's build output (dev runs).
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 8 && dir is not null; i++, dir = dir.Parent)
        {
            yield return Path.Combine(dir.FullName, "build", "bin", "Release");
            yield return Path.Combine(dir.FullName, "build", "bin");
        }
    }
}
