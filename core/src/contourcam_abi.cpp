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
#include <new>
#include <string>

#include "abi_detail.hpp"
#include "geom/dxf_reader.hpp"
#include "geom/part_model.hpp"

// Opaque document: owns the parsed + classified part model.
struct cc_document_s {
    contourcam::PartModel model;
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

cc_status CC_CALL cc_free_document(cc_document doc) {
    try {
        delete doc;  // deleting nullptr is well-defined
        return CC_OK;
    } catch (...) {
        setLastError("cc_free_document: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

}  // extern "C"
