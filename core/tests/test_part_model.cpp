// Tests for the island/containment model on the MVP sample part.
#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "geom/dxf_reader.hpp"
#include "geom/part_model.hpp"

using namespace contourcam;

namespace {

PartModel sampleModel() {
    const auto r = parseDxfFile(
        std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_holes.dxf");
    EXPECT_TRUE(r.ok) << r.error;
    return buildPartModel(r.document);
}

// Index of the largest-area closed wire (the outer profile).
int largestNode(const PartModel& m) {
    int best = -1;
    double bestArea = -1.0;
    for (std::size_t i = 0; i < m.nodes.size(); ++i) {
        const double a = std::abs(m.nodes[i].wire.signedArea());
        if (a > bestArea) {
            bestArea = a;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}  // namespace

TEST(PartModel, SampleHasSixClosedWires) {
    const PartModel m = sampleModel();
    // 1 outer rectangle + 1 pocket + 4 holes.
    EXPECT_EQ(m.nodes.size(), 6u);
    EXPECT_EQ(m.circles.size(), 4u);
    EXPECT_TRUE(m.openWires.empty());
}

TEST(PartModel, OuterContourIsTheSingleRoot) {
    const PartModel m = sampleModel();
    EXPECT_EQ(m.outerCount(), 1u);

    const int outer = largestNode(m);
    ASSERT_GE(outer, 0);
    EXPECT_EQ(m.nodes[outer].parent, -1);
    EXPECT_EQ(m.nodes[outer].depth, 0);
    EXPECT_TRUE(m.nodes[outer].isOuter);
    EXPECT_NEAR(std::abs(m.nodes[outer].wire.signedArea()), 100.0 * 80.0, 1.0);
    // Pocket + 4 holes all nest directly under the outer contour.
    EXPECT_EQ(m.nodes[outer].children.size(), 5u);
}

TEST(PartModel, InnerLoopsAreVoidsAtDepthOne) {
    const PartModel m = sampleModel();
    const int outer = largestNode(m);

    std::size_t voids = 0;
    bool pocketFound = false;
    for (std::size_t i = 0; i < m.nodes.size(); ++i) {
        if (static_cast<int>(i) == outer) continue;
        EXPECT_EQ(m.nodes[i].depth, 1);
        EXPECT_FALSE(m.nodes[i].isOuter);  // odd depth == void
        ++voids;
        if (std::abs(std::abs(m.nodes[i].wire.signedArea()) - 40.0 * 30.0) < 1.0) {
            pocketFound = true;
        }
    }
    EXPECT_EQ(voids, 5u);
    EXPECT_TRUE(pocketFound);
}
