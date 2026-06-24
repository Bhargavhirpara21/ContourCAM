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

// Ordered vertices of a (line-segment) wire.
std::vector<Point2> sampleWire(const TopoDS_Wire& wire) {
    std::vector<Point2> pts;
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next()) {
        const gp_Pnt p = BRep_Tool::Pnt(ex.CurrentVertex());
        pts.push_back(Point2{p.X(), p.Y()});
    }
    return pts;
}

// Largest-area ring among an offset result's wires (area returned via outArea).
std::vector<Point2> bestRing(const TopoDS_Shape& shape, double& outArea) {
    std::vector<Point2> best;
    outArea = 0.0;
    for (TopExp_Explorer ex(shape, TopAbs_WIRE); ex.More(); ex.Next()) {
        std::vector<Point2> ring = sampleWire(TopoDS::Wire(ex.Current()));
        const double a = std::abs(signedArea(ring));
        if (ring.size() >= 3 && a > outArea) {
            outArea = a;
            best = std::move(ring);
        }
    }
    return best;
}

}  // namespace
#endif  // CONTOURCAM_HAVE_OCCT

std::vector<std::vector<Point2>> clearPocketRings(const std::vector<Point2>& boundary,
                                                  double toolRadius, double stepover) {
    std::vector<std::vector<Point2>> rings;
#ifdef CONTOURCAM_HAVE_OCCT
    if (boundary.size() < 3 || toolRadius <= 0.0 || stepover <= 0.0) return rings;

    // Normalise winding (CCW) for a deterministic offset direction.
    std::vector<Point2> ccw(boundary);
    if (signedArea(ccw) < 0.0) std::reverse(ccw.begin(), ccw.end());
    const double boundaryArea = std::abs(signedArea(ccw));
    if (boundaryArea < 1e-9) return rings;

    BRepBuilderAPI_MakePolygon mk;
    for (const Point2& p : ccw) mk.Add(gp_Pnt(p.x, p.y, 0.0));
    mk.Close();
    if (!mk.IsDone()) return rings;
    const TopoDS_Wire wire = mk.Wire();

    // Choose the sign that offsets INWARD (first ring area < boundary area).
    double sign = -1.0;
    try {
        BRepOffsetAPI_MakeOffset probe(wire, GeomAbs_Intersection);
        probe.Perform(sign * toolRadius);
        double a = 0.0;
        if (probe.IsDone() && !probe.Shape().IsNull()) bestRing(probe.Shape(), a);
        if (!(a > 1e-9 && a < boundaryArea)) sign = 1.0;
    } catch (const Standard_Failure&) {
        sign = 1.0;
    }

    for (double dist = toolRadius;; dist += stepover) {
        try {
            BRepOffsetAPI_MakeOffset off(wire, GeomAbs_Intersection);
            off.Perform(sign * dist);
            if (!off.IsDone() || off.Shape().IsNull()) break;
            double area = 0.0;
            std::vector<Point2> ring = bestRing(off.Shape(), area);
            if (ring.size() < 3 || area < 1e-6) break;
            rings.push_back(std::move(ring));
        } catch (const Standard_Failure&) {
            break;  // offset collapsed -> pocket cleared
        }
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
