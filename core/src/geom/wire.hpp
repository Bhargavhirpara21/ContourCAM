// wire.hpp -- an ordered chain of edges, optionally closed into a loop.
//
// A closed wire is a candidate machining boundary (outer profile, pocket, or
// hole). The polygon() tessellation feeds the area/containment maths used to
// classify wires into the island hierarchy.
#ifndef CONTOURCAM_GEOM_WIRE_HPP
#define CONTOURCAM_GEOM_WIRE_HPP

#include <vector>

#include "geom/edge.hpp"
#include "geom/geometry.hpp"

namespace contourcam {

struct Wire {
    std::vector<Edge> edges;
    bool closed = false;

    // Polyline approximation. Lines map 1:1; arcs are sampled at <= angStepDeg.
    std::vector<Point2> polygon(double angStepDeg = 4.0) const;

    // Signed area of the tessellated loop (positive == CCW). 0 if degenerate.
    double signedArea() const;

    // Even-odd containment using a strictly-interior representative point.
    bool contains(Point2 p) const;
};

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_WIRE_HPP
