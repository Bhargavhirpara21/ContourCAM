// Tests for the hand-rolled DXF reader (entity subset + tolerance behaviour).
#include <string>

#include <gtest/gtest.h>

#include "geom/dxf_reader.hpp"

using namespace contourcam;

namespace {

// Wrap an entity body in a minimal ENTITIES section.
std::string entities(const std::string& body) {
    return "0\nSECTION\n2\nENTITIES\n" + body + "0\nENDSEC\n0\nEOF\n";
}

}  // namespace

TEST(DxfReader, ParsesLine) {
    const auto r = parseDxfString(
        entities("0\nLINE\n8\n0\n10\n1.0\n20\n2.0\n11\n3.0\n21\n4.0\n"));
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.document.lines.size(), 1u);
    EXPECT_DOUBLE_EQ(r.document.lines[0].start.x, 1.0);
    EXPECT_DOUBLE_EQ(r.document.lines[0].start.y, 2.0);
    EXPECT_DOUBLE_EQ(r.document.lines[0].end.x, 3.0);
    EXPECT_DOUBLE_EQ(r.document.lines[0].end.y, 4.0);
}

TEST(DxfReader, ParsesCircle) {
    const auto r = parseDxfString(
        entities("0\nCIRCLE\n10\n5.0\n20\n6.0\n40\n3.0\n"));
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.document.circles.size(), 1u);
    EXPECT_DOUBLE_EQ(r.document.circles[0].center.x, 5.0);
    EXPECT_DOUBLE_EQ(r.document.circles[0].center.y, 6.0);
    EXPECT_DOUBLE_EQ(r.document.circles[0].radius, 3.0);
}

TEST(DxfReader, RejectsZeroRadiusCircle) {
    const auto r = parseDxfString(
        entities("0\nCIRCLE\n10\n5.0\n20\n6.0\n40\n0.0\n"));
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.document.circles.size(), 0u);
    EXPECT_GE(r.warnings, 1u);
}

TEST(DxfReader, ParsesArc) {
    const auto r = parseDxfString(entities(
        "0\nARC\n10\n0.0\n20\n0.0\n40\n10.0\n50\n0.0\n51\n90.0\n"));
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.document.arcs.size(), 1u);
    EXPECT_DOUBLE_EQ(r.document.arcs[0].radius, 10.0);
    EXPECT_DOUBLE_EQ(r.document.arcs[0].startAngleDeg, 0.0);
    EXPECT_DOUBLE_EQ(r.document.arcs[0].endAngleDeg, 90.0);
}

TEST(DxfReader, ParsesClosedLwPolyline) {
    const auto r = parseDxfString(entities(
        "0\nLWPOLYLINE\n90\n4\n70\n1\n"
        "10\n0.0\n20\n0.0\n10\n10.0\n20\n0.0\n"
        "10\n10.0\n20\n10.0\n10\n0.0\n20\n10.0\n"));
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.document.polylines.size(), 1u);
    EXPECT_TRUE(r.document.polylines[0].closed);
    ASSERT_EQ(r.document.polylines[0].vertices.size(), 4u);
    EXPECT_DOUBLE_EQ(r.document.polylines[0].vertices[2].x, 10.0);
    EXPECT_DOUBLE_EQ(r.document.polylines[0].vertices[2].y, 10.0);
}

TEST(DxfReader, ParsesOldStylePolyline) {
    const auto r = parseDxfString(entities(
        "0\nPOLYLINE\n70\n1\n"
        "0\nVERTEX\n10\n0.0\n20\n0.0\n"
        "0\nVERTEX\n10\n5.0\n20\n0.0\n"
        "0\nVERTEX\n10\n5.0\n20\n5.0\n"
        "0\nSEQEND\n"));
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.document.polylines.size(), 1u);
    EXPECT_TRUE(r.document.polylines[0].closed);
    EXPECT_EQ(r.document.polylines[0].vertices.size(), 3u);
}

TEST(DxfReader, ReadsInchUnits) {
    const std::string dxf =
        "0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n1\n0\nENDSEC\n"
        "0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n";
    const auto r = parseDxfString(dxf);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.document.units, DxfUnits::Inches);
}

TEST(DxfReader, ToleratesGarbageWithoutCrashing) {
    const auto r = parseDxfString("not a dxf file\njust text\n\n42\n");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.document.entityCount(), 0u);
}

TEST(DxfReader, MissingFileReportsError) {
    const auto r = parseDxfFile("does_not_exist_12345.dxf");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(DxfReader, ParsesSamplePart) {
    const auto r = parseDxfFile(
        std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_holes.dxf");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(r.document.lines.size(), 8u);    // outer 4 + pocket 4
    EXPECT_EQ(r.document.circles.size(), 4u);  // 4 holes
    EXPECT_EQ(r.document.units, DxfUnits::Millimeters);
}
