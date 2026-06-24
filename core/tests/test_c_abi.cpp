// Tests that drive the flat C ABI directly -- the actual consumer boundary that
// C# and Python bind to. Until now only the C++ geometry was tested; this
// covers the exported surface (handles, status codes, the two-call pattern).
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "contourcam_c_api.h"

namespace {

std::string samplePath() {
    return std::string(CONTOURCAM_SAMPLES_DIR) + "/plate_pocket_holes.dxf";
}

}  // namespace

TEST(CApi, VersionIsNonEmpty) {
    ASSERT_NE(cc_version(), nullptr);
    EXPECT_FALSE(std::string(cc_version()).empty());
}

TEST(CApi, AddOutParamAndNullGuard) {
    int32_t sum = 0;
    EXPECT_EQ(cc_add(2, 3, &sum), CC_OK);
    EXPECT_EQ(sum, 5);
    EXPECT_EQ(cc_add(2, 3, nullptr), CC_ERR_INVALID_ARG);
    EXPECT_FALSE(std::string(cc_last_error()).empty());
}

TEST(CApi, LoadSampleAndQueryCounts) {
    cc_document doc = nullptr;
    ASSERT_EQ(cc_load_dxf(samplePath().c_str(), &doc), CC_OK) << cc_last_error();
    ASSERT_NE(doc, nullptr);

    int32_t wires = 0;
    int32_t outers = 0;
    int32_t circles = 0;
    EXPECT_EQ(cc_document_wire_count(doc, &wires), CC_OK);
    EXPECT_EQ(wires, 6);  // outer rectangle + pocket + 4 holes
    EXPECT_EQ(cc_document_outer_count(doc, &outers), CC_OK);
    EXPECT_EQ(outers, 1);
    EXPECT_EQ(cc_document_circle_count(doc, &circles), CC_OK);
    EXPECT_EQ(circles, 4);

    EXPECT_EQ(cc_free_document(doc), CC_OK);
}

TEST(CApi, GetCirclesTwoCallPattern) {
    cc_document doc = nullptr;
    ASSERT_EQ(cc_load_dxf(samplePath().c_str(), &doc), CC_OK) << cc_last_error();

    // Query mode (NULL buffer) reports the required count.
    int32_t need = -1;
    EXPECT_EQ(cc_document_get_circles(doc, nullptr, 0, &need), CC_OK);
    EXPECT_EQ(need, 4);

    // Buffer too small -> nothing copied, explicit status (NFR-7 bounding).
    std::vector<cc_circle> tooSmall(2);
    int32_t written = -1;
    EXPECT_EQ(cc_document_get_circles(doc, tooSmall.data(), 2, &written), CC_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(written, 0);

    // Correctly sized buffer -> all four Ø6 (radius 3.0) holes.
    std::vector<cc_circle> buf(static_cast<std::size_t>(need));
    EXPECT_EQ(cc_document_get_circles(doc, buf.data(), need, &written), CC_OK);
    EXPECT_EQ(written, 4);
    for (const cc_circle& c : buf) {
        EXPECT_NEAR(c.radius, 3.0, 1e-6);
    }

    cc_free_document(doc);
}

TEST(CApi, LoadMissingFileReportsParseError) {
    cc_document doc = reinterpret_cast<cc_document>(0x1);  // poison; must be nulled
    EXPECT_EQ(cc_load_dxf("definitely_not_here.dxf", &doc), CC_ERR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_FALSE(std::string(cc_last_error()).empty());
}

TEST(CApi, GenerateExportToolpath) {
    cc_document doc = nullptr;
    ASSERT_EQ(cc_load_dxf(samplePath().c_str(), &doc), CC_OK) << cc_last_error();

    cc_tool_params tool{6.0, 2, CC_TOOL_END_MILL};
    cc_job_params job{6.0, 2.0, 0.45, 600.0, 200.0, 10000.0, 5.0, CC_CLIMB};
    cc_toolpath tp = nullptr;
    ASSERT_EQ(cc_generate_toolpath(doc, &tool, &job, &tp), CC_OK) << cc_last_error();
    ASSERT_NE(tp, nullptr);

    int32_t n = 0;
    EXPECT_EQ(cc_toolpath_segment_count(tp, &n), CC_OK);
    EXPECT_GT(n, 1);

    // Two-call fill, then the buffer-too-small path.
    std::vector<cc_segment> segs(static_cast<std::size_t>(n));
    int32_t written = 0;
    EXPECT_EQ(cc_toolpath_get_segments(tp, segs.data(), n, &written), CC_OK);
    EXPECT_EQ(written, n);
    int32_t w2 = -1;
    EXPECT_EQ(cc_toolpath_get_segments(tp, segs.data(), 1, &w2), CC_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(w2, 0);

    const char* outPath = "cc_abi_export_test.gcode";
    cc_post_params post{1, 0, 1};
    ASSERT_EQ(cc_export_gcode(tp, outPath, &post), CC_OK) << cc_last_error();

    std::ifstream f(outPath);
    const std::string content((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("G21 G90 G17 G54"), std::string::npos);
    EXPECT_NE(content.find("M30"), std::string::npos);

    EXPECT_EQ(cc_free_toolpath(tp), CC_OK);
    EXPECT_EQ(cc_free_document(doc), CC_OK);
}

TEST(CApi, WirePointsTwoCall) {
    cc_document doc = nullptr;
    ASSERT_EQ(cc_load_dxf(samplePath().c_str(), &doc), CC_OK) << cc_last_error();

    int32_t wires = 0;
    ASSERT_EQ(cc_document_wire_count(doc, &wires), CC_OK);
    ASSERT_GT(wires, 0);

    int32_t need = 0;
    EXPECT_EQ(cc_document_wire_point_count(doc, 0, &need), CC_OK);
    EXPECT_GE(need, 3);  // a closed loop has at least 3 points

    std::vector<cc_point> pts(static_cast<std::size_t>(need));
    int32_t written = 0;
    EXPECT_EQ(cc_document_get_wire_points(doc, 0, pts.data(), need, &written), CC_OK);
    EXPECT_EQ(written, need);

    int32_t w2 = -1;
    EXPECT_EQ(cc_document_get_wire_points(doc, 0, pts.data(), 0, &w2), CC_ERR_BUFFER_TOO_SMALL);
    EXPECT_EQ(w2, 0);

    int32_t dummy = 0;
    EXPECT_EQ(cc_document_wire_point_count(doc, wires, &dummy), CC_ERR_INVALID_ARG);  // out of range

    cc_free_document(doc);
}
