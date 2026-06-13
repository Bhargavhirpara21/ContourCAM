/*
 * contourcam_core.cpp  --  Phase 0 implementation of the C ABI.
 *
 * Demonstrates the interop hard rules that the whole project depends on:
 *   - every extern "C" function is wrapped so no exception escapes;
 *   - errors are reported via status code + cc_last_error(), never thrown;
 *   - returned strings live in library-owned storage.
 */
#include "contourcam_c_api.h"

#include <string>

namespace {

// Per-thread last-error buffer. Returned via cc_last_error(); valid until the
// next ABI call on the same thread overwrites it.
thread_local std::string g_last_error;

constexpr const char* kVersion = "ContourCAM 0.0.1 (Phase 0 bridge)";

}  // namespace

extern "C" {

const char* CC_CALL cc_version(void) {
    return kVersion;
}

cc_status CC_CALL cc_add(int32_t a, int32_t b, int32_t* out) {
    try {
        if (out == nullptr) {
            g_last_error = "cc_add: out pointer is null";
            return CC_ERR_INVALID_ARG;
        }
        *out = a + b;
        g_last_error.clear();
        return CC_OK;
    } catch (...) {
        g_last_error = "cc_add: unknown exception";
        return CC_ERR_UNKNOWN;
    }
}

const char* CC_CALL cc_last_error(void) {
    return g_last_error.c_str();
}

}  // extern "C"
