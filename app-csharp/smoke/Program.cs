using ContourCam.Interop;

namespace ContourCam.Smoke;

// Proves the shared interop layer end-to-end: bridge call, load a DXF, query
// geometry, generate a contour toolpath, export G-code. Output is kept identical
// to the previous smoke so the CI M4 parity gate (C# vs Python G-code) holds.
internal static class Program
{
    private static int Main(string[] args)
    {
        string dxf = args.Length > 0 ? args[0] : "samples/plate_pocket_holes.dxf";
        string gpath = args.Length > 1 ? args[1] : "contourcam_csharp.gcode";

        try
        {
            Console.WriteLine($"[C#] core version: {CoreLibrary.Version()}");

            if (CoreLibrary.Add(2, 3) != 5)
            {
                Console.Error.WriteLine("[C#] FAIL: cc_add(2,3) != 5");
                return 1;
            }
            Console.WriteLine("[C#] cc_add(2, 3) = 5");

            using Document doc = Document.Load(dxf);
            int outers = doc.OuterCount;
            IReadOnlyList<Circle> circles = doc.GetCircles();
            Console.WriteLine($"[C#] {dxf}: outer={outers}, circles={circles.Count}");
            foreach (Circle c in circles)
            {
                Console.WriteLine($"[C#]   hole @ ({c.X:0.#}, {c.Y:0.#}) r={c.Radius:0.#}");
            }
            if (outers != 1 || circles.Count != 4)
            {
                Console.Error.WriteLine($"[C#] FAIL: expected outer=1, circles=4 (got {outers}, {circles.Count})");
                return 3;
            }

            var tool = new ToolParams { DiameterMm = 6.0, Flutes = 2, Type = (int)ToolType.EndMill };
            var job = new JobParams
            {
                TargetDepthMm = 6.0, StepDownMm = 2.0, StepoverFrac = 0.45,
                Feed = 600, PlungeFeed = 200, SpindleRpm = 10000, SafeZmm = 5.0,
                Direction = (int)CutDirection.Climb,
            };
            using Toolpath tp = doc.GenerateToolpath(in tool, in job);
            tp.ExportGcode(gpath, new PostParams { Metric = 1, Coolant = 0, ToolNumber = 1 });
            Console.WriteLine($"[C#] contour toolpath: {tp.SegmentCount} segments -> {gpath}");

            Console.WriteLine("[C#] geometry + toolpath bridge OK");
            return 0;
        }
        catch (ContourCamException ex)
        {
            Console.Error.WriteLine($"[C#] FAIL: {ex.Message}");
            return 2;
        }
    }
}
