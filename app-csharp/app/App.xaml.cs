using System.Windows;
using ContourCam.Interop;

namespace ContourCam.App;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        try
        {
            CoreLibrary.EnsureLoaded();
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Could not load the native core (contourcam_core).\n\n{ex.Message}\n\n" +
                "Build the core first (cmake --build --preset windows-msvc) or set " +
                "CONTOURCAM_LIB_DIR to its folder.",
                "ContourCAM", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }
}
