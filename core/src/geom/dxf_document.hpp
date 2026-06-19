// dxf_document.hpp -- the parsed contents of a DXF file (the supported subset).
//
// A flat bag of the entity types ContourCAM understands (FR-C1). Angles are in
// degrees as stored in DXF; everything else is in drawing units, normalised to
// millimetres downstream once $INSUNITS is known.
#ifndef CONTOURCAM_GEOM_DXF_DOCUMENT_HPP
#define CONTOURCAM_GEOM_DXF_DOCUMENT_HPP

#include <cstddef>
#include <vector>

#include "geom/geometry.hpp"

namespace contourcam {

struct DxfLine {
    Point2 start;
    Point2 end;
};

// DXF arcs are always swept counter-clockwise from start angle to end angle.
struct DxfArc {
    Point2 center;
    double radius = 0.0;
    double startAngleDeg = 0.0;
    double endAngleDeg = 0.0;
};

struct DxfCircle {
    Point2 center;
    double radius = 0.0;
};

struct DxfPolyline {
    std::vector<Point2> vertices;
    bool closed = false;
};

// Subset of $INSUNITS we act on; everything else is treated as Unknown.
enum class DxfUnits { Unknown = 0, Millimeters, Inches };

struct DxfDocument {
    std::vector<DxfLine> lines;
    std::vector<DxfArc> arcs;
    std::vector<DxfCircle> circles;
    std::vector<DxfPolyline> polylines;
    DxfUnits units = DxfUnits::Unknown;

    std::size_t entityCount() const {
        return lines.size() + arcs.size() + circles.size() + polylines.size();
    }
};

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_DXF_DOCUMENT_HPP
