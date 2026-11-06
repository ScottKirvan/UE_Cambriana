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

/* For cambriana_build_grammar: the grammar's own seed and size or step apply unless these are set. A grammar
   stating neither takes them from the settings regardless. cambriana_build_legacy ignores them. */
enum {
    CAMBRIANA_OVERRIDE_SEED = 8,
    CAMBRIANA_OVERRIDE_SIZE = 16
};

enum {
    CAMBRIANA_DIAGNOSTIC_SYNTAX = 9,
    CAMBRIANA_DIAGNOSTIC_COMMAND_USED_AS_NAME = 10,
    CAMBRIANA_DIAGNOSTIC_RESERVED_NAME = 11,
    CAMBRIANA_DIAGNOSTIC_UNKNOWN_NAME = 12,
    CAMBRIANA_DIAGNOSTIC_DUPLICATE_NAME = 13,
    CAMBRIANA_DIAGNOSTIC_ARGUMENT_COUNT = 14,
    CAMBRIANA_DIAGNOSTIC_POSE_PARAMETER = 15,
    CAMBRIANA_DIAGNOSTIC_MACRO_CYCLE = 16,
    CAMBRIANA_DIAGNOSTIC_MODULE_IN_DRAW_RULE = 17,
    CAMBRIANA_DIAGNOSTIC_UNKNOWN_AXIOM = 18,
    CAMBRIANA_DIAGNOSTIC_SEGMENT_REPLACED_BY_SEVERAL = 19,
    CAMBRIANA_DIAGNOSTIC_UNREACHABLE_RULE = 20,
    CAMBRIANA_DIAGNOSTIC_NOT_CONSTANT = 21,
    CAMBRIANA_DIAGNOSTIC_SIZE_AND_STEP = 22,
    CAMBRIANA_DIAGNOSTIC_NON_FINITE_VALUE = 23,
    CAMBRIANA_DIAGNOSTIC_NOT_CONVERTIBLE = 24,
    CAMBRIANA_DIAGNOSTIC_SHAPE_PARAMETER = 25
};

enum {
    CAMBRIANA_STYLE_GLYPHS = 0,
    CAMBRIANA_STYLE_NAMES = 1
};

enum {
    CAMBRIANA_STATEMENT_LSYSTEM = 0,
    CAMBRIANA_STATEMENT_CREDIT = 1,
    CAMBRIANA_STATEMENT_SETTING = 2,
    CAMBRIANA_STATEMENT_PARAMETER = 3,
    CAMBRIANA_STATEMENT_AXIOM = 4,
    CAMBRIANA_STATEMENT_MACRO = 5,
    CAMBRIANA_STATEMENT_GROW = 6,
    CAMBRIANA_STATEMENT_DRAW = 7,
    CAMBRIANA_STATEMENT_QUERY = 8
};

enum {
    CAMBRIANA_SETTING_GENERATIONS = 0,
    CAMBRIANA_SETTING_ANGLE = 1,
    CAMBRIANA_SETTING_THICKNESS = 2,
    CAMBRIANA_SETTING_SIZE = 3,
    CAMBRIANA_SETTING_STEP = 4,
    CAMBRIANA_SETTING_SEED = 5,
    CAMBRIANA_SETTING_BONE_BUDGET = 6
};

enum {
    CAMBRIANA_TIER_NONE = 0,
    CAMBRIANA_TIER_POSE = 1,
    CAMBRIANA_TIER_SHAPE = 2,
    CAMBRIANA_TIER_STRUCTURE = 3
};

typedef struct cambriana_socket {
    uint64_t bone;
    const char* name;
} cambriana_socket;

typedef struct cambriana_grammar cambriana_grammar;

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

/* Builds Cambriana grammar text. axiom_name selects a named axiom; null means the unnamed axiom, else the first.
   settings, cancel and result behave as for cambriana_build_legacy. */
CAMBRIANA_API cambriana_status cambriana_build_grammar(const char* text, size_t text_length, const char* axiom_name,
                                                       const cambriana_build_settings* settings,
                                                       cambriana_cancel* cancel, cambriana_result** result);

/* Results of cambriana_build_grammar and cambriana_build_grammar_model; empty from cambriana_build_legacy. */
CAMBRIANA_API const cambriana_socket* cambriana_result_sockets(const cambriana_result* result, uint64_t* count);
CAMBRIANA_API const char* const* cambriana_result_attribute_names(const cambriana_result* result, uint64_t* count);
/* One row per surface vertex, one column per attribute name, in that order. */
CAMBRIANA_API const double* cambriana_result_vertex_attributes(const cambriana_result* result, uint64_t* count);

