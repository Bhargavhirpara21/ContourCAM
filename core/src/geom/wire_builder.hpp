// wire_builder.hpp -- assemble loose DXF entities into connected wires.
//
// Lines, arcs and open polylines are stitched end-to-end (healing small gaps);
// circles and closed polylines become closed wires directly. The result mixes
// closed loops (machining boundaries) and any leftover open chains.
#ifndef CONTOURCAM_GEOM_WIRE_BUILDER_HPP
#define CONTOURCAM_GEOM_WIRE_BUILDER_HPP

#include <vector>

#include "geom/dxf_document.hpp"
#include "geom/geometry.hpp"
#include "geom/wire.hpp"

namespace contourcam {

std::vector<Wire> assembleWires(const DxfDocument& doc,
                                double healTol = kHealTolerance);

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_WIRE_BUILDER_HPP
