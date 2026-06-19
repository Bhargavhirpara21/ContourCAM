// part_model.hpp -- closed wires organised into a containment hierarchy.
//
// Island detection: every closed wire is nested under the smallest wire that
// contains it. Nesting depth gives the machining role -- even depth is solid
// material (outer profile / island top), odd depth is a void (pocket / hole).
#ifndef CONTOURCAM_GEOM_PART_MODEL_HPP
#define CONTOURCAM_GEOM_PART_MODEL_HPP

#include <cstddef>
#include <vector>

#include "geom/dxf_document.hpp"
#include "geom/geometry.hpp"
#include "geom/wire.hpp"

namespace contourcam {

struct WireNode {
    Wire wire;
    int parent = -1;              // index of the immediately enclosing wire, or -1
    std::vector<int> children;    // immediately enclosed wires
    int depth = 0;                // nesting level (0 == outermost)
    bool isOuter = true;          // true == solid boundary (even depth)
};

struct PartModel {
    std::vector<WireNode> nodes;     // all closed wires, classified
    std::vector<DxfCircle> circles;  // circle entities (drill candidates)
    std::vector<Wire> openWires;     // chains that did not close (diagnostics)

    std::size_t outerCount() const;  // number of depth-0 boundaries
};

PartModel buildPartModel(const DxfDocument& doc, double healTol = kHealTolerance);

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_PART_MODEL_HPP
