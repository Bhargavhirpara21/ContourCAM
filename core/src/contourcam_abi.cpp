/*
 * contourcam_abi.cpp  --  the geometry half of the flat C ABI.
 *
 * Wraps the authored Phase-1 geometry engine (parse -> assemble -> classify)
 * behind opaque handles, so C# (P/Invoke) and Python (ctypes) can drive the
 * SAME compiled core. Every function is try/catch-guarded so that no C++
 * exception can cross the boundary (PRD section 13, the #1 correctness risk),
 * and untrusted-input-derived sizes are range-checked before narrowing to the
 * int32 ABI types (NFR-7).
 */
#include "contourcam_c_api.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <new>
#include <string>

#include "abi_detail.hpp"
#include "cam/gcode.hpp"
#include "cam/toolpath.hpp"
#include "geom/dxf_reader.hpp"
#include "geom/part_model.hpp"

// Opaque document: owns the parsed + classified part model.
struct cc_document_s {
    contourcam::PartModel model;
};

// Opaque toolpath: owns the generated path plus the tool/job it was made with,
// so cc_export_gcode needs only the post params.
struct cc_toolpath_s {
    contourcam::Toolpath path;
    contourcam::ToolParams tool;
    contourcam::JobParams job;
};

namespace {
using contourcam::abi::clearLastError;
using contourcam::abi::setLastError;

// Guard against narrowing an untrusted-input-derived size to int32 (NFR-7).
bool fitsInt32(std::size_t n) noexcept {
    return n <= static_cast<std::size_t>(INT32_MAX);
}

// Shared body for the three count getters: validate args, range-check, narrow.
cc_status countOut(const void* doc, int32_t* out_count, std::size_t value, const char* who) noexcept {
    if (doc == nullptr || out_count == nullptr) {
        setLastError(who);
        return CC_ERR_INVALID_ARG;
    }
    if (!fitsInt32(value)) {
        setLastError("count exceeds int32 range");
        return CC_ERR_UNKNOWN;
    }
    *out_count = static_cast<int32_t>(value);
    clearLastError();
    return CC_OK;
}
}  // namespace

