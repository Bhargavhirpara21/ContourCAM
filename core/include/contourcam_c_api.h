/*
 * contourcam_c_api.h  --  the single flat C ABI boundary for ContourCAM.
 *
 * This header is the ONE contract shared by every consumer (C# via P/Invoke,
 * Python via ctypes). Rules that must hold for the lifetime of the project:
 *   - extern "C", C-linkage only; no C++ types cross this boundary.
 *   - POD / blittable types and fixed-width integers only.
 *   - Explicit calling convention (CC_CALL) and x64 on both sides.
 *   - Every call returns a status code; no exception may cross the boundary.
 *   - Returned const char* point to library-owned storage; callers must NOT
 *     free them. (Real handle/buffer ownership arrives with the core in later
 *     phases, each with an explicit free function.)
 *
 * Phase 0 scope: trivial functions only, used to prove the C++/C#/Python
 * bridge before any geometry code exists.
 */
#ifndef CONTOURCAM_C_API_H
#define CONTOURCAM_C_API_H

#include <stdint.h>

/* ---- Export / calling-convention macros ---- */
#if defined(_WIN32)
#  if defined(CONTOURCAM_BUILD)
#    define CC_API __declspec(dllexport)
#  else
#    define CC_API __declspec(dllimport)
#  endif
#  define CC_CALL __cdecl
#else
#  if defined(CONTOURCAM_BUILD)
#    define CC_API __attribute__((visibility("default")))
#  else
#    define CC_API
#  endif
#  define CC_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Status code returned by (almost) every ABI call. 0 == success. */
typedef enum cc_status {
    CC_OK = 0,
    CC_ERR_UNKNOWN = 1,
    CC_ERR_INVALID_ARG = 2
} cc_status;

/*
 * Returns the library version string. The pointer is owned by the library
 * (static storage); do NOT free it.
 */
CC_API const char* CC_CALL cc_version(void);

/*
 * Adds two integers and writes the result to *out.
 * Returns CC_OK on success, or CC_ERR_INVALID_ARG if out is NULL.
 * Phase 0 smoke function: proves int args + pointer out-param + status code
 * marshal correctly across the ABI from C#, Python and C++.
 */
CC_API cc_status CC_CALL cc_add(int32_t a, int32_t b, int32_t* out);

/*
 * Returns a human-readable message for the most recent failure on the calling
 * thread (empty string if none). Library-owned storage; do NOT free.
 */
CC_API const char* CC_CALL cc_last_error(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONTOURCAM_C_API_H */
