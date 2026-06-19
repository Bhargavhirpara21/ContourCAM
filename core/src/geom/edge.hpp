// edge.hpp -- a single directed boundary curve (line segment or circular arc).
//
// Edges are the atoms of wire assembly: lines and arcs are reduced to a common
// start/end representation so the stitcher can chain them by endpoint, while
// arcs keep their center/radius for later toolpath generation.
#ifndef CONTOURCAM_GEOM_EDGE_HPP
#define CONTOURCAM_GEOM_EDGE_HPP

#include <cmath>

#include "geom/geometry.hpp"

namespace contourcam {

struct Edge {
    enum class Kind { Line, Arc };

    Kind kind = Kind::Line;
    Point2 start;
    Point2 end;
    // Arc-only data (ignored for lines).
    Point2 center{0.0, 0.0};
    double radius = 0.0;
    bool ccw = true;  // sweep direction from start to end
};

inline Edge makeLineEdge(Point2 a, Point2 b) {
    Edge e;
    e.kind = Edge::Kind::Line;
    e.start = a;
    e.end = b;
    return e;
}

// Build an arc edge from DXF-style center/radius/angles (degrees, CCW).
inline Edge makeArcEdge(Point2 center, double radius, double startDeg, double endDeg) {
    Edge e;
    e.kind = Edge::Kind::Arc;
    e.center = center;
    e.radius = radius;
    e.ccw = true;
    const double a0 = startDeg * kPi / 180.0;
    const double a1 = endDeg * kPi / 180.0;
    e.start = {center.x + radius * std::cos(a0), center.y + radius * std::sin(a0)};
    e.end = {center.x + radius * std::cos(a1), center.y + radius * std::sin(a1)};
    return e;
}

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_EDGE_HPP
