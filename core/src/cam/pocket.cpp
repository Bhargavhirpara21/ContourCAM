// pocket.cpp -- concentric inward pocket clearing via OpenCASCADE 2D offsets.
//
// Without islands: march concentric offsets inward from the wall until the area
// is exhausted. With islands: each clearing level is the boolean difference of
// the inward-offset pocket wall and the outward-offset islands, so the tool
// clears AROUND standing islands with a tool-radius standoff and never gouges
// them. Both paths fail closed -- on any degenerate/failed OCCT step they stop
// emitting rather than risk an outward (gouging) or island-crossing path.
#include "cam/pocket.hpp"

#include <algorithm>
#include <cmath>

#include "geom/polygon.hpp"

#ifdef CONTOURCAM_HAVE_OCCT
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
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

// ---- Island-aware helpers (boolean region erosion) ----------------------------

// Like sampleWire, but also reports whether any edge is non-linear. The boolean
// region of polygonal inputs is line-only; a curved edge means unexpected
// geometry, so the caller fails closed instead of silently dropping mid-edge
// shape (which vertex-only sampling cannot see).
std::vector<Point2> sampleWireChecked(const TopoDS_Wire& wire, bool& hasArc) {
    std::vector<Point2> pts;
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next()) {
        BRepAdaptor_Curve curve(ex.Current());
        if (curve.GetType() != GeomAbs_Line) hasArc = true;
        const gp_Pnt p = BRep_Tool::Pnt(ex.CurrentVertex());
        const Point2 q{p.X(), p.Y()};
        if (pts.empty() || !nearlyEqual(pts.back(), q)) pts.push_back(q);
    }
    if (pts.size() > 1 && nearlyEqual(pts.front(), pts.back())) pts.pop_back();
    return pts;
}

// Closed polygon wire from CCW-ordered points (null wire on failure).
TopoDS_Wire makeClosedWire(const std::vector<Point2>& pts) {
    BRepBuilderAPI_MakePolygon mk;
    for (const Point2& p : pts) mk.Add(gp_Pnt(p.x, p.y, 0.0));
    mk.Close();
    return mk.IsDone() ? mk.Wire() : TopoDS_Wire();
}

// Offset a closed wire by `signed_dist`; return the largest-area resulting wire
// (a simple boundary offsets to one wire; pick the biggest if OCCT yields more).
TopoDS_Wire offsetLargestWire(const TopoDS_Wire& wire, double signed_dist) {
    try {
        BRepOffsetAPI_MakeOffset off(wire, GeomAbs_Intersection);
        off.Perform(signed_dist);
        if (off.IsDone() && !off.Shape().IsNull()) {
            TopoDS_Wire best;
            double bestArea = -1.0;
            for (TopExp_Explorer ex(off.Shape(), TopAbs_WIRE); ex.More(); ex.Next()) {
                const TopoDS_Wire w = TopoDS::Wire(ex.Current());
                const double a = std::abs(signedArea(sampleWire(w)));
                if (a > bestArea) {
                    bestArea = a;
                    best = w;
                }
            }
            return best;
        }
    } catch (const Standard_Failure&) {
    }
    return TopoDS_Wire();
}

// Planar face from a closed wire (false on failure).
bool faceFromWire(const TopoDS_Wire& wire, TopoDS_Face& out) {
    if (wire.IsNull()) return false;
    try {
        BRepBuilderAPI_MakeFace mk(wire, Standard_True /*OnlyPlane*/);
        if (!mk.IsDone()) return false;
        out = mk.Face();
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

// True if any point along the ring (vertices + edge subdivisions) lands strictly
// inside any island -- a gouge. Edges are sampled because a straight segment
// between two safe vertices could still clip an island corner. This is a
// fail-closed BACKSTOP against degenerate geometry, not the standoff guarantee:
// the tool-radius clearance is enforced by Cutting the OUTWARD-dilated islands
// (offset by `dist`) out of the eroded region, so a correct ring already stands
// >= dist clear of every island by construction.
bool ringGouges(const std::vector<Point2>& ring,
                const std::vector<std::vector<Point2>>& islands) {
    const std::size_t n = ring.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 a = ring[i];
        const Point2 b = ring[(i + 1) % n];
        for (int s = 0; s < 4; ++s) {  // a, plus 3 points toward b
            const double t = static_cast<double>(s) / 4.0;
            const Point2 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
            for (const std::vector<Point2>& island : islands) {
                if (pointInPolygon(island, p)) return true;
            }
        }
    }
    return false;
}

// Boundary rings of an eroded region (the Cut result). `fail` is set (and {}
// returned) on a curved edge or a gouging ring, so the caller stops clearing
// rather than emit a questionable pass.
std::vector<std::vector<Point2>> collectRegionRings(
    const TopoDS_Shape& region, double boundaryArea,
    const std::vector<std::vector<Point2>>& islands, bool& fail) {
    std::vector<std::vector<Point2>> out;
    fail = false;
    for (TopExp_Explorer ex(region, TopAbs_WIRE); ex.More(); ex.Next()) {
        bool hasArc = false;
        std::vector<Point2> ring = sampleWireChecked(TopoDS::Wire(ex.Current()), hasArc);
        if (hasArc) {
            fail = true;
            return {};
        }
        const double a = std::abs(signedArea(ring));
        if (ring.size() < 3 || a <= 1e-6 || a >= boundaryArea) continue;
        if (ringGouges(ring, islands)) {
            fail = true;
            return {};
        }
        out.push_back(std::move(ring));
    }
    return out;
}

}  // namespace
#endif  // CONTOURCAM_HAVE_OCCT

