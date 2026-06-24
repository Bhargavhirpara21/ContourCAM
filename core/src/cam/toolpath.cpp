// toolpath.cpp -- OCCT-free toolpath generation (drill + outer contour + depth).
#include "cam/toolpath.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "cam/pocket.hpp"
#include "geom/polygon.hpp"

namespace contourcam {

namespace {

// Intersection of line (p + t*r) and line (q + u*s). ok=false if near-parallel.
Point2 intersectLines(Point2 p, Point2 r, Point2 q, Point2 s, bool& ok) {
    const double denom = cross(r, s);
    if (std::abs(denom) < 1e-12) {
        ok = false;
        return p;
    }
    ok = true;
    const double t = cross(q - p, s) / denom;
    return p + r * t;
}

// Drop consecutive duplicate points and a trailing closing duplicate.
std::vector<Point2> cleanLoop(const std::vector<Point2>& poly) {
    std::vector<Point2> out;
    for (const Point2& p : poly) {
        if (out.empty() || !nearlyEqual(out.back(), p)) out.push_back(p);
    }
    if (out.size() > 1 && nearlyEqual(out.front(), out.back())) out.pop_back();
    return out;
}

void appendContour(Toolpath& tp, const std::vector<Point2>& loop, const JobParams& job) {
    if (loop.size() < 3) return;

    std::vector<Point2> path(loop);
    if (job.direction == CutDirection::Conventional) {
        std::reverse(path.begin(), path.end());
    }

    const std::vector<double> levels = depthLevels(job.target_depth_mm, job.step_down_mm);

    tp.segments.push_back({SegmentKind::Rapid, path[0].x, path[0].y, job.safe_z_mm, 0.0, 0.0, 0.0});
    for (const double z : levels) {
        // Plunge to this Z at the entry point, then cut the closed loop.
        tp.segments.push_back(
            {SegmentKind::Feed, path[0].x, path[0].y, z, 0.0, 0.0, job.plunge_feed});
        for (std::size_t k = 1; k < path.size(); ++k) {
            tp.segments.push_back({SegmentKind::Feed, path[k].x, path[k].y, z, 0.0, 0.0, job.feed});
        }
        tp.segments.push_back({SegmentKind::Feed, path[0].x, path[0].y, z, 0.0, 0.0, job.feed});
    }
    tp.segments.push_back({SegmentKind::Rapid, path[0].x, path[0].y, job.safe_z_mm, 0.0, 0.0, 0.0});
}

void appendDrill(Toolpath& tp, const DxfCircle& c, const JobParams& job) {
    const double x = c.center.x;
    const double y = c.center.y;
    tp.segments.push_back({SegmentKind::Rapid, x, y, job.safe_z_mm, 0.0, 0.0, 0.0});
    tp.segments.push_back(
        {SegmentKind::Feed, x, y, -job.target_depth_mm, 0.0, 0.0, job.plunge_feed});
    tp.segments.push_back({SegmentKind::Rapid, x, y, job.safe_z_mm, 0.0, 0.0, 0.0});
}

// A void wire is a drilled hole (skip milling) if it matches a detected circle:
// centroid near the circle centre AND area near pi*r^2 (tolerance scaled to r).
bool isHoleNode(const WireNode& node, const std::vector<DxfCircle>& circles) {
    const std::vector<Point2> poly = node.wire.polygon();
    const Point2 c = representativePoint(poly);
    const double area = std::abs(signedArea(poly));
    for (const DxfCircle& circ : circles) {
        const double circleArea = kPi * circ.radius * circ.radius;
        const double centreTol = std::max(0.1 * circ.radius, 1e-3);
        if (distance(c, circ.center) < centreTol && std::abs(area - circleArea) < 0.25 * circleArea) {
            return true;
        }
    }
    return false;
}

// Emit a depth-stepped pocket-clearing path from concentric rings (outer-to-inner).
// NOTE: entry is a straight plunge per Z step (step_down deep) -- it assumes a
// centre-cutting end mill in soft stock; a helical/ramp lead-in is future work.
void appendPocket(Toolpath& tp, const std::vector<std::vector<Point2>>& rings,
                  const JobParams& job) {
    if (rings.empty()) return;
    const double depth = job.pocket_depth_mm > 0.0 ? job.pocket_depth_mm : job.target_depth_mm;
    const std::vector<double> levels = depthLevels(depth, job.step_down_mm);
    const Point2 entry = rings.front().front();

    tp.segments.push_back({SegmentKind::Rapid, entry.x, entry.y, job.safe_z_mm, 0.0, 0.0, 0.0});
    for (const double z : levels) {
        bool firstRing = true;
        for (const std::vector<Point2>& ring : rings) {
            if (ring.size() < 3) continue;
            const double entryFeed = firstRing ? job.plunge_feed : job.feed;
            tp.segments.push_back({SegmentKind::Feed, ring[0].x, ring[0].y, z, 0.0, 0.0, entryFeed});
            firstRing = false;
            for (std::size_t k = 1; k < ring.size(); ++k) {
                tp.segments.push_back(
                    {SegmentKind::Feed, ring[k].x, ring[k].y, z, 0.0, 0.0, job.feed});
            }
            tp.segments.push_back({SegmentKind::Feed, ring[0].x, ring[0].y, z, 0.0, 0.0, job.feed});
        }
    }
    tp.segments.push_back({SegmentKind::Rapid, entry.x, entry.y, job.safe_z_mm, 0.0, 0.0, 0.0});
}

}  // namespace

std::vector<double> depthLevels(double target_depth, double step_down) {
    std::vector<double> levels;
    if (target_depth <= 0.0 || step_down <= 0.0) return levels;
    double z = 0.0;
    while (true) {
        z -= step_down;
        if (z <= -target_depth + 1e-9) {
            levels.push_back(-target_depth);
            break;
        }
        levels.push_back(z);
    }
    return levels;
}

std::vector<Point2> offsetConvex(const std::vector<Point2>& poly, double dist) {
    const std::vector<Point2> ccwIn = cleanLoop(poly);
    const std::size_t n = ccwIn.size();
    if (n < 3) return {};

    // Normalise winding to CCW so the outward normal is well-defined (the review
    // flagged that the source geometry discards winding via std::abs).
    std::vector<Point2> ccw(ccwIn);
    if (signedArea(ccw) < 0.0) std::reverse(ccw.begin(), ccw.end());

    std::vector<Point2> edgeP(n);
    std::vector<Point2> edgeD(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 a = ccw[i];
        const Point2 b = ccw[(i + 1) % n];
        Point2 d = b - a;
        const double len = length(d);
        d = (len < 1e-12) ? Point2{1.0, 0.0} : d * (1.0 / len);
        const Point2 outward = {d.y, -d.x};  // right of a CCW edge == exterior
        edgeP[i] = a + outward * dist;
        edgeD[i] = d;
    }

    std::vector<Point2> out(n);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t i = (j + n - 1) % n;  // previous edge
        bool ok = false;
        const Point2 v = intersectLines(edgeP[i], edgeD[i], edgeP[j], edgeD[j], ok);
        out[j] = ok ? v : edgeP[j];
    }
    return out;
}

Toolpath generateToolpath(const PartModel& part, const ToolParams& tool, const JobParams& job) {
    Toolpath tp;

    if (tool.type == ToolType::Drill) {
        for (const DxfCircle& c : part.circles) appendDrill(tp, c, job);
        return tp;
    }

    // End mill: radius-compensated OUTER contour (outside), plus pocket clearing
    // for inner voids that are not drilled holes (needs OCCT; otherwise no rings).
    const double stepover = tool.diameter_mm * job.stepover_frac;
    for (const WireNode& node : part.nodes) {
        if (node.depth == 0) {
            // Outermost boundary -> radius-compensated contour on the outside.
            const std::vector<Point2> offset = offsetConvex(node.wire.polygon(), tool.radius());
            appendContour(tp, offset, job);
        } else if (node.depth % 2 == 1 && !isHoleNode(node, part.circles)) {
            // Odd depth == a void (pocket) that is not a drilled hole -> clear it.
            // Even depth >= 2 are solid islands inside a pocket (left standing).
            appendPocket(tp, clearPocketRings(node.wire.polygon(), tool.radius(), stepover), job);
        }
    }
    return tp;
}

}  // namespace contourcam
