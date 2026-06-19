// part_model.cpp -- assemble wires, then nest them into an island hierarchy.
#include "geom/part_model.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

#include "geom/polygon.hpp"
#include "geom/wire_builder.hpp"

namespace contourcam {

std::size_t PartModel::outerCount() const {
    std::size_t n = 0;
    for (const WireNode& node : nodes) {
        if (node.depth == 0) ++n;
    }
    return n;
}

PartModel buildPartModel(const DxfDocument& doc, double healTol) {
    PartModel model;
    model.circles = doc.circles;

    std::vector<Wire> closed;
    for (Wire& w : assembleWires(doc, healTol)) {
        if (w.closed) {
            closed.push_back(std::move(w));
        } else {
            model.openWires.push_back(std::move(w));
        }
    }

    const std::size_t n = closed.size();
    std::vector<std::vector<Point2>> polys(n);
    std::vector<double> areas(n);
    model.nodes.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        polys[i] = closed[i].polygon();
        areas[i] = std::abs(signedArea(polys[i]));
        model.nodes[i].wire = std::move(closed[i]);
    }

    // Parent = smallest-area wire strictly larger than i that contains it.
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 rep = representativePoint(polys[i]);
        int best = -1;
        double bestArea = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j || areas[j] <= areas[i]) continue;
            if (pointInPolygon(polys[j], rep)) {
                if (best == -1 || areas[j] < bestArea) {
                    best = static_cast<int>(j);
                    bestArea = areas[j];
                }
            }
        }
        model.nodes[i].parent = best;
    }

    // Depth, children and the solid/void classification.
    for (std::size_t i = 0; i < n; ++i) {
        int depth = 0;
        int p = model.nodes[i].parent;
        while (p != -1) {
            ++depth;
            p = model.nodes[static_cast<std::size_t>(p)].parent;
        }
        model.nodes[i].depth = depth;
        model.nodes[i].isOuter = (depth % 2) == 0;
        if (model.nodes[i].parent != -1) {
            model.nodes[static_cast<std::size_t>(model.nodes[i].parent)]
                .children.push_back(static_cast<int>(i));
        }
    }

    return model;
}

}  // namespace contourcam
