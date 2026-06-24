// Tests for the OCCT-free toolpath engine (depth levels, convex offset, drilling,
// outer contour) on the MVP sample part.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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

TEST(Toolpath, DepthLevelsStepDownToTarget) {
    const auto lv = depthLevels(6.0, 2.0);
    ASSERT_EQ(lv.size(), 3u);
    EXPECT_NEAR(lv[0], -2.0, 1e-9);
    EXPECT_NEAR(lv[1], -4.0, 1e-9);
    EXPECT_NEAR(lv[2], -6.0, 1e-9);

    const auto lv2 = depthLevels(5.0, 2.0);  // 5 mm by 2 mm -> -2, -4, -5
    ASSERT_EQ(lv2.size(), 3u);
    EXPECT_NEAR(lv2.back(), -5.0, 1e-9);
}

TEST(Toolpath, OffsetConvexExpandsRectangleOutward) {
    const std::vector<Point2> rect = {{0, 0}, {100, 0}, {100, 80}, {0, 80}};
    const auto off = offsetConvex(rect, 3.0);
    ASSERT_EQ(off.size(), 4u);
    // Outward by 3 -> 106 x 86 outline.
    EXPECT_NEAR(std::abs(signedArea(off)), 106.0 * 86.0, 1e-6);
    EXPECT_GT(std::abs(signedArea(off)), std::abs(signedArea(rect)));
}

TEST(Toolpath, OffsetIsWindingIndependent) {
    // Same rectangle wound CW must still offset OUTWARD (the review's gouge trap).
    const std::vector<Point2> cw = {{0, 80}, {100, 80}, {100, 0}, {0, 0}};
    const auto off = offsetConvex(cw, 3.0);
    EXPECT_NEAR(std::abs(signedArea(off)), 106.0 * 86.0, 1e-6);
}

TEST(Toolpath, DrillVisitsEveryHoleToDepth) {
    const PartModel m = sampleModel();
    ToolParams drill;
    drill.type = ToolType::Drill;
    drill.diameter_mm = 6.0;
    JobParams job;
    job.target_depth_mm = 6.0;

    const Toolpath tp = generateToolpath(m, drill, job);
    EXPECT_EQ(tp.segments.size(), 12u);  // 4 holes x (rapid, plunge, retract)

    int plunges = 0;
    double deepest = 0.0;
    for (const Segment& s : tp.segments) {
        if (s.kind == SegmentKind::Feed) {
            ++plunges;
            deepest = std::min(deepest, s.z);
        }
    }
    EXPECT_EQ(plunges, 4);
    EXPECT_NEAR(deepest, -6.0, 1e-9);
}

TEST(Toolpath, OuterContourStaysOutsideThePart) {
    const PartModel m = sampleModel();
    ToolParams mill;
    mill.type = ToolType::EndMill;
    mill.diameter_mm = 6.0;
    JobParams job;
    job.target_depth_mm = 6.0;
    job.step_down_mm = 2.0;

    const Toolpath tp = generateToolpath(m, mill, job);
    ASSERT_FALSE(tp.segments.empty());

    double minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (const Segment& s : tp.segments) {
        minx = std::min(minx, s.x);
        maxx = std::max(maxx, s.x);
        miny = std::min(miny, s.y);
        maxy = std::max(maxy, s.y);
    }
    // Radius-compensated OUTSIDE pass: 3 mm beyond each edge of the 100x80 part.
    EXPECT_NEAR(minx, -3.0, 1e-6);
    EXPECT_NEAR(maxx, 103.0, 1e-6);
    EXPECT_NEAR(miny, -3.0, 1e-6);
    EXPECT_NEAR(maxy, 83.0, 1e-6);

    // Three depth levels -> the deepest cut reaches -6 mm.
    double deepest = 0.0;
    for (const Segment& s : tp.segments) deepest = std::min(deepest, s.z);
    EXPECT_NEAR(deepest, -6.0, 1e-9);
}