extern "C" {

cc_status CC_CALL cc_load_dxf(const char* path, cc_document* out_doc) {
    try {
        if (path == nullptr || out_doc == nullptr) {
            setLastError("cc_load_dxf: null argument");
            return CC_ERR_INVALID_ARG;
        }
        *out_doc = nullptr;

        const contourcam::DxfParseResult result = contourcam::parseDxfFile(std::string(path));
        if (!result.ok) {
            setLastError("cc_load_dxf: " + result.error);  // in try; a throw here -> catch(...)
            return CC_ERR_PARSE;
        }

        auto* doc = new (std::nothrow) cc_document_s{contourcam::buildPartModel(result.document)};
        if (doc == nullptr) {
            setLastError("cc_load_dxf: out of memory");  // const char* overload, noexcept
            return CC_ERR_UNKNOWN;
        }
        *out_doc = doc;
        clearLastError();
        return CC_OK;
    } catch (const std::exception& e) {
        setLastError(e.what());  // const char*, no allocation at the call site
        return CC_ERR_UNKNOWN;
    } catch (...) {
        setLastError("cc_load_dxf: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_wire_count(cc_document doc, int32_t* out_count) {
    try {
        return countOut(doc, out_count, doc != nullptr ? doc->model.nodes.size() : 0,
                        "cc_document_wire_count: null argument");
    } catch (...) {
        setLastError("cc_document_wire_count: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_outer_count(cc_document doc, int32_t* out_count) {
    try {
        return countOut(doc, out_count, doc != nullptr ? doc->model.outerCount() : 0,
                        "cc_document_outer_count: null argument");
    } catch (...) {
        setLastError("cc_document_outer_count: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_circle_count(cc_document doc, int32_t* out_count) {
    try {
        return countOut(doc, out_count, doc != nullptr ? doc->model.circles.size() : 0,
                        "cc_document_circle_count: null argument");
    } catch (...) {
        setLastError("cc_document_circle_count: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_get_circles(cc_document doc, cc_circle* out_buf, int32_t capacity,
                                          int32_t* out_written) {
    try {
        if (doc == nullptr || out_written == nullptr) {
            setLastError("cc_document_get_circles: null argument");
            return CC_ERR_INVALID_ARG;
        }
        const std::size_t total = doc->model.circles.size();
        if (!fitsInt32(total)) {
            *out_written = 0;
            setLastError("cc_document_get_circles: count exceeds int32 range");
            return CC_ERR_UNKNOWN;
        }
        const int32_t count = static_cast<int32_t>(total);

        if (out_buf == nullptr) {  // query mode: report the required count
            *out_written = count;
            clearLastError();
            return CC_OK;
        }
        if (capacity < count) {
            *out_written = 0;
            setLastError("cc_document_get_circles: buffer too small (need " +
                         std::to_string(count) + ")");
            return CC_ERR_BUFFER_TOO_SMALL;
        }

        for (int32_t i = 0; i < count; ++i) {
            const contourcam::DxfCircle& c = doc->model.circles[static_cast<std::size_t>(i)];
            out_buf[i].x = c.center.x;
            out_buf[i].y = c.center.y;
            out_buf[i].radius = c.radius;
        }
        *out_written = count;
        clearLastError();
        return CC_OK;
    } catch (...) {
        setLastError("cc_document_get_circles: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_wire_point_count(cc_document doc, int32_t wire_index,
                                              int32_t* out_count) {
    try {
        if (doc == nullptr || out_count == nullptr) {
            setLastError("cc_document_wire_point_count: null argument");
            return CC_ERR_INVALID_ARG;
        }
        if (wire_index < 0 ||
            static_cast<std::size_t>(wire_index) >= doc->model.nodes.size()) {
            setLastError("cc_document_wire_point_count: wire index out of range");
            return CC_ERR_INVALID_ARG;
        }
        const std::size_t pts =
            doc->model.nodes[static_cast<std::size_t>(wire_index)].wire.polygon().size();
        return countOut(doc, out_count, pts, "cc_document_wire_point_count: null argument");
    } catch (...) {
        setLastError("cc_document_wire_point_count: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_document_get_wire_points(cc_document doc, int32_t wire_index,
                                              cc_point* out_buf, int32_t capacity,
                                              int32_t* out_written) {
    try {
        if (doc == nullptr || out_written == nullptr) {
            setLastError("cc_document_get_wire_points: null argument");
            return CC_ERR_INVALID_ARG;
        }
        if (wire_index < 0 ||
            static_cast<std::size_t>(wire_index) >= doc->model.nodes.size()) {
            setLastError("cc_document_get_wire_points: wire index out of range");
            return CC_ERR_INVALID_ARG;
        }
        const std::vector<contourcam::Point2> pts =
            doc->model.nodes[static_cast<std::size_t>(wire_index)].wire.polygon();
        if (!fitsInt32(pts.size())) {
            *out_written = 0;
            setLastError("cc_document_get_wire_points: count exceeds int32 range");
            return CC_ERR_UNKNOWN;
        }
        const int32_t count = static_cast<int32_t>(pts.size());

        if (out_buf == nullptr) {
            *out_written = count;
            clearLastError();
            return CC_OK;
        }
        if (capacity < count) {
            *out_written = 0;
            setLastError("cc_document_get_wire_points: buffer too small (need " +
                         std::to_string(count) + ")");
            return CC_ERR_BUFFER_TOO_SMALL;
        }
        for (int32_t k = 0; k < count; ++k) {
            out_buf[k].x = pts[static_cast<std::size_t>(k)].x;
            out_buf[k].y = pts[static_cast<std::size_t>(k)].y;
        }
        *out_written = count;
        clearLastError();
        return CC_OK;
    } catch (...) {
        setLastError("cc_document_get_wire_points: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_free_document(cc_document doc) {
    try {
        delete doc;  // deleting nullptr is well-defined
        return CC_OK;
    } catch (...) {
        setLastError("cc_free_document: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_generate_toolpath(cc_document doc, const cc_tool_params* tool,
                                       const cc_job_params* job, cc_toolpath* out_tp) {
    try {
        if (doc == nullptr || tool == nullptr || job == nullptr || out_tp == nullptr) {
            setLastError("cc_generate_toolpath: null argument");
            return CC_ERR_INVALID_ARG;
        }
        *out_tp = nullptr;
        if (tool->type != CC_TOOL_END_MILL && tool->type != CC_TOOL_DRILL) {
            setLastError("cc_generate_toolpath: invalid tool type");
            return CC_ERR_INVALID_ARG;
        }

        contourcam::ToolParams t;
        t.diameter_mm = tool->diameter_mm;
        t.flutes = tool->flutes;
        t.type = static_cast<contourcam::ToolType>(tool->type);

        contourcam::JobParams j;
        j.target_depth_mm = job->target_depth_mm;
        j.pocket_depth_mm = job->pocket_depth_mm;
        j.step_down_mm = job->step_down_mm;
        j.stepover_frac = job->stepover_frac;
        j.feed = job->feed;
        j.plunge_feed = job->plunge_feed;
        j.spindle_rpm = job->spindle_rpm;
        j.safe_z_mm = job->safe_z_mm;
        j.direction = (job->direction == CC_CONVENTIONAL) ? contourcam::CutDirection::Conventional
                                                          : contourcam::CutDirection::Climb;

        auto* h = new (std::nothrow)
            cc_toolpath_s{contourcam::generateToolpath(doc->model, t, j), t, j};
        if (h == nullptr) {
            setLastError("cc_generate_toolpath: out of memory");
            return CC_ERR_UNKNOWN;
        }
        *out_tp = h;
        clearLastError();
        return CC_OK;
    } catch (const std::exception& e) {
        setLastError(e.what());
        return CC_ERR_UNKNOWN;
    } catch (...) {
        setLastError("cc_generate_toolpath: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_toolpath_segment_count(cc_toolpath tp, int32_t* out_count) {
    try {
        return countOut(tp, out_count, tp != nullptr ? tp->path.segments.size() : 0,
                        "cc_toolpath_segment_count: null argument");
    } catch (...) {
        setLastError("cc_toolpath_segment_count: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_toolpath_get_segments(cc_toolpath tp, cc_segment* out_buf, int32_t capacity,
                                           int32_t* out_written) {
    try {
        if (tp == nullptr || out_written == nullptr) {
            setLastError("cc_toolpath_get_segments: null argument");
            return CC_ERR_INVALID_ARG;
        }
        const std::size_t total = tp->path.segments.size();
        if (!fitsInt32(total)) {
            *out_written = 0;
            setLastError("cc_toolpath_get_segments: count exceeds int32 range");
            return CC_ERR_UNKNOWN;
        }
        const int32_t count = static_cast<int32_t>(total);

        if (out_buf == nullptr) {
            *out_written = count;
            clearLastError();
            return CC_OK;
        }
        if (capacity < count) {
            *out_written = 0;
            setLastError("cc_toolpath_get_segments: buffer too small (need " +
                         std::to_string(count) + ")");
            return CC_ERR_BUFFER_TOO_SMALL;
        }

        for (int32_t k = 0; k < count; ++k) {
            const contourcam::Segment& s = tp->path.segments[static_cast<std::size_t>(k)];
            out_buf[k].kind = static_cast<int32_t>(s.kind);
            out_buf[k].x = s.x;
            out_buf[k].y = s.y;
            out_buf[k].z = s.z;
            out_buf[k].i = s.i;
            out_buf[k].j = s.j;
            out_buf[k].feed = s.feed;
        }
        *out_written = count;
        clearLastError();
        return CC_OK;
    } catch (...) {
        setLastError("cc_toolpath_get_segments: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_export_gcode(cc_toolpath tp, const char* path, const cc_post_params* post) {
    try {
        if (tp == nullptr || path == nullptr || post == nullptr) {
            setLastError("cc_export_gcode: null argument");
            return CC_ERR_INVALID_ARG;
        }
        contourcam::PostParams p;
        p.metric = post->metric != 0;
        p.coolant = post->coolant != 0;
        p.tool_number = post->tool_number;

        const std::string program = contourcam::writeGcode(tp->path, tp->tool, tp->job, p);

        std::ofstream out(path, std::ios::binary);
        if (!out) {
            setLastError("cc_export_gcode: cannot open output file");
            return CC_ERR_IO;
        }
        out << program;
        if (!out) {
            setLastError("cc_export_gcode: write failed");
            return CC_ERR_IO;
        }
        clearLastError();
        return CC_OK;
    } catch (const std::exception& e) {
        setLastError(e.what());
        return CC_ERR_UNKNOWN;
    } catch (...) {
        setLastError("cc_export_gcode: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

cc_status CC_CALL cc_free_toolpath(cc_toolpath tp) {
    try {
        delete tp;  // deleting nullptr is well-defined
        return CC_OK;
    } catch (...) {
        setLastError("cc_free_toolpath: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

}  // extern "C"
