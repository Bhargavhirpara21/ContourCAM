// wire_builder.cpp -- Wire tessellation/area plus the edge-stitching assembler.
#include "geom/wire_builder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "geom/polygon.hpp"

namespace contourcam {

std::vector<Point2> Wire::polygon(double angStepDeg) const {
    std::vector<Point2> pts;
    const double angStep = std::max(angStepDeg, 0.1) * kPi / 180.0;
    for (const Edge& e : edges) {
        pts.push_back(e.start);
        if (e.kind == Edge::Kind::Arc) {
            const double a0 = std::atan2(e.start.y - e.center.y, e.start.x - e.center.x);
            const double a1 = std::atan2(e.end.y - e.center.y, e.end.x - e.center.x);
            double sweep = a1 - a0;
            // DXF arcs (and our circles) sweep CCW: normalise into (0, 2pi].
            while (sweep <= 0.0) sweep += 2.0 * kPi;
            const int steps = std::max(1, static_cast<int>(std::ceil(sweep / angStep)));
            for (int i = 1; i < steps; ++i) {
                const double a = a0 + sweep * (static_cast<double>(i) / steps);
                pts.push_back({e.center.x + e.radius * std::cos(a),
                               e.center.y + e.radius * std::sin(a)});
            }
        }
    }
    return pts;
}

double Wire::signedArea() const {
    return contourcam::signedArea(polygon());
}

bool Wire::contains(Point2 p) const {
    return pointInPolygon(polygon(), p);
}

namespace {

Edge reversedEdge(Edge e) {
    std::swap(e.start, e.end);
    if (e.kind == Edge::Kind::Arc) e.ccw = !e.ccw;
    return e;
}

Wire closedWireFromVertices(const std::vector<Point2>& verts, double healTol) {
    Wire w;
    for (std::size_t i = 0; i + 1 < verts.size(); ++i) {
        w.edges.push_back(makeLineEdge(verts[i], verts[i + 1]));
    }
    if (!nearlyEqual(verts.front(), verts.back(), healTol)) {
        w.edges.push_back(makeLineEdge(verts.back(), verts.front()));
    }
    w.closed = true;
    return w;
}

}  // namespace

std::vector<Wire> assembleWires(const DxfDocument& doc, double healTol) {
    std::vector<Wire> wires;

    // Circles are already closed loops.
    for (const DxfCircle& c : doc.circles) {
        Wire w;
        w.edges.push_back(makeArcEdge(c.center, c.radius, 0.0, 360.0));
        w.closed = true;
        wires.push_back(std::move(w));
    }

    // Edges that still need stitching go into this pool.
    std::vector<Edge> pool;
    for (const DxfPolyline& p : doc.polylines) {
        if (p.vertices.size() < 2) continue;
        if (p.closed) {
            wires.push_back(closedWireFromVertices(p.vertices, healTol));
        } else {
            for (std::size_t i = 0; i + 1 < p.vertices.size(); ++i) {
                pool.push_back(makeLineEdge(p.vertices[i], p.vertices[i + 1]));
            }
        }
    }
    for (const DxfLine& l : doc.lines) pool.push_back(makeLineEdge(l.start, l.end));
    for (const DxfArc& a : doc.arcs) {
        pool.push_back(makeArcEdge(a.center, a.radius, a.startAngleDeg, a.endAngleDeg));
    }

    // Greedy chain growth: each unused edge seeds a wire, then we extend from the
    // open end by any edge whose endpoint coincides (within healTol), flipping it
    // if needed and snapping the join shut to heal small gaps.
    std::vector<char> used(pool.size(), 0);
    for (std::size_t i = 0; i < pool.size(); ++i) {
        if (used[i]) continue;
        used[i] = 1;

        Wire w;
        w.edges.push_back(pool[i]);
        const Point2 chainStart = pool[i].start;
        Point2 chainEnd = pool[i].end;

        bool extended = true;
        while (extended) {
            extended = false;
            if (w.edges.size() >= 2 && nearlyEqual(chainEnd, chainStart, healTol)) break;
            for (std::size_t j = 0; j < pool.size(); ++j) {
                if (used[j]) continue;
                Edge cand = pool[j];
                if (nearlyEqual(cand.start, chainEnd, healTol)) {
                    cand.start = chainEnd;
                } else if (nearlyEqual(cand.end, chainEnd, healTol)) {
                    cand = reversedEdge(cand);
                    cand.start = chainEnd;
                } else {
                    continue;
                }
                w.edges.push_back(cand);
                chainEnd = cand.end;
                used[j] = 1;
                extended = true;
                break;
            }
        }

        w.closed = w.edges.size() >= 2 && nearlyEqual(chainEnd, chainStart, healTol);
        if (w.closed) w.edges.back().end = chainStart;  // snap the loop shut
        wires.push_back(std::move(w));
    }

    return wires;
}

}  // namespace contourcam
