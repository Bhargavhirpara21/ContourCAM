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
 *     free them. Opaque handles are released by their explicit cc_free_* call.
 *
 * STATUS: PROVISIONAL. The document/geometry surface below is stable enough to
 * build consumers against, but the toolpath/G-code surface (segments, tool/job
 * params, generate, export) is NOT here yet. The ABI is declared FROZEN only
 * once the G-code post-processor proves the segment model end-to-end (Phase 3).
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
    CC_ERR_INVALID_ARG = 2,
    CC_ERR_PARSE = 3,
    CC_ERR_BUFFER_TOO_SMALL = 4,
    CC_ERR_IO = 5
} cc_status;

/* ============================ Bridge / diagnostics ======================== */

/*
 * Returns the library version string. The pointer is owned by the library
 * (static storage); do NOT free it.
 */
CC_API const char* CC_CALL cc_version(void);

/*
 * Adds two integers and writes the result to *out. Kept as a trivial liveness
 * check for the interop bridge. Returns CC_ERR_INVALID_ARG if out is NULL.
 */
CC_API cc_status CC_CALL cc_add(int32_t a, int32_t b, int32_t* out);

/*
 * Returns a human-readable message for the most recent failure on the calling
 * thread (empty string if none). Library-owned storage; do NOT free.
 */
CC_API const char* CC_CALL cc_last_error(void);

/* ============================ Geometry document =========================== */

/*
 * Opaque handle to a parsed + classified DXF part (closed wires organised into
 * an island/containment hierarchy, plus detected circles). Created by
 * cc_load_dxf; released by cc_free_document.
 */
typedef struct cc_document_s* cc_document;

/* A detected circle (drill candidate): centre + radius, in millimetres. POD. */
typedef struct cc_circle {
    double x;
    double y;
    double radius;
} cc_circle;

/*
 * Parse + assemble + classify a DXF file into a document handle. On success
 * *out_doc owns the result (release with cc_free_document) and is non-NULL; on
 * failure *out_doc is set to NULL. Returns CC_ERR_PARSE for unreadable/invalid
 * input (details via cc_last_error), CC_ERR_INVALID_ARG for NULL arguments.
 */
CC_API cc_status CC_CALL cc_load_dxf(const char* path, cc_document* out_doc);

/* Number of closed wires (outer profile + pockets + holes). */
CC_API cc_status CC_CALL cc_document_wire_count(cc_document doc, int32_t* out_count);

/* Number of outermost (depth-0) boundaries. */
CC_API cc_status CC_CALL cc_document_outer_count(cc_document doc, int32_t* out_count);

/* Number of detected circles (drill candidates). */
CC_API cc_status CC_CALL cc_document_circle_count(cc_document doc, int32_t* out_count);

/*
 * Copy detected circles into a caller-owned buffer (two-call pattern, FR-A4):
 *   1. call cc_document_circle_count (or pass out_buf == NULL) for the count;
 *   2. allocate >= count entries and call again.
 * If out_buf is NULL, *out_written is set to the required count and CC_OK is
 * returned. If capacity < count, nothing is copied, *out_written is 0, and
 * CC_ERR_BUFFER_TOO_SMALL is returned. out_written must be non-NULL.
 */
CC_API cc_status CC_CALL cc_document_get_circles(cc_document doc, cc_circle* out_buf,
                                                 int32_t capacity, int32_t* out_written);

/* Release a document handle. Passing NULL is a no-op. */
CC_API cc_status CC_CALL cc_free_document(cc_document doc);

/* ============================ Toolpath + G-code =========================== */

typedef enum cc_tool_type { CC_TOOL_END_MILL = 0, CC_TOOL_DRILL = 1 } cc_tool_type;
typedef enum cc_cut_direction { CC_CLIMB = 0, CC_CONVENTIONAL = 1 } cc_cut_direction;
typedef enum cc_segment_kind {
    CC_RAPID = 0,
    CC_FEED = 1,
    CC_ARC_CW = 2,
    CC_ARC_CCW = 3
} cc_segment_kind;

typedef struct cc_tool_params {
    double diameter_mm;
    int32_t flutes;
    int32_t type;  /* cc_tool_type */
} cc_tool_params;

typedef struct cc_job_params {
    double target_depth_mm;
    double step_down_mm;
    double stepover_frac;
    double feed;
    double plunge_feed;
    double spindle_rpm;
    double safe_z_mm;
    int32_t direction;  /* cc_cut_direction */
} cc_job_params;

typedef struct cc_post_params {
    int32_t metric;       /* 1 -> G21 (mm), 0 -> G20 (inch) */
    int32_t coolant;      /* 1 -> emit M8/M9 */
    int32_t tool_number;  /* T word for the tool change */
} cc_post_params;

/* One ordered move. i/j are arc-centre offsets from the start (arcs only). POD. */
typedef struct cc_segment {
    int32_t kind;  /* cc_segment_kind */
    double x;
    double y;
    double z;
    double i;
    double j;
    double feed;
} cc_segment;

/* Opaque handle to a generated toolpath. Free with cc_free_toolpath. */
typedef struct cc_toolpath_s* cc_toolpath;

/* Generate a toolpath for one tool over a document (EndMill -> outer contour;
 * Drill -> hole cycles). *out_tp owns the result on success (NULL on failure). */
CC_API cc_status CC_CALL cc_generate_toolpath(cc_document doc, const cc_tool_params* tool,
                                              const cc_job_params* job, cc_toolpath* out_tp);

/* Number of segments in the toolpath. */
CC_API cc_status CC_CALL cc_toolpath_segment_count(cc_toolpath tp, int32_t* out_count);

/* Copy segments into a caller buffer (two-call pattern; see cc_document_get_circles). */
CC_API cc_status CC_CALL cc_toolpath_get_segments(cc_toolpath tp, cc_segment* out_buf,
                                                  int32_t capacity, int32_t* out_written);

/* Write the toolpath as an ISO G-code program to `path`. */
CC_API cc_status CC_CALL cc_export_gcode(cc_toolpath tp, const char* path,
                                         const cc_post_params* post);

/* Release a toolpath handle. Passing NULL is a no-op. */
CC_API cc_status CC_CALL cc_free_toolpath(cc_toolpath tp);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONTOURCAM_C_API_H */
