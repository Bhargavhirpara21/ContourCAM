// polygon.hpp -- free functions on polygons (closed point loops).
//
// Used by wire/island analysis: signed area (orientation + size), point-in-
// polygon containment, and a representative interior point. Header-only; these
// are small, hot, and shared.
#ifndef CONTOURCAM_GEOM_POLYGON_HPP
#define CONTOURCAM_GEOM_POLYGON_HPP

#include <cstddef>
#include <vector>

#include "geom/geometry.hpp"

namespace contourcam {

// Signed area via the shoelace formula. Positive == counter-clockwise winding.
// Returns 0 for degenerate input (< 3 points).
inline double signedArea(const std::vector<Point2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) return 0.0;
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const Point2& a = poly[i];
        const Point2& b = poly[(i + 1) % n];
        acc += a.x * b.y - b.x * a.y;
    }
    return 0.5 * acc;
}

// Even-odd ray-casting containment test. Boundary cases are not guaranteed
// (callers use a strictly-interior representative point).
inline bool pointInPolygon(const std::vector<Point2>& poly, Point2 p) {
    const std::size_t n = poly.size();
    if (n < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point2& a = poly[i];
        const Point2& b = poly[j];
        const bool straddles = (a.y > p.y) != (b.y > p.y);
        if (straddles) {
            const double xCross = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
            if (p.x < xCross) inside = !inside;
        }
    }
    return inside;
}

// Vertex average -- a robust interior point for the convex/near-convex loops
// produced by mechanical drawings (rectangles, circles, simple pockets).
inline Point2 representativePoint(const std::vector<Point2>& poly) {
    Point2 sum{0.0, 0.0};
    if (poly.empty()) return sum;
    for (const Point2& p : poly) {
        sum.x += p.x;
        sum.y += p.y;
    }
    const double inv = 1.0 / static_cast<double>(poly.size());
    return {sum.x * inv, sum.y * inv};
}

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_POLYGON_HPP
