// Tests for the ISO G-code post-processor: required modal codes, lexability
// (parse-back), and determinism (byte-identical output for identical input).
#include <cctype>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "cam/gcode.hpp"
#include "cam/toolpath.hpp"
#include "geom/dxf_reader.hpp"
#include "geom/part_model.hpp"

using namespace contourcam;

namespace {

PartModel sampleModel() {
    const auto r = parseDxfFile(std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_holes.dxf");
    EXPECT_TRUE(r.ok) << r.error;
    return buildPartModel(r.document);
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// Every non-empty, non-comment line must be a sequence of <letter><number> words
// using only the RS-274 address letters we emit.
bool lexable(const std::string& gcode) {
    std::istringstream in(gcode);
    std::string line;
    const std::string allowed = "GMTSXYZFIJ";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '(') continue;
        std::istringstream ls(line);
        std::string tok;
        while (ls >> tok) {
            if (allowed.find(tok[0]) == std::string::npos) return false;
            for (std::size_t i = 1; i < tok.size(); ++i) {
                const char c = tok[i];
                if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-' && c != '.' &&
                    c != '+') {
                    return false;
                }
            }
        }
    }
    return true;
}

Toolpath drillPath(const PartModel& m) {
    ToolParams drill;
    drill.type = ToolType::Drill;
    JobParams job;
    job.target_depth_mm = 6.0;
    return generateToolpath(m, drill, job);
}

}  // namespace

TEST(Gcode, EmitsRequiredModalCodes) {
    const PartModel m = sampleModel();
    JobParams job;
    PostParams post;
    const std::string g = writeGcode(drillPath(m), ToolParams{}, job, post);

    EXPECT_TRUE(contains(g, "G21 G90 G17 G54"));
    EXPECT_TRUE(contains(g, "M6 T1"));
    EXPECT_TRUE(contains(g, "M3 S10000"));
    EXPECT_TRUE(contains(g, "M5"));
    EXPECT_TRUE(contains(g, "M30"));
    EXPECT_TRUE(contains(g, "Z-6.000"));  // drilled to depth
}

TEST(Gcode, OutputIsLexable) {
    const PartModel m = sampleModel();
    ToolParams mill;
    mill.type = ToolType::EndMill;
    const std::string g = writeGcode(generateToolpath(m, mill, JobParams{}), mill, JobParams{},
                                     PostParams{});
    EXPECT_TRUE(lexable(g)) << g;
    EXPECT_TRUE(contains(g, "X-3.000"));  // outside profile on the left
}

TEST(Gcode, IsDeterministic) {
    const PartModel m = sampleModel();
    const Toolpath tp = drillPath(m);
    const std::string a = writeGcode(tp, ToolParams{}, JobParams{}, PostParams{});
    const std::string b = writeGcode(tp, ToolParams{}, JobParams{}, PostParams{});
    EXPECT_EQ(a, b);
}

TEST(Gcode, InchModeEmitsG20) {
    const PartModel m = sampleModel();
    PostParams post;
    post.metric = false;
    const std::string g = writeGcode(drillPath(m), ToolParams{}, JobParams{}, post);
    EXPECT_TRUE(contains(g, "G20 G90 G17 G54"));
}
