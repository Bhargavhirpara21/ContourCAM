// Tests for OCCT-backed pocket clearing. The ring test is skipped without OCCT;
// the toolpath test runs in both builds and asserts the right behavior for each.
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cam/pocket.hpp"
#include "cam/toolpath.hpp"
#include "geom/dxf_reader.hpp"
#include "geom/part_model.hpp"
#include "geom/polygon.hpp"

using namespace contourcam;

namespace {
PartModel sampleModel() {
    const auto r = parseDxfFile(std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_holes.dxf");
    EXPECT_TRUE(r.ok) << r.error;
    return buildPartModel(r.document);
}
}  // namespace

TEST(Pocket, ClearsRectangleInward) {
    if (!occtEnabled()) GTEST_SKIP() << "core built without OpenCASCADE";

    const std::vector<Point2> rect = {{30, 25}, {70, 25}, {70, 55}, {30, 55}};  // 40 x 30
    const auto rings = clearPocketRings(rect, 3.0, 2.7);
    ASSERT_FALSE(rings.empty());
    EXPECT_LT(std::abs(signedArea(rings.front())), 40.0 * 30.0);  // inside the boundary

    for (const auto& ring : rings) {
        for (const Point2& p : ring) {
            EXPECT_GE(p.x, 30.0 - 1e-6);
            EXPECT_LE(p.x, 70.0 + 1e-6);
            EXPECT_GE(p.y, 25.0 - 1e-6);
            EXPECT_LE(p.y, 55.0 + 1e-6);
        }
    }
}

TEST(Pocket, ToolpathClearsSamplePocket) {
    const PartModel m = sampleModel();
    ToolParams mill;
    mill.type = ToolType::EndMill;
    mill.diameter_mm = 6.0;
    JobParams job;
    job.target_depth_mm = 5.0;
    job.step_down_mm = 2.0;
    job.stepover_frac = 0.45;

    const Toolpath tp = generateToolpath(m, mill, job);

    bool insidePocket = false;
    for (const Segment& s : tp.segments) {
        if (s.kind == SegmentKind::Feed && s.x > 33 && s.x < 67 && s.y > 28 && s.y < 52) {
            insidePocket = true;
            break;
        }
    }
    // With OCCT the pocket is cleared (feed moves inside it); without OCCT the end
    // mill only profiles the outside, so no feed moves land inside the pocket.
    EXPECT_EQ(insidePocket, occtEnabled());
}
