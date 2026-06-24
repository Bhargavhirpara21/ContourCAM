// gcode.cpp -- deterministic ISO 6983 / RS-274 G-code emission.
#include "cam/gcode.hpp"

#include <cmath>
#include <cstdio>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace contourcam {

namespace {

// Fixed 3-decimal, locale-independent, with -0.000 normalised to 0.000.
std::string num(double v) {
    double r = std::round(v * 1000.0) / 1000.0;
    if (r == 0.0) r = 0.0;  // collapse -0.0
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", r);
    return std::string(buf);
}

const char* gword(SegmentKind k) {
    switch (k) {
        case SegmentKind::Rapid: return "G0";
        case SegmentKind::Feed: return "G1";
        case SegmentKind::ArcCW: return "G2";
        case SegmentKind::ArcCCW: return "G3";
    }
    return "G1";
}

}  // namespace

std::string writeGcode(const Toolpath& tp, const ToolParams& tool, const JobParams& job,
                       const PostParams& post) {
    std::ostringstream os;
    os.imbue(std::locale::classic());  // determinism: no locale-dependent formatting

    os << "(ContourCAM generated program)\n";
    os << "(tool: diameter " << num(tool.diameter_mm) << " mm, "
       << (tool.type == ToolType::Drill ? "drill" : "end mill") << ")\n";
    os << (post.metric ? "G21" : "G20") << " G90 G17 G54\n";
    os << "M6 T" << post.tool_number << "\n";
    os << "M3 S" << std::lround(job.spindle_rpm) << "\n";
    if (post.coolant) os << "M8\n";

    // Modal emission: only emit a word when it changes.
    std::string lastG;
    bool have = false;
    double lx = 0.0, ly = 0.0, lz = 0.0, lf = -1.0;

    for (const Segment& s : tp.segments) {
        std::vector<std::string> toks;
        const std::string g = gword(s.kind);
        if (g != lastG) {
            toks.push_back(g);
            lastG = g;
        }
        if (!have || std::abs(s.x - lx) > 1e-9) toks.push_back("X" + num(s.x));
        if (!have || std::abs(s.y - ly) > 1e-9) toks.push_back("Y" + num(s.y));
        if (!have || std::abs(s.z - lz) > 1e-9) toks.push_back("Z" + num(s.z));
        if (s.kind != SegmentKind::Rapid && (lf < 0.0 || std::abs(s.feed - lf) > 1e-9)) {
            toks.push_back("F" + num(s.feed));
            lf = s.feed;
        }

        if (!toks.empty()) {
            for (std::size_t t = 0; t < toks.size(); ++t) {
                if (t) os << ' ';
                os << toks[t];
            }
            os << '\n';
        }
        lx = s.x;
        ly = s.y;
        lz = s.z;
        have = true;
    }

    if (post.coolant) os << "M9\n";
    os << "M5\n";
    os << "M30\n";
    return os.str();
}

}  // namespace contourcam
