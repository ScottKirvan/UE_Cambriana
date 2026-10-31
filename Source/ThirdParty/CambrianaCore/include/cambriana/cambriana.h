#ifndef CAMBRIANA_CAMBRIANA_H
#define CAMBRIANA_CAMBRIANA_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(CAMBRIANA_BUILDING_LIBRARY)
#    define CAMBRIANA_API __declspec(dllexport)
#  else
#    define CAMBRIANA_API __declspec(dllimport)
#  endif
#else
#  define CAMBRIANA_API __attribute__((visibility("default")))
#endif

#define CAMBRIANA_API_VERSION 1u

#define CAMBRIANA_NO_BONE UINT64_MAX
#define CAMBRIANA_NO_POSITION UINT64_MAX

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t cambriana_status;
enum {
    CAMBRIANA_OK = 0,
    CAMBRIANA_ERROR_INVALID_ARGUMENT = 1,
    CAMBRIANA_ERROR_OUT_OF_MEMORY = 2
};

enum {
    CAMBRIANA_OVERRIDE_RECURSION = 1,
    CAMBRIANA_OVERRIDE_BASIC_ANGLE = 2,
    CAMBRIANA_OVERRIDE_THICKNESS = 4
};

enum {
    CAMBRIANA_RESULT_HAS_ERRORS = 1,
    CAMBRIANA_RESULT_PRODUCTION_BUDGET_REACHED = 2,
    CAMBRIANA_RESULT_BONE_BUDGET_REACHED = 4,
    CAMBRIANA_RESULT_CANCELLED = 8,
    CAMBRIANA_RESULT_WORK_BUDGET_REACHED = 16
};

enum {
    CAMBRIANA_RESULT_VERTEX_BUDGET_REACHED = 32
};

enum {
    CAMBRIANA_SIZE_FIT = 0,
    CAMBRIANA_SIZE_STEP = 1
};

enum {
    CAMBRIANA_JUNCTION_TRANSITION = 0,
    CAMBRIANA_JUNCTION_LEGACY = 1
};

enum {
    CAMBRIANA_SEVERITY_WARNING = 0,
    CAMBRIANA_SEVERITY_ERROR = 1
};

enum {
    CAMBRIANA_DIAGNOSTIC_MISSING_END_MARKER = 1,
    CAMBRIANA_DIAGNOSTIC_INVALID_NUMBER = 2,
    CAMBRIANA_DIAGNOSTIC_MISSING_SETTING = 3,
    CAMBRIANA_DIAGNOSTIC_MISSING_AXIOM = 4,
    CAMBRIANA_DIAGNOSTIC_MALFORMED_RULE = 5,
    CAMBRIANA_DIAGNOSTIC_UNMATCHED_POP = 6,
    CAMBRIANA_DIAGNOSTIC_UNMATCHED_POLYGON_END = 7,
    CAMBRIANA_DIAGNOSTIC_UNCLOSED_POLYGON = 8
};

enum {
    CAMBRIANA_BONE_DRAWN_SEGMENT = 0,
    CAMBRIANA_BONE_POLYGON_ATTACHMENT = 1,
    CAMBRIANA_BONE_SOCKET = 2,
    CAMBRIANA_BONE_OTHER_UNDRAWN = 3
};

/* Versioned by struct_size: later versions only append fields, each struct ending on an 8-byte
   boundary so that no new field can occupy old trailing padding. Fields beyond the caller's
   struct_size take values that keep the earlier behaviour. */
typedef struct cambriana_build_settings {
    uint32_t struct_size;
    uint32_t seed;
    uint64_t overrides;
    double recursion;
    double basic_angle;
    double thickness;
    double step_length;
    double min_thickness;
    uint64_t production_budget;
    uint64_t bone_budget;
    uint64_t work_budget;
    /* CAMBRIANA_SIZE_FIT chooses the step length so that the largest bounding-box dimension of the
       full-growth surface equals fit_size (cm); CAMBRIANA_SIZE_STEP uses step_length (cm). A struct_size
       that ends before size_mode builds in Step mode. min_thickness is in cm in both modes. */
    int32_t size_mode;
    int32_t junction_style;
    double fit_size;
    uint32_t closed_form;
    uint32_t caps;
    uint64_t sides;
    uint64_t vertex_budget;
} cambriana_build_settings;

/* Versioned by struct_size like the settings: later versions only append fields. */
typedef struct cambriana_summary {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t production_length;
    uint64_t generations_run;
    double growth_fraction;
    double recursion;
    double basic_angle;
    double thickness;
    double step_length;
} cambriana_summary;

