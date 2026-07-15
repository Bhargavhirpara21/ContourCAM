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

PartModel islandSampleModel() {
    const auto r = parseDxfFile(std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_island.dxf");
    EXPECT_TRUE(r.ok) << r.error;
    return buildPartModel(r.document);
}

// True if a point sampled along segment a->b (endpoints + interior) lands inside
// `forbidden`. Used to prove no cutting move crosses an island's keep-out zone.
bool segmentEntersPolygon(Point2 a, Point2 b, const std::vector<Point2>& forbidden) {
    for (int s = 0; s <= 8; ++s) {
        const double t = static_cast<double>(s) / 8.0;
        const Point2 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        if (pointInPolygon(forbidden, p)) return true;
    }
    return false;
}

// True if any ring (vertices + densified edges) enters `forbidden`.
bool ringsEnterPolygon(const std::vector<std::vector<Point2>>& rings,
                       const std::vector<Point2>& forbidden) {
    for (const auto& ring : rings) {
        const std::size_t n = ring.size();
        for (std::size_t i = 0; i < n; ++i) {
            if (segmentEntersPolygon(ring[i], ring[(i + 1) % n], forbidden)) return true;
        }
    }
    return false;
}
}  // namespace

// A 60x40 pocket with a 10x10 island centred at (30,20); tool radius 3.
namespace {
const std::vector<Point2> kIslePocket = {{0, 0}, {60, 0}, {60, 40}, {0, 40}};
const std::vector<Point2> kIsle = {{25, 15}, {35, 15}, {35, 25}, {25, 25}};
}  // namespace

TEST(Pocket, ClearsAroundIslandNoGouge) {
    if (!occtEnabled()) GTEST_SKIP() << "core built without OpenCASCADE";

    const double r = 3.0;
    const auto rings = clearPocketRings(kIslePocket, r, 2.7, {kIsle});
    ASSERT_FALSE(rings.empty());

    // No ring (vertex or edge) may enter the island grown by ~the tool radius:
    // the standoff rings sit at exactly r, so a slightly-smaller keep-out proves
    // the clearance without boundary-case flakiness.
    const std::vector<Point2> keepOut = offsetConvex(kIsle, r - 0.05);
    EXPECT_FALSE(ringsEnterPolygon(rings, keepOut));

    // And every point stays within the pocket walls.
    for (const auto& ring : rings) {
        for (const Point2& p : ring) {
            EXPECT_GE(p.x, 0.0 - 1e-6);
            EXPECT_LE(p.x, 60.0 + 1e-6);
            EXPECT_GE(p.y, 0.0 - 1e-6);
            EXPECT_LE(p.y, 40.0 + 1e-6);
        }
    }
}

TEST(Pocket, ClearsAllSidesOfIsland) {
    if (!occtEnabled()) GTEST_SKIP() << "core built without OpenCASCADE";

    const auto rings = clearPocketRings(kIslePocket, 3.0, 2.7, {kIsle});
    ASSERT_FALSE(rings.empty());

    // The annulus around the island must be reached on all four sides (island
    // bbox is [25,35] x [15,25]).
    bool left = false, right = false, below = false, above = false;
    for (const auto& ring : rings) {
        for (const Point2& p : ring) {
            if (p.x < 25.0) left = true;
            if (p.x > 35.0) right = true;
            if (p.y < 15.0) below = true;
            if (p.y > 25.0) above = true;
        }
    }
    EXPECT_TRUE(left && right && below && above);
}

TEST(Pocket, IslandClearingIsDeterministic) {
    if (!occtEnabled()) GTEST_SKIP() << "core built without OpenCASCADE";

    const auto a = clearPocketRings(kIslePocket, 3.0, 2.7, {kIsle});
    const auto b = clearPocketRings(kIslePocket, 3.0, 2.7, {kIsle});
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a[i].size(), b[i].size());
        for (std::size_t k = 0; k < a[i].size(); ++k) {
            EXPECT_NEAR(a[i][k].x, b[i][k].x, 1e-9);
            EXPECT_NEAR(a[i][k].y, b[i][k].y, 1e-9);
        }
    }
}

