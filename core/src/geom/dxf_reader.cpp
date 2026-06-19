// dxf_reader.cpp -- implementation of the tolerant ASCII DXF reader.
#include "geom/dxf_reader.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace contourcam {
namespace {

// A DXF record is a (group code, value) pair spread over two physical lines.
struct Pair {
    int code = 0;
    std::string value;
};

std::string trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool parseInt(const std::string& s, int& out) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool parseDouble(const std::string& s, double& out) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

// First value for a group code in an entity's collected fields, or default.
double fieldDouble(const std::vector<Pair>& fields, int code, double def = 0.0) {
    for (const Pair& p : fields) {
        if (p.code == code) {
            double d = def;
            if (parseDouble(p.value, d)) return d;
            return def;
        }
    }
    return def;
}

int fieldInt(const std::vector<Pair>& fields, int code, int def = 0) {
    for (const Pair& p : fields) {
        if (p.code == code) {
            int d = def;
            if (parseInt(p.value, d)) return d;
            return def;
        }
    }
    return def;
}

// Turn one finished entity record into document geometry. `pendingPoly` carries
// the open state of an old-style POLYLINE across its VERTEX/SEQEND records.
void buildEntity(const std::string& type, const std::vector<Pair>& fields,
                 DxfDocument& doc, std::optional<DxfPolyline>& pendingPoly,
                 std::size_t& warnings) {
    if (type == "LINE") {
        DxfLine line;
        line.start = {fieldDouble(fields, 10), fieldDouble(fields, 20)};
        line.end = {fieldDouble(fields, 11), fieldDouble(fields, 21)};
        doc.lines.push_back(line);
    } else if (type == "CIRCLE") {
        DxfCircle circle;
        circle.center = {fieldDouble(fields, 10), fieldDouble(fields, 20)};
        circle.radius = fieldDouble(fields, 40);
        if (circle.radius > 0.0) {
            doc.circles.push_back(circle);
        } else {
            ++warnings;
        }
    } else if (type == "ARC") {
        DxfArc arc;
        arc.center = {fieldDouble(fields, 10), fieldDouble(fields, 20)};
        arc.radius = fieldDouble(fields, 40);
        arc.startAngleDeg = fieldDouble(fields, 50);
        arc.endAngleDeg = fieldDouble(fields, 51);
        if (arc.radius > 0.0) {
            doc.arcs.push_back(arc);
        } else {
            ++warnings;
        }
    } else if (type == "LWPOLYLINE") {
        DxfPolyline poly;
        poly.closed = (fieldInt(fields, 70) & 1) != 0;
        // Vertices arrive as ordered, interleaved 10/20 pairs.
        for (const Pair& p : fields) {
            if (p.code == 10) {
                double x = 0.0;
                if (parseDouble(p.value, x)) poly.vertices.push_back({x, 0.0});
            } else if (p.code == 20 && !poly.vertices.empty()) {
                double y = 0.0;
                if (parseDouble(p.value, y)) poly.vertices.back().y = y;
            }
        }
        if (poly.vertices.size() >= 2) {
            doc.polylines.push_back(std::move(poly));
        } else {
            ++warnings;
        }
    } else if (type == "POLYLINE") {
        DxfPolyline poly;
        poly.closed = (fieldInt(fields, 70) & 1) != 0;
        pendingPoly = std::move(poly);
    } else if (type == "VERTEX") {
        if (pendingPoly) {
            pendingPoly->vertices.push_back(
                {fieldDouble(fields, 10), fieldDouble(fields, 20)});
        } else {
            ++warnings;
        }
    } else if (type == "SEQEND") {
        if (pendingPoly) {
            if (pendingPoly->vertices.size() >= 2) {
                doc.polylines.push_back(std::move(*pendingPoly));
            } else {
                ++warnings;
            }
            pendingPoly.reset();
        }
    } else {
        // Unsupported but well-formed entity -- tolerated, not fatal.
        ++warnings;
    }
}

}  // namespace

DxfParseResult parseDxf(std::istream& in) {
    DxfParseResult result;
    DxfDocument& doc = result.document;

    std::string section;
    bool expectSectionName = false;
    std::string headerVar;

    std::string curType;
    std::vector<Pair> curFields;
    bool haveCur = false;
    std::optional<DxfPolyline> pendingPoly;

    auto flush = [&]() {
        if (!haveCur) return;
        buildEntity(curType, curFields, doc, pendingPoly, result.warnings);
        haveCur = false;
        curType.clear();
        curFields.clear();
    };

    std::string codeLine;
    std::string valueLine;
    while (std::getline(in, codeLine)) {
        if (!std::getline(in, valueLine)) {
            // Dangling group code with no value: malformed tail, stop cleanly.
            ++result.warnings;
            break;
        }
        if (!codeLine.empty() && codeLine.back() == '\r') codeLine.pop_back();
        if (!valueLine.empty() && valueLine.back() == '\r') valueLine.pop_back();

        int code = 0;
        if (!parseInt(codeLine, code)) {
            ++result.warnings;
            continue;
        }
        const std::string vtrim = trim(valueLine);

        if (code == 0) {
            flush();
            if (vtrim == "SECTION") {
                expectSectionName = true;
                section.clear();
            } else if (vtrim == "ENDSEC") {
                section.clear();
            } else if (vtrim == "EOF") {
                break;
            } else if (section == "ENTITIES") {
                curType = vtrim;
                haveCur = true;
            } else {
                haveCur = false;  // ignore entity-like tokens outside ENTITIES
            }
            continue;
        }

        if (expectSectionName && code == 2) {
            section = vtrim;
            expectSectionName = false;
            continue;
        }

        if (haveCur) {
            curFields.push_back({code, valueLine});
        } else if (section == "HEADER") {
            if (code == 9) {
                headerVar = vtrim;
            } else if (headerVar == "$INSUNITS" && code == 70) {
                int units = 0;
                parseInt(valueLine, units);
                if (units == 1) {
                    doc.units = DxfUnits::Inches;
                } else if (units == 4) {
                    doc.units = DxfUnits::Millimeters;
                } else {
                    doc.units = DxfUnits::Unknown;
                }
                headerVar.clear();
            }
        }
    }
    flush();

    // An unterminated old-style POLYLINE (no SEQEND): keep what we got.
    if (pendingPoly && pendingPoly->vertices.size() >= 2) {
        doc.polylines.push_back(std::move(*pendingPoly));
    }

    result.ok = true;
    return result;
}

DxfParseResult parseDxfString(std::string_view text) {
    std::istringstream in{std::string(text)};
    return parseDxf(in);
}

DxfParseResult parseDxfFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        DxfParseResult result;
        result.ok = false;
        result.error = "cannot open DXF file: " + path;
        return result;
    }
    return parseDxf(in);
}

}  // namespace contourcam
