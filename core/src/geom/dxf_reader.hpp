// dxf_reader.hpp -- a small, tolerant ASCII DXF reader (hand-rolled, no deps).
//
// Deliberately authored rather than pulled from libdxfrw: it sidesteps that
// library's GPL licence (PRD decision A3) and reuses the author's DXF domain
// knowledge. It reads the documented entity subset (LINE, ARC, CIRCLE,
// LWPOLYLINE, POLYLINE) and tolerates unknown content instead of failing.
//
// Security note (NFR-7): DXF is untrusted input. The reader never trusts a
// length field, bounds every access, validates each numeric token, and skips
// malformed entities (counted in `warnings`) rather than crashing.
#ifndef CONTOURCAM_GEOM_DXF_READER_HPP
#define CONTOURCAM_GEOM_DXF_READER_HPP

#include <cstddef>
#include <istream>
#include <string>
#include <string_view>

#include "geom/dxf_document.hpp"

namespace contourcam {

struct DxfParseResult {
    bool ok = false;            // false only on hard failures (e.g. unreadable file)
    DxfDocument document;       // parsed entities (may be empty)
    std::string error;          // human-readable reason when !ok
    std::size_t warnings = 0;   // tolerated issues (skipped/invalid entities)
};

DxfParseResult parseDxf(std::istream& in);
DxfParseResult parseDxfString(std::string_view text);
DxfParseResult parseDxfFile(const std::string& path);

}  // namespace contourcam

#endif  // CONTOURCAM_GEOM_DXF_READER_HPP
