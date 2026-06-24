// pocket.cpp -- concentric inward pocket clearing via OpenCASCADE 2D offsets.
#include "cam/pocket.hpp"

#include <algorithm>
#include <cmath>

#include "geom/polygon.hpp"

#ifdef CONTOURCAM_HAVE_OCCT
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#endif

namespace contourcam {

bool occtEnabled() {
#ifdef CONTOURCAM_HAVE_OCCT
    return true;
#else
    return false;
#endif
}

#ifdef CONTOURCAM_HAVE_OCCT
namespace {

// Ordered vertices of a wire. Offset wires here are line-only (polygon input +
// GeomAbs_Intersection joins), so an edge's start vertex is a path point;
// consecutive/closing duplicates are dropped so they don't become null moves.
std::vector<Point2> sampleWire(const TopoDS_Wire& wire) {
    std::vector<Point2> pts;
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next()) {
        const gp_Pnt p = BRep_Tool::Pnt(ex.CurrentVertex());
        const Point2 q{p.X(), p.Y()};
        if (pts.empty() || !nearlyEqual(pts.back(), q)) pts.push_back(q);
    }
    if (pts.size() > 1 && nearlyEqual(pts.front(), pts.back())) pts.pop_back();
    return pts;
}

// All valid rings (>=3 pts, non-degenerate, strictly inside the boundary) from an
// offset result, plus their combined area. Concave offsets legitimately split
// into several wires, so every wire is kept -- not just the largest.
std::vector<std::vector<Point2>> collectRings(const TopoDS_Shape& shape, double boundaryArea,
                                              double& totalArea) {
    std::vector<std::vector<Point2>> out;
    totalArea = 0.0;
    for (TopExp_Explorer ex(shape, TopAbs_WIRE); ex.More(); ex.Next()) {
        std::vector<Point2> ring = sampleWire(TopoDS::Wire(ex.Current()));
        const double a = std::abs(signedArea(ring));
        if (ring.size() >= 3 && a > 1e-6 && a < boundaryArea) {  // inside the wall
            totalArea += a;
            out.push_back(std::move(ring));
        }
    }
    return out;
}

// Offset distance `signed_dist`; return the rings inside the boundary (may be empty).
std::vector<std::vector<Point2>> offsetRings(const TopoDS_Wire& wire, double signed_dist,
                                             double boundaryArea, double& totalArea) {
    totalArea = 0.0;
    try {
        BRepOffsetAPI_MakeOffset off(wire, GeomAbs_Intersection);
        off.Perform(signed_dist);
        if (off.IsDone() && !off.Shape().IsNull()) {
            return collectRings(off.Shape(), boundaryArea, totalArea);
        }
    } catch (const Standard_Failure&) {
        // collapsed / degenerate offset -> no rings
    }
    return {};
}

}  // namespace
#endif  // CONTOURCAM_HAVE_OCCT

std::vector<std::vector<Point2>> clearPocketRings(const std::vector<Point2>& boundary,
                                                  double toolRadius, double stepover) {
    std::vector<std::vector<Point2>> rings;
#ifdef CONTOURCAM_HAVE_OCCT
    if (boundary.size() < 3 || toolRadius <= 0.0 || stepover <= 0.0) return rings;

    std::vector<Point2> ccw(boundary);
    if (signedArea(ccw) < 0.0) std::reverse(ccw.begin(), ccw.end());
    const double boundaryArea = std::abs(signedArea(ccw));
    if (boundaryArea < 1e-9) return rings;

    BRepBuilderAPI_MakePolygon mk;
    for (const Point2& p : ccw) mk.Add(gp_Pnt(p.x, p.y, 0.0));
    mk.Close();
    if (!mk.IsDone()) return rings;
    const TopoDS_Wire wire = mk.Wire();

    // Pick the sign that offsets INWARD (rings strictly inside the boundary).
    // Fail closed: if neither sign produces an inside ring, emit no pocket move
    // rather than risk an outward (gouging) path.
    double sign = 0.0;
    for (const double trySign : {-1.0, 1.0}) {
        double area = 0.0;
        if (!offsetRings(wire, trySign * toolRadius, boundaryArea, area).empty()) {
            sign = trySign;
            break;
        }
    }
    if (sign == 0.0) return rings;

    // March inward; require the total cleared-ring area to strictly shrink so the
    // passes stay concentric and the loop always terminates.
    double prevArea = boundaryArea;
    for (double dist = toolRadius;; dist += stepover) {
        double area = 0.0;
        std::vector<std::vector<Point2>> step = offsetRings(wire, sign * dist, boundaryArea, area);
        if (step.empty() || area >= prevArea - 1e-9) break;
        for (std::vector<Point2>& ring : step) rings.push_back(std::move(ring));
        prevArea = area;
        if (rings.size() > 5000) break;  // safety guard
    }
#else
    (void)boundary;
    (void)toolRadius;
    (void)stepover;
#endif
    return rings;
}

}  // namespace contourcam