/* Array element types are fixed; later data comes in new arrays. */
typedef struct cambriana_diagnostic {
    int32_t severity;
    int32_t code;
    uint64_t line;
    uint64_t column;
    uint64_t production_position;
} cambriana_diagnostic;

typedef struct cambriana_vec3 {
    double x;
    double y;
    double z;
} cambriana_vec3;

typedef struct cambriana_frame {
    cambriana_vec3 side;
    cambriana_vec3 forward;
    cambriana_vec3 up;
} cambriana_frame;

typedef struct cambriana_bone {
    cambriana_vec3 start;
    cambriana_vec3 end;
    cambriana_frame frame;
    double radius;
    uint64_t parent;
    int64_t colour;
    uint64_t generation;
    const char* name;
    int32_t kind;
    uint8_t symbol;
    uint8_t chain_break_before;
} cambriana_bone;

typedef struct cambriana_polygon_vertex {
    cambriana_vec3 position;
    cambriana_frame frame;
} cambriana_polygon_vertex;

typedef struct cambriana_polygon {
    uint64_t first_vertex;
    uint64_t vertex_count;
    uint64_t bone;
    int64_t colour;
} cambriana_polygon;

/* Front faces follow Unreal: the outward normal of {a, b, c} is cross(c - a, b - a). */
typedef struct cambriana_triangle {
    uint64_t vertices[3];
    int64_t material;
} cambriana_triangle;

typedef struct cambriana_vertex_influences {
    uint64_t first_influence;
    uint64_t influence_count;
} cambriana_vertex_influences;

typedef struct cambriana_influence {
    uint64_t bone;
    double weight;
} cambriana_influence;

typedef struct cambriana_vertex_channels {
    double along_branch;
    double root_distance;
    double branch_random;
    double thickness;
    uint64_t generation;
} cambriana_vertex_channels;

typedef struct cambriana_cancel cambriana_cancel;
typedef struct cambriana_result cambriana_result;

CAMBRIANA_API uint32_t cambriana_api_version(void);

CAMBRIANA_API cambriana_status cambriana_build_settings_init(cambriana_build_settings* settings, uint32_t struct_size);

CAMBRIANA_API cambriana_status cambriana_cancel_create(cambriana_cancel** cancel);
/* Safe to call from any thread while a build using this cancel object runs. */
CAMBRIANA_API void cambriana_cancel_request(cambriana_cancel* cancel);
CAMBRIANA_API void cambriana_cancel_destroy(cambriana_cancel* cancel);

/* Builds a legacy .ls grammar. settings and cancel may be null. On CAMBRIANA_OK *result receives
   a result owned by the caller, released with cambriana_result_destroy; grammar errors, budgets
   and cancellation are reported in its summary. Builds on separate threads are independent. */
CAMBRIANA_API cambriana_status cambriana_build_legacy(const char* text, size_t text_length,
                                                      const cambriana_build_settings* settings,
                                                      cambriana_cancel* cancel, cambriana_result** result);

CAMBRIANA_API void cambriana_result_destroy(cambriana_result* result);

/* Returned pointers and bone names stay valid until the result is destroyed. */
CAMBRIANA_API const cambriana_summary* cambriana_result_summary(const cambriana_result* result);
CAMBRIANA_API const cambriana_diagnostic* cambriana_result_diagnostics(const cambriana_result* result,
                                                                       uint64_t* count);
CAMBRIANA_API const cambriana_bone* cambriana_result_bones(const cambriana_result* result, uint64_t* count);
CAMBRIANA_API const cambriana_polygon* cambriana_result_polygons(const cambriana_result* result,
                                                                 uint64_t* count);
CAMBRIANA_API const cambriana_polygon_vertex* cambriana_result_polygon_vertices(const cambriana_result* result,
                                                                                uint64_t* count);

/* Surface vertices: positions, influences and channels are indexed alike. */
CAMBRIANA_API const cambriana_vec3* cambriana_result_surface_positions(const cambriana_result* result,
                                                                       uint64_t* count);
CAMBRIANA_API const cambriana_triangle* cambriana_result_triangles(const cambriana_result* result, uint64_t* count);
CAMBRIANA_API const cambriana_vertex_influences* cambriana_result_vertex_influences(const cambriana_result* result,
                                                                                    uint64_t* count);
CAMBRIANA_API const cambriana_influence* cambriana_result_influences(const cambriana_result* result,
                                                                     uint64_t* count);
CAMBRIANA_API const cambriana_vertex_channels* cambriana_result_vertex_channels(const cambriana_result* result,
                                                                                uint64_t* count);

#ifdef __cplusplus
}
#endif

#endif
