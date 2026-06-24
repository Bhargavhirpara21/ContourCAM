// abi_detail.hpp -- internal helpers shared across the C ABI translation units.
//
// NOT part of the public ABI and never installed. The per-thread last-error
// buffer behind cc_last_error() is defined once (in contourcam_core.cpp) and
// used by every extern "C" function so they can report failures uniformly.
//
// Every setter is noexcept: error reporting must never itself throw across the
// C ABI, even under memory pressure (PRD section 13). Catch/OOM paths should
// prefer the const char* overload so no std::string is constructed at the call
// site (which could itself throw).
#ifndef CONTOURCAM_ABI_DETAIL_HPP
#define CONTOURCAM_ABI_DETAIL_HPP

#include <string>

namespace contourcam::abi {

void setLastError(const char* message) noexcept;
void setLastError(const std::string& message) noexcept;
void clearLastError() noexcept;
const char* lastError() noexcept;

}  // namespace contourcam::abi

#endif  // CONTOURCAM_ABI_DETAIL_HPP
