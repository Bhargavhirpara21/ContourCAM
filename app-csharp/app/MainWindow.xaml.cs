using System.Windows;
using ContourCam.App.Logic;
using Microsoft.Win32;

namespace ContourCam.App;

public partial class MainWindow : Window
{
    private readonly MainViewModel _vm = new();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _vm;

        // The View owns the file dialogs; the ViewModel just asks for a path.
        _vm.RequestOpenPath = ShowOpenDialog;
        _vm.RequestSavePath = ShowSaveDialog;
        _vm.SceneChanged += () => Viewport.SetScene(_vm.Scene);

        Closed += (_, _) => _vm.Dispose();
    }

    private string? ShowOpenDialog()
    {
        var dlg = new OpenFileDialog
        {
            Title = "Open a DXF drawing",
            Filter = "DXF drawings (*.dxf)|*.dxf|All files (*.*)|*.*",
        };
        return dlg.ShowDialog(this) == true ? dlg.FileName : null;
    }

    private string? ShowSaveDialog()
    {
        var dlg = new SaveFileDialog
        {
            Title = "Export G-code",
            Filter = "G-code (*.gcode;*.nc)|*.gcode;*.nc|All files (*.*)|*.*",
            FileName = "part.gcode",
        };
        return dlg.ShowDialog(this) == true ? dlg.FileName : null;
    }
}
