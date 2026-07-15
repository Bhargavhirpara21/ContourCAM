// pocket.hpp -- pocket clearing via concentric inward offsets (OpenCASCADE).
//
// Robust 2D offsetting is hard for general/concave shapes, so this leans on
// OCCT's BRepOffsetAPI. The implementation is compiled in only when the core is
// built with CONTOURCAM_USE_OCCT; otherwise clearPocketRings returns empty and
// occtEnabled() reports false (the toolpath engine then skips pockets).
#ifndef CONTOURCAM_CAM_POCKET_HPP
#define CONTOURCAM_CAM_POCKET_HPP

#include <vector>

#include "geom/geometry.hpp"

namespace contourcam {

// True if the core was built with the OpenCASCADE backend.
bool occtEnabled();

// Concentric clearing rings for a closed pocket boundary, ordered outer-to-inner.
// The first ring sits toolRadius inside the wall (no gouge); each subsequent ring
// steps inward by `stepover`, until the area is exhausted. Empty without OCCT.
//
// `islands` are solid regions standing inside the pocket (e.g. a boss). When
// given, each clearing level is the boolean difference of the inward-offset
// pocket wall and the outward-offset islands, so the rings clear AROUND every
// island with a tool-radius standoff and never gouge it. The implementation
// fails closed: on any offset/boolean failure it stops emitting rather than risk
// an island-crossing cut. Without islands the original concentric path is used.
std::vector<std::vector<Point2>> clearPocketRings(const std::vector<Point2>& boundary,
                                                  double toolRadius, double stepover,
                                                  const std::vector<std::vector<Point2>>& islands = {});

}  // namespace contourcam

#endif  // CONTOURCAM_CAM_POCKET_HPP