TEST(Pocket, IslandNearlyFillingPocketIsSafe) {
    if (!occtEnabled()) GTEST_SKIP() << "core built without OpenCASCADE";

    // Island leaves < one tool diameter to the wall on every side: there is no
    // room to clear, so the call must not crash and must never gouge the island.
    const std::vector<Point2> big = {{2, 2}, {58, 2}, {58, 38}, {2, 38}};  // 56x36
    const auto rings = clearPocketRings(kIslePocket, 3.0, 2.7, {big});
    const std::vector<Point2> keepOut = offsetConvex(big, 3.0 - 0.05);
    EXPECT_FALSE(ringsEnterPolygon(rings, keepOut));  // empty rings pass trivially
}

TEST(Pocket, ToolpathAvoidsIslandInSample) {
    const PartModel m = islandSampleModel();

    // Classification: exactly one depth-2 solid island stands inside the pocket.
    int islandIdx = -1;
    int depth2 = 0;
    for (std::size_t i = 0; i < m.nodes.size(); ++i) {
        if (m.nodes[i].depth == 2) {
            ++depth2;
            islandIdx = static_cast<int>(i);
        }
    }
    ASSERT_EQ(depth2, 1);
    EXPECT_TRUE(m.nodes[static_cast<std::size_t>(islandIdx)].isOuter);
    const std::vector<Point2> island = m.nodes[static_cast<std::size_t>(islandIdx)].wire.polygon();

    ToolParams mill;
    mill.type = ToolType::EndMill;
    mill.diameter_mm = 6.0;
    JobParams job;
    job.target_depth_mm = 6.0;
    job.pocket_depth_mm = 5.0;
    job.step_down_mm = 2.0;

    const Toolpath tp = generateToolpath(m, mill, job);

    // No cutting move may enter the island grown by ~the tool radius (both builds:
    // without OCCT there are no pocket feeds, so this holds trivially).
    const std::vector<Point2> keepOut = offsetConvex(island, mill.radius() - 0.05);
    bool pocketCleared = false;
    bool prevWasFeed = false;
    Point2 prev{0, 0};
    for (const Segment& s : tp.segments) {
        const Point2 cur{s.x, s.y};
        if (s.kind == SegmentKind::Feed) {
            EXPECT_FALSE(pointInPolygon(keepOut, cur)) << "feed endpoint gouges island";
            if (prevWasFeed) {
                EXPECT_FALSE(segmentEntersPolygon(prev, cur, keepOut)) << "feed move crosses island";
            }
            // A feed inside the pocket window but outside the island => clearing.
            if (s.x > 22 && s.x < 78 && s.y > 12 && s.y < 68 && std::abs(s.z + 5.0) < 1e-6 &&
                !pointInPolygon(island, cur)) {
                pocketCleared = true;
            }
        }
        prevWasFeed = (s.kind == SegmentKind::Feed);
        prev = cur;
    }
    EXPECT_EQ(pocketCleared, occtEnabled());
}

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
    job.target_depth_mm = 6.0;   // outer profile through the 6 mm stock
    job.pocket_depth_mm = 5.0;   // pocket floor at 5 mm (PRD section 7)
    job.step_down_mm = 2.0;
    job.stepover_frac = 0.45;

    const Toolpath tp = generateToolpath(m, mill, job);

    bool insidePocket = false;
    bool pocketFloorAt5 = false;
    bool contourAt6 = false;
    for (const Segment& s : tp.segments) {
        if (s.kind != SegmentKind::Feed) continue;
        const bool inPocketXY = s.x > 33 && s.x < 67 && s.y > 28 && s.y < 52;
        if (inPocketXY) insidePocket = true;
        if (inPocketXY && std::abs(s.z + 5.0) < 1e-6) pocketFloorAt5 = true;
        if (std::abs(s.z + 6.0) < 1e-6) contourAt6 = true;
    }
    // With OCCT the pocket is cleared (feed moves inside it); without OCCT the end
    // mill only profiles the outside, so no feed moves land inside the pocket.
    EXPECT_EQ(insidePocket, occtEnabled());
    EXPECT_TRUE(contourAt6);  // outer contour reaches the 6 mm stock depth (both builds)
    if (occtEnabled()) EXPECT_TRUE(pocketFloorAt5);  // pocket floor honours its own depth
}
