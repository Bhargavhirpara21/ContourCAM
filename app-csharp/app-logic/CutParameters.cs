using System.ComponentModel;
using System.Runtime.CompilerServices;
using ContourCam.Interop;

namespace ContourCam.App.Logic;

/// <summary>
/// Bindable tool/job/post parameters, pre-filled with the PRD section-7 defaults.
/// Projects cleanly to the native POD structs.
/// </summary>
public sealed class CutParameters : INotifyPropertyChanged
{
    private double _toolDiameterMm = 6.0;
    private int _flutes = 2;
    private ToolType _toolType = ToolType.EndMill;
    private double _targetDepthMm = 6.0;
    private double _pocketDepthMm = 5.0;
    private double _stepDownMm = 2.0;
    private double _stepoverFrac = 0.45;
    private double _feed = 600.0;
    private double _plungeFeed = 200.0;
    private double _spindleRpm = 10000.0;
    private double _safeZmm = 5.0;
    private CutDirection _direction = CutDirection.Climb;
    private bool _metric = true;
    private int _toolNumber = 1;

    public double ToolDiameterMm { get => _toolDiameterMm; set => Set(ref _toolDiameterMm, value); }
    public int Flutes { get => _flutes; set => Set(ref _flutes, value); }
    public ToolType ToolType { get => _toolType; set => Set(ref _toolType, value); }
    public double TargetDepthMm { get => _targetDepthMm; set => Set(ref _targetDepthMm, value); }
    public double PocketDepthMm { get => _pocketDepthMm; set => Set(ref _pocketDepthMm, value); }
    public double StepDownMm { get => _stepDownMm; set => Set(ref _stepDownMm, value); }
    public double StepoverFrac { get => _stepoverFrac; set => Set(ref _stepoverFrac, value); }
    public double Feed { get => _feed; set => Set(ref _feed, value); }
    public double PlungeFeed { get => _plungeFeed; set => Set(ref _plungeFeed, value); }
    public double SpindleRpm { get => _spindleRpm; set => Set(ref _spindleRpm, value); }
    public double SafeZmm { get => _safeZmm; set => Set(ref _safeZmm, value); }
    public CutDirection Direction { get => _direction; set => Set(ref _direction, value); }
    public bool Metric { get => _metric; set => Set(ref _metric, value); }
    public int ToolNumber { get => _toolNumber; set => Set(ref _toolNumber, value); }

    public ToolParams ToTool() => new()
    {
        DiameterMm = ToolDiameterMm,
        Flutes = Flutes,
        Type = (int)ToolType,
    };

    public JobParams ToJob() => new()
    {
        TargetDepthMm = TargetDepthMm,
        PocketDepthMm = PocketDepthMm,
        StepDownMm = StepDownMm,
        StepoverFrac = StepoverFrac,
        Feed = Feed,
        PlungeFeed = PlungeFeed,
        SpindleRpm = SpindleRpm,
        SafeZmm = SafeZmm,
        Direction = (int)Direction,
    };

    public PostParams ToPost() => new()
    {
        Metric = Metric ? 1 : 0,
        Coolant = 0,
        ToolNumber = ToolNumber,
    };

    public event PropertyChangedEventHandler? PropertyChanged;

    private void Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}
