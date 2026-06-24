using System.ComponentModel;
using System.Runtime.CompilerServices;
using ContourCam.Interop;

namespace ContourCam.App.Logic;

/// <summary>
/// The app workflow: open a DXF, generate a toolpath off the UI thread, export
/// G-code. WPF-free so it is unit-testable; the View supplies file pickers via
/// the Request* delegates and observes <see cref="SceneChanged"/> to redraw.
/// </summary>
public sealed class MainViewModel : INotifyPropertyChanged, IDisposable
{
    private Document? _document;
    private Toolpath? _toolpath;
    private SceneModel _scene = new();
    private string _statusText = "Open a DXF to begin.";
    private bool _isBusy;
    private bool _toolpathReady;

    public CutParameters Parameters { get; } = new();

    /// <summary>Enum value lists for the parameter-panel combo boxes.</summary>
    public IReadOnlyList<ToolType> ToolTypes { get; } = Enum.GetValues<ToolType>();
    public IReadOnlyList<CutDirection> Directions { get; } = Enum.GetValues<CutDirection>();

    public SceneModel Scene
    {
        get => _scene;
        private set
        {
            _scene = value;
            OnPropertyChanged();
            SceneChanged?.Invoke();
        }
    }

    public string StatusText { get => _statusText; private set => SetField(ref _statusText, value); }

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetField(ref _isBusy, value)) RefreshCommands();
        }
    }

    public bool CanGenerate => !_isBusy && _document is not null;
    public bool CanExport => !_isBusy && _toolpathReady;

    public RelayCommand OpenCommand { get; }
    public RelayCommand GenerateCommand { get; }
    public RelayCommand ExportCommand { get; }

    /// <summary>Set by the View to show Open / Save dialogs (return null to cancel).</summary>
    public Func<string?>? RequestOpenPath { get; set; }
    public Func<string?>? RequestSavePath { get; set; }

    /// <summary>Raised after the scene snapshot changes (the View re-renders).</summary>
    public event Action? SceneChanged;

    public MainViewModel()
    {
        OpenCommand = new RelayCommand(() => _ = OpenAsync(), () => !_isBusy);
        GenerateCommand = new RelayCommand(() => _ = GenerateAsync(), () => CanGenerate);
        ExportCommand = new RelayCommand(() => _ = ExportAsync(), () => CanExport);
    }

    public async Task OpenAsync()
    {
        string? path = RequestOpenPath?.Invoke();
        if (!string.IsNullOrEmpty(path)) await LoadAsync(path);
    }

    public async Task LoadAsync(string path)
    {
        IsBusy = true;
        StatusText = $"Loading {Path.GetFileName(path)} ...";
        try
        {
            (Document doc, SceneModel scene) = await Task.Run(() =>
            {
                Document d = Document.Load(path);
                return (d, SceneBuilder.BuildGeometry(d));
            });
            _document?.Dispose();
            _toolpath?.Dispose();
            _toolpath = null;
            _toolpathReady = false;
            _document = doc;
            Scene = scene;
            StatusText = $"Loaded {Path.GetFileName(path)}: {scene.Wires.Count} wires, {scene.Holes.Count} holes.";
        }
        catch (Exception ex)
        {
            StatusText = $"Open failed: {ex.Message}";
        }
        finally
        {
            IsBusy = false;
        }
    }

    public async Task GenerateAsync()
    {
        if (_document is null) return;
        IsBusy = true;
        StatusText = "Generating toolpath ...";
        try
        {
            ToolParams tool = Parameters.ToTool();
            JobParams job = Parameters.ToJob();
            Document doc = _document;
            (Toolpath tp, IReadOnlyList<Segment> segs) = await Task.Run(() =>
            {
                Toolpath t = doc.GenerateToolpath(in tool, in job);
                return (t, t.GetSegments());
            });
            _toolpath?.Dispose();
            _toolpath = tp;
            SceneBuilder.AddToolpath(_scene, segs);
            _toolpathReady = true;
            Scene = _scene;  // trigger a redraw with the overlay
            StatusText = $"Toolpath: {segs.Count} segments. Ready to export.";
        }
        catch (Exception ex)
        {
            StatusText = $"Generate failed: {ex.Message}";
        }
        finally
        {
            IsBusy = false;
        }
    }

    public async Task ExportAsync()
    {
        if (_toolpath is null) return;
        string? path = RequestSavePath?.Invoke();
        if (string.IsNullOrEmpty(path)) return;

        IsBusy = true;
        StatusText = $"Exporting {Path.GetFileName(path)} ...";
        try
        {
            PostParams post = Parameters.ToPost();
            Toolpath tp = _toolpath;
            await Task.Run(() => tp.ExportGcode(path, in post));
            StatusText = $"Exported {Path.GetFileName(path)}.";
        }
        catch (Exception ex)
        {
            StatusText = $"Export failed: {ex.Message}";
        }
        finally
        {
            IsBusy = false;
        }
    }

    private void RefreshCommands()
    {
        OpenCommand.RaiseCanExecuteChanged();
        GenerateCommand.RaiseCanExecuteChanged();
        ExportCommand.RaiseCanExecuteChanged();
        OnPropertyChanged(nameof(CanGenerate));
        OnPropertyChanged(nameof(CanExport));
    }

    public void Dispose()
    {
        _toolpath?.Dispose();
        _document?.Dispose();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        return true;
    }
}