/* The grammar as an editable model. Parse errors are reported in its diagnostics, not the status: *grammar is
   created whatever the text. A model may be built from several threads at once while nothing edits or serialises
   it. */
CAMBRIANA_API cambriana_status cambriana_grammar_parse(const char* text, size_t text_length,
                                                       cambriana_grammar** grammar);
/* Converts legacy .ls text to the model, which builds the same bone tree, surface and generations as
   cambriana_build_legacy unless a budget is reached (the model's budgets count modules, not characters). *grammar is
   created whatever the text. Diagnostics carry lines of the legacy text. A file the format cannot express gives
   CAMBRIANA_DIAGNOSTIC_NOT_CONVERTIBLE errors at their line and column, and an empty model. */
CAMBRIANA_API cambriana_status cambriana_grammar_convert_legacy(const char* text, size_t text_length,
                                                                cambriana_grammar** grammar);
CAMBRIANA_API cambriana_status cambriana_grammar_clone(const cambriana_grammar* grammar, cambriana_grammar** clone);
CAMBRIANA_API void cambriana_grammar_destroy(cambriana_grammar* grammar);
/* Diagnostics for the current model; valid until the next edit. An edit rejected for its own text leaves the model
   unchanged and reports that text's diagnostics, with positions in that text. An edit that runs out of memory leaves
   the model and its diagnostics unchanged. Errors in the text the model came from stay, and it builds nothing, because
   what they dropped is not in the model: parse its serialised text to go on without it. */
CAMBRIANA_API const cambriana_diagnostic* cambriana_grammar_diagnostics(const cambriana_grammar* grammar,
                                                                        uint64_t* count);
/* 1 when equal, ignoring source positions. Parse, serialise, parse gives an equal model. */
CAMBRIANA_API uint32_t cambriana_grammar_equal(const cambriana_grammar* a, const cambriana_grammar* b);
/* Text written by these functions stays valid until the next call on the same grammar, except diagnostics. */
CAMBRIANA_API const char* cambriana_grammar_serialise(cambriana_grammar* grammar, int32_t style, size_t* length);
CAMBRIANA_API cambriana_status cambriana_build_grammar_model(const cambriana_grammar* grammar, const char* axiom_name,
                                                             const cambriana_build_settings* settings,
                                                             cambriana_cancel* cancel, cambriana_result** result);

/* Top-level statements, in file order. Editing text holds exactly one statement, comments included; a comment after
   the statement would belong to the next one, so such text is rejected. */
CAMBRIANA_API uint64_t cambriana_grammar_statement_count(const cambriana_grammar* grammar);
CAMBRIANA_API int32_t cambriana_grammar_statement_kind(const cambriana_grammar* grammar, uint64_t statement);
CAMBRIANA_API const char* cambriana_grammar_statement_text(cambriana_grammar* grammar, uint64_t statement,
                                                           int32_t style, size_t* length);
CAMBRIANA_API cambriana_status cambriana_grammar_insert_statement(cambriana_grammar* grammar, uint64_t before,
                                                                  const char* text, size_t text_length);
CAMBRIANA_API cambriana_status cambriana_grammar_replace_statement(cambriana_grammar* grammar, uint64_t statement,
                                                                   const char* text, size_t text_length);
CAMBRIANA_API cambriana_status cambriana_grammar_remove_statement(cambriana_grammar* grammar, uint64_t statement);

/* Rules of a grow or draw statement. Editing text holds exactly one rule, comments included; a comment after the rule
   would belong to the next one, so such text is rejected. */
CAMBRIANA_API uint64_t cambriana_grammar_rule_count(const cambriana_grammar* grammar, uint64_t statement);
CAMBRIANA_API const char* cambriana_grammar_rule_text(cambriana_grammar* grammar, uint64_t statement, uint64_t rule,
                                                      int32_t style, size_t* length);
CAMBRIANA_API cambriana_status cambriana_grammar_insert_rule(cambriana_grammar* grammar, uint64_t statement,
                                                             uint64_t before, const char* text, size_t text_length);
CAMBRIANA_API cambriana_status cambriana_grammar_replace_rule(cambriana_grammar* grammar, uint64_t statement,
                                                              uint64_t rule, const char* text, size_t text_length);
