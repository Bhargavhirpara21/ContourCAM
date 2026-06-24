/*
 * contourcam_core.cpp  --  bridge/diagnostic ABI functions + the shared
 * per-thread last-error store used by every C ABI translation unit.
 *
 * Demonstrates the interop hard rules the project depends on: every extern "C"
 * function is wrapped so no exception escapes, and errors are reported via a
 * status code + cc_last_error(), never thrown across the boundary. The error
 * setters are themselves noexcept so even the failure path cannot throw.
 */
#include "contourcam_c_api.h"

#include <string>

#include "abi_detail.hpp"

namespace contourcam::abi {
namespace {
// Returned via cc_last_error(); valid until the next ABI call on this thread.
thread_local std::string g_last_error;
}  // namespace

void setLastError(const char* message) noexcept {
    try {
        g_last_error.assign(message != nullptr ? message : "");
    } catch (...) {
        g_last_error.clear();  // std::string::clear() is noexcept
    }
}

void setLastError(const std::string& message) noexcept {
    try {
        g_last_error = message;
    } catch (...) {
        g_last_error.clear();
    }
}

void clearLastError() noexcept { g_last_error.clear(); }

const char* lastError() noexcept { return g_last_error.c_str(); }
}  // namespace contourcam::abi

namespace {
constexpr const char* kVersion = "ContourCAM 0.1.0 (geometry via C ABI)";
}  // namespace

extern "C" {

const char* CC_CALL cc_version(void) {
    return kVersion;
}

cc_status CC_CALL cc_add(int32_t a, int32_t b, int32_t* out) {
    try {
        if (out == nullptr) {
            contourcam::abi::setLastError("cc_add: out pointer is null");
            return CC_ERR_INVALID_ARG;
        }
        *out = a + b;
        contourcam::abi::clearLastError();
        return CC_OK;
    } catch (...) {
        contourcam::abi::setLastError("cc_add: unknown exception");
        return CC_ERR_UNKNOWN;
    }
}

const char* CC_CALL cc_last_error(void) {
    return contourcam::abi::lastError();
}

}  // extern "C"
