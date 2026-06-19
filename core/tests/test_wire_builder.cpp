// Tests for wire assembly: stitching, orientation, gap healing, area/contains.
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "geom/dxf_document.hpp"
#include "geom/geometry.hpp"
#include "geom/wire_builder.hpp"

using namespace contourcam;

namespace {

std::size_t countClosed(const std::vector<Wire>& wires) {
    std::size_t n = 0;
    for (const Wire& w : wires) {
        if (w.closed) ++n;
    }
    return n;
}

}  // namespace

TEST(WireBuilder, StitchesOrderedSquare) {
    DxfDocument doc;
    doc.lines.push_back({{0, 0}, {10, 0}});
    doc.lines.push_back({{10, 0}, {10, 10}});
    doc.lines.push_back({{10, 10}, {0, 10}});
    doc.lines.push_back({{0, 10}, {0, 0}});

    const auto wires = assembleWires(doc);
    ASSERT_EQ(wires.size(), 1u);
    EXPECT_TRUE(wires[0].closed);
    EXPECT_EQ(wires[0].edges.size(), 4u);
}

TEST(WireBuilder, StitchesShuffledAndReversedSquare) {
    DxfDocument doc;
    doc.lines.push_back({{10, 10}, {10, 0}});  // reversed
    doc.lines.push_back({{0, 0}, {10, 0}});
    doc.lines.push_back({{0, 10}, {0, 0}});    // reversed
    doc.lines.push_back({{10, 10}, {0, 10}});

    const auto wires = assembleWires(doc);
    ASSERT_EQ(wires.size(), 1u);
    EXPECT_TRUE(wires[0].closed);
    EXPECT_EQ(wires[0].edges.size(), 4u);
}

TEST(WireBuilder, HealsSmallGap) {
    DxfDocument doc;
    doc.lines.push_back({{0, 0}, {10, 0}});
    doc.lines.push_back({{10, 0}, {10, 10}});
    doc.lines.push_back({{10, 10}, {0, 10}});
    doc.lines.push_back({{0, 10}, {0, 0.0005}});  // 0.5 micron gap (< healTol)

    const auto wires = assembleWires(doc);
    ASSERT_EQ(countClosed(wires), 1u);
}

TEST(WireBuilder, LeavesLargeGapOpen) {
    DxfDocument doc;
    doc.lines.push_back({{0, 0}, {10, 0}});
    doc.lines.push_back({{10, 0}, {10, 10}});
    doc.lines.push_back({{10, 10}, {0, 10}});
    doc.lines.push_back({{0, 10}, {0, 0.5}});  // 0.5 mm gap (> healTol)

    const auto wires = assembleWires(doc);
    EXPECT_EQ(countClosed(wires), 0u);
}

TEST(WireBuilder, CircleBecomesClosedWireWithCorrectArea) {
    DxfDocument doc;
    doc.circles.push_back({{0, 0}, 5.0});

    const auto wires = assembleWires(doc);
    ASSERT_EQ(wires.size(), 1u);
    EXPECT_TRUE(wires[0].closed);
    EXPECT_NEAR(std::abs(wires[0].signedArea()), kPi * 25.0, 1.0);
}

TEST(WireBuilder, ClosedPolylineBecomesWire) {
    DxfDocument doc;
    DxfPolyline p;
    p.closed = true;
    p.vertices = {{0, 0}, {4, 0}, {4, 3}, {0, 3}};
    doc.polylines.push_back(p);

    const auto wires = assembleWires(doc);
    ASSERT_EQ(wires.size(), 1u);
    EXPECT_TRUE(wires[0].closed);
    EXPECT_NEAR(std::abs(wires[0].signedArea()), 12.0, 1e-9);
}

TEST(WireBuilder, ContainsPointInsideSquare) {
    DxfDocument doc;
    doc.lines.push_back({{0, 0}, {10, 0}});
    doc.lines.push_back({{10, 0}, {10, 10}});
    doc.lines.push_back({{10, 10}, {0, 10}});
    doc.lines.push_back({{0, 10}, {0, 0}});

    const auto wires = assembleWires(doc);
    ASSERT_EQ(wires.size(), 1u);
    EXPECT_TRUE(wires[0].contains({5, 5}));
    EXPECT_FALSE(wires[0].contains({15, 5}));
}