CAMBRIANA_API cambriana_status cambriana_grammar_remove_rule(cambriana_grammar* grammar, uint64_t statement,
                                                             uint64_t rule);

/* Settings by CAMBRIANA_SETTING_*. Getting one the grammar does not state returns CAMBRIANA_ERROR_INVALID_ARGUMENT.
   Setting one the grammar does not state adds it. */
CAMBRIANA_API cambriana_status cambriana_grammar_get_setting(const cambriana_grammar* grammar, int32_t setting,
                                                             double* value);
CAMBRIANA_API cambriana_status cambriana_grammar_set_setting(cambriana_grammar* grammar, int32_t setting, double value);
CAMBRIANA_API cambriana_status cambriana_grammar_remove_setting(cambriana_grammar* grammar, int32_t setting);

/* Parameters in file order. value is the parameter's value with the grammar's stated settings. name stays valid until
   the next edit. */
CAMBRIANA_API uint64_t cambriana_grammar_parameter_count(const cambriana_grammar* grammar);
CAMBRIANA_API cambriana_status cambriana_grammar_parameter(const cambriana_grammar* grammar, uint64_t parameter,
                                                           const char** name, int32_t* tier, double* value);
CAMBRIANA_API cambriana_status cambriana_grammar_set_parameter(cambriana_grammar* grammar, const char* name,
                                                               double value);

/* Environment query (AR-02): rewriting one generation at a time, with the grammar's query modules and the bone tree
   handed to the caller after the axiom and after each generation, the last one included. */
typedef struct cambriana_environment_step cambriana_environment_step;

/* A query module of the current production, standing where the turtle is after drawing it. bone is the bone the turtle
   is on, or CAMBRIANA_NO_BONE. */
typedef struct cambriana_query_module {
    const char* name;
    const double* arguments;
    uint64_t argument_count;
    uint64_t bone;
    cambriana_vec3 position;
    cambriana_frame frame;
} cambriana_query_module;

/* Return 0 to go on; anything else stops the build, reported as cancelled. Runs on the calling thread of the build. step
   and everything read from it are valid until the function returns. */
typedef int32_t (*cambriana_environment_query)(void* context, cambriana_environment_step* step);

/* Versioned by struct_size like the settings: later versions only append fields. */
typedef struct cambriana_environment {
    uint32_t struct_size;
    uint32_t reserved;
    void* context;
    cambriana_environment_query query;
} cambriana_environment;

/* Generations completed so far: 0 for the axiom. */
CAMBRIANA_API uint64_t cambriana_environment_step_generation(const cambriana_environment_step* step);
/* The bone tree of the current production. In Step mode at the build's step length and min thickness; in Fit mode, whose
   step length is not known until the last generation, at a 1 cm step with no min thickness. */
CAMBRIANA_API const cambriana_bone* cambriana_environment_step_bones(const cambriana_environment_step* step,
                                                                     uint64_t* count);
CAMBRIANA_API const cambriana_query_module* cambriana_environment_step_query_modules(
    const cambriana_environment_step* step, uint64_t* count);
/* Sets an argument of a query module, for the next generation's rules. A value that is not finite returns
   CAMBRIANA_ERROR_INVALID_ARGUMENT. */
CAMBRIANA_API cambriana_status cambriana_environment_step_set_argument(cambriana_environment_step* step,
                                                                       uint64_t query_module, uint64_t argument,
                                                                       double value);
/* Removes a bone and its descendants from this step on; what would have grown from them does not. The bones read from
   this step do not change until the next. In later steps and the result, the remaining bones' indices are compacted and
   their names are those they would have had with the branch present. */
CAMBRIANA_API cambriana_status cambriana_environment_step_prune(cambriana_environment_step* step, uint64_t bone);

/* As cambriana_build_grammar_model, querying environment between generations. An environment that changes nothing gives
   the same output unless a budget is reached: each query also counts against the work budget. */
CAMBRIANA_API cambriana_status cambriana_build_grammar_model_stepped(const cambriana_grammar* grammar,
                                                                     const char* axiom_name,
                                                                     const cambriana_build_settings* settings,
                                                                     const cambriana_environment* environment,
                                                                     cambriana_cancel* cancel,
                                                                     cambriana_result** result);

#ifdef __cplusplus
}
#endif

#endif
