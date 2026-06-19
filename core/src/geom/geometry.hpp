// geometry.hpp -- core 2D primitives shared by the geometry/CAM modules.
//
// Pure, dependency-free C++. Everything is in millimetres; the project's single
// internal unit. Tolerances are tuned for mechanical drawings (sub-micron point
// identity, ~1 micron gap healing).
#ifndef CONTOURCAM_GEOM_GEOMETRY_HPP
#define CONTOURCAM_GEOM_GEOMETRY_HPP

#include <cmath>

namespace contourcam {

// Pi as a constant so the code stays clean under MSVC /permissive- (where the
// non-standard M_PI macro is not defined).
inline constexpr double kPi = 3.14159265358979323846;

// Two points closer than this are considered identical.
inline constexpr double kPointTolerance = 1e-6;
// Largest gap between edge endpoints that wire assembly will heal (snap shut).
inline constexpr double kHealTolerance = 1e-3;

struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

inline Point2 operator+(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
inline Point2 operator-(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
inline Point2 operator*(Point2 a, double s) { return {a.x * s, a.y * s}; }

inline double dot(Point2 a, Point2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Point2 a, Point2 b) { return a.x * b.y - a.y * b.x; }
inline double lengthSquared(Point2 a) { return a.x * a.x + a.y * a.y; }
inline double length(Point2 a) { return std::sqrt(lengthSquared(a)); }
inline double distanceSquared(Point2 a, Point2 b) { return lengthSquared(b - a); }
inline double distance(Point2 a, Point2 b) { return std::sqrt(distanceSquared(a, b)); }

// Endpoint-coincidence test, tolerant to a configurable gap.
inline bool nearlyEqual(Point2 a, Point2 b, double tol = kPointTolerance) {
    return distanceSquared(a, b) <= tol * tol;
}

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_GEOMETRY_HPP