std::vector<std::vector<Point2>> clearPocketRings(const std::vector<Point2>& boundary,
                                                  double toolRadius, double stepover,
                                                  const std::vector<std::vector<Point2>>& islands) {
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

    // Normalise islands to CCW; drop degenerate ones.
    std::vector<std::vector<Point2>> islandPolys;
    for (const std::vector<Point2>& isl : islands) {
        if (isl.size() < 3) continue;
        std::vector<Point2> ci(isl);
        if (signedArea(ci) < 0.0) std::reverse(ci.begin(), ci.end());
        if (std::abs(signedArea(ci)) > 1e-9) islandPolys.push_back(std::move(ci));
    }

    if (islandPolys.empty()) {
        // ---- No islands: original concentric single-boundary clearing. --------
        // Pick the sign that offsets INWARD (rings strictly inside the boundary).
        // Fail closed: if neither sign produces an inside ring, emit no pocket
        // move rather than risk an outward (gouging) path.
        double sign = 0.0;
        for (const double trySign : {-1.0, 1.0}) {
            double area = 0.0;
            if (!offsetRings(wire, trySign * toolRadius, boundaryArea, area).empty()) {
                sign = trySign;
                break;
            }
        }
        if (sign == 0.0) return rings;

        // March inward; require the total cleared-ring area to strictly shrink so
        // the passes stay concentric and the loop always terminates.
        double prevArea = boundaryArea;
        for (double dist = toolRadius;; dist += stepover) {
            double area = 0.0;
            std::vector<std::vector<Point2>> step =
                offsetRings(wire, sign * dist, boundaryArea, area);
            if (step.empty() || area >= prevArea - 1e-9) break;
            for (std::vector<Point2>& ring : step) rings.push_back(std::move(ring));
            prevArea = area;
            if (rings.size() > 5000) break;  // safety guard
        }
        return rings;
    }

    // ---- Islands present: per-level region erosion (pocket minus islands). -----
    // The pocket boundary (CCW) erodes inward with a NEGATIVE offset; each island
    // (CCW) dilates outward with a POSITIVE offset (verified against OCCT). The
    // boolean Cut of the eroded pocket and the dilated islands yields rings that
    // stand a tool-radius clear of every island, and the Cut naturally splits the
    // region when an island pinches it (appendPocket enters each ring from above,
    // so disjoint rings are never linked through standing material).
    //
    // Termination uses the eroded wall area, which marches monotonically inward
    // until it vanishes -- robust regardless of how the Cut orients its wires.
    //
    // v1 scope: the eroded wall is taken as a single wire (offsetLargestWire), so
    // a concave pocket that erodes into several disjoint lobes would clear only
    // the largest lobe (smaller lobes under-cleared, never gouged). The sample
    // pockets are convex; multi-lobe clearing is future work.
    double prevErodedArea = boundaryArea;
    for (double dist = toolRadius;; dist += stepover) {
        const TopoDS_Wire eroded = offsetLargestWire(wire, -dist);
        if (eroded.IsNull()) break;
        const double erodedArea = std::abs(signedArea(sampleWire(eroded)));
        if (erodedArea <= 1e-6 || erodedArea >= prevErodedArea - 1e-9) break;

        TopoDS_Face outerFace;
        if (!faceFromWire(eroded, outerFace)) break;  // fail closed

        TopoDS_Shape region = outerFace;
        bool occtOk = true;
        for (const std::vector<Point2>& isl : islandPolys) {
            const TopoDS_Wire islWire = makeClosedWire(isl);
            const TopoDS_Wire dilated = offsetLargestWire(islWire, dist);  // outward
            TopoDS_Face islFace;
            if (dilated.IsNull() || !faceFromWire(dilated, islFace)) {
                occtOk = false;
                break;
            }
            try {
                BRepAlgoAPI_Cut cut(region, islFace);
                if (!cut.IsDone() || cut.Shape().IsNull()) {
                    occtOk = false;
                    break;
                }
                region = cut.Shape();
            } catch (const Standard_Failure&) {
                occtOk = false;
                break;
            }
        }
        if (!occtOk) break;  // fail closed: stop rather than risk a gouge

        bool fail = false;
        std::vector<std::vector<Point2>> step =
            collectRegionRings(region, boundaryArea, islandPolys, fail);
        if (fail) break;
        if (step.empty()) break;  // region fully consumed
        for (std::vector<Point2>& ring : step) rings.push_back(std::move(ring));
        prevErodedArea = erodedArea;
        if (rings.size() > 5000) break;  // safety guard
    }
#else
    (void)boundary;
    (void)toolRadius;
    (void)stepover;
    (void)islands;
#endif
    return rings;
}

}  // namespace contourcam
