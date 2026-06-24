// toolpath.hpp -- the toolpath/segment model and the OCCT-free toolpath
// generators: drilling cycles, radius-compensated OUTER contour, and 2.5D depth
// passes. Pure arithmetic on the Phase-1 geometry; no external dependencies.
//
// General/concave offset and pocket clearing (island avoidance) need robust 2D
// offsetting and arrive with OpenCASCADE in a later phase.
#ifndef CONTOURCAM_CAM_TOOLPATH_HPP
#define CONTOURCAM_CAM_TOOLPATH_HPP

#include <cstdint>
#include <vector>

#include "geom/dxf_document.hpp"
#include "geom/geometry.hpp"
#include "geom/part_model.hpp"

namespace contourcam {

enum class SegmentKind : int32_t { Rapid = 0, Feed = 1, ArcCW = 2, ArcCCW = 3 };
enum class ToolType : int32_t { EndMill = 0, Drill = 1 };
enum class CutDirection : int32_t { Climb = 0, Conventional = 1 };

// One ordered move. Arc centre offsets (i,j) are relative to the segment start
// and unused for Rapid/Feed. POD-friendly so it maps 1:1 to the ABI segment.
struct Segment {
    SegmentKind kind = SegmentKind::Rapid;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double i = 0.0;
    double j = 0.0;
    double feed = 0.0;  // mm/min; 0 for rapids
};

struct Toolpath {
    std::vector<Segment> segments;
};

struct ToolParams {
    double diameter_mm = 6.0;
    int32_t flutes = 2;
    ToolType type = ToolType::EndMill;

    double radius() const { return diameter_mm * 0.5; }
};

struct JobParams {
    double target_depth_mm = 6.0;   // total cut depth (contour) / drill depth
    double step_down_mm = 2.0;      // Z removed per pass
    double stepover_frac = 0.45;    // pocket clearing (OCCT phase)
    double feed = 600.0;            // cutting feed, mm/min
    double plunge_feed = 200.0;     // Z plunge feed, mm/min
    double spindle_rpm = 10000.0;
    double safe_z_mm = 5.0;         // retract/rapid plane
    CutDirection direction = CutDirection::Climb;
};

// Descending Z cut levels from 0 down to -target by step (last == -target).
std::vector<double> depthLevels(double target_depth, double step_down);

// Offset a convex closed polygon by `dist`. The result is outward when dist > 0,
// independent of the input winding (it is normalised to CCW first). The input
// must not repeat its closing vertex (a trailing duplicate is tolerated).
std::vector<Point2> offsetConvex(const std::vector<Point2>& poly, double dist);

// Generate the toolpath for ONE tool over a part:
//   EndMill -> radius-compensated OUTER contour (profiled outside), depth-stepped.
//   Drill   -> a drilling cycle at each detected circle centre.
Toolpath generateToolpath(const PartModel& part, const ToolParams& tool, const JobParams& job);

}  // namespace contourcam

#endif  // CONTOURCAM_CAM_TOOLPATH_HPP
