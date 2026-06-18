#ifndef ZCC_VIR_H
#define ZCC_VIR_H

#include "zcc_svg_path_parser.h"
#include <stddef.h>

typedef enum {
    VIR_MOVE = 0,
    VIR_LINE,
    VIR_CUBIC,
    VIR_ARC,
    VIR_CLOSE
} VirOp;

typedef struct {
    uint32_t source_id;
    uint32_t path_id;
    uint32_t flags;
} VirMetadata;

typedef struct {
    VirOp op;
    float coords[8]; // Space for up to 8 coordinates (Cubic control points or Arc parameters)
} VirSegment;

typedef struct {
    VirSegment *segments;
    size_t count;
    size_t capacity;
    VirMetadata metadata;
    int bounds_valid;
    uint32_t state_flags;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
} VirPath;

typedef enum {
    SDF_LINE = 0,
    SDF_CUBIC
} SdfSeedOp;

typedef struct {
    SdfSeedOp op;
    float points[8];
} SdfSeedSegment;

typedef struct {
    SdfSeedSegment *segments;
    size_t count;
} SdfSeed;

typedef struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
} SdfBounds;

typedef enum {
    VIR_STATE_CLEAN             = 0,
    VIR_STATE_DEGENERATE_FREE   = 1 << 0,
    VIR_STATE_ARCS_EXPANDED     = 1 << 1,
    VIR_STATE_CANONICALIZED     = 1 << 2,
    VIR_STATE_BOUNDS_VALID      = 1 << 3,
    VIR_STATE_EXACT_BOUNDS      = 1 << 4
} VirPathStateFlags;

typedef enum {
    VIR_PASS_OK,
    VIR_PASS_NO_CHANGE,
    VIR_PASS_ERROR
} VirPassResult;

typedef enum {
    VIR_PASS_DEGENERATE = 0,
    VIR_PASS_EXPAND_ARCS,
    VIR_PASS_COMPUTE_BOUNDS,
    VIR_PASS_CANONICALIZE,
    VIR_PASS_EXACT_BOUNDS
} VirPass;

typedef struct {
    VirPass pass_id;
    const char *name;
    VirPassResult (*run)(VirPath *);
    uint64_t runs;
    uint64_t mutations;
    uint64_t failures;
    uint32_t required_state;
    uint32_t produced_state;
    uint32_t invalidated_state;
} VirPassDescriptor;

typedef struct {
    uint64_t total_passes;
    uint64_t mutations;
    uint64_t no_change;
    uint64_t failures;
} VirPipelineStats;


/**
 * Creates an empty Vector IR path container.
 */
VirPath* vir_path_create(void);

/**
 * Frees the Vector IR path and all child segments.
 */
void vir_path_free(VirPath *path);

/**
 * Ingestion functions for building VIR segment queues.
 */
int vir_path_add_move_to(VirPath *path, float x, float y);
int vir_path_add_line_to(VirPath *path, float x, float y);
int vir_path_add_cubic_to(VirPath *path, float x1, float y1, float x2, float y2, float x, float y);
int vir_path_add_arc_to(VirPath *path, float rx, float ry, float rotx, float fa, float fs, float x, float y);
int vir_path_add_close(VirPath *path);

/**
 * Optimization passes:
 * Strips degenerate nodes (zero-length segments, null moves) to simplify geometry.
 */
VirPassResult vir_path_optimize_degenerate(VirPath *path);
VirPassResult vir_path_expand_arcs(VirPath *path);
VirPassResult vir_path_canonicalize(VirPath *path);
VirPassResult vir_path_compute_bounds_pass(VirPath *path);
VirPassResult vir_path_compute_exact_bounds_pass(VirPath *path);
void vir_path_invalidate_bounds(VirPath *path);

/**
 * Bounding Box pass:
 * Traverses segment types and calculates the min/max coordinate bounding box.
 */
void vir_path_compute_bounds(const VirPath *path, float *min_x, float *min_y, float *max_x, float *max_y);

/**
 * Serialization adapter:
 * Translates VIR segment operations back into standard SvgPathBuilder string commands.
 */
void vir_path_to_builder(const VirPath *path, SvgPathBuilder *out);

/**
 * Ingestion parser:
 * Parses an SVG path string directly into Vector IR segments.
 */
ZccSvgStatus zcc_svg_parse_to_vir(
    const char *d,
    VirPath *out,
    ZccSvgError *err
);

char* vir_to_svg_path_data(const VirPath *path);
SdfSeed* vir_to_sdf_seed(const VirPath *path);
void sdf_seed_free(SdfSeed *seed);

SdfBounds sdf_seed_compute_bounds(const SdfSeed *seed);
char* sdf_seed_to_glsl(const SdfSeed *seed);

int vir_run_passes(VirPath *path, const VirPass *passes, size_t count);

VirPassDescriptor* vir_pipeline_get_default_registry(size_t *out_count);
void vir_pipeline_reset_telemetry(VirPassDescriptor *registry, size_t count);

int vir_run_pipeline(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats
);

int vir_run_pipeline_until_stable(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats,
    size_t max_iterations
);

int vir_run_pipeline_with_deps(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    const VirPass *passes,
    size_t pass_count,
    VirPipelineStats *stats
);

typedef enum {
    VIR_BACKEND_SVG,
    VIR_BACKEND_SDF,
    VIR_BACKEND_GLSL
} VirBackend;

int vir_prepare_backend(
    VirPath *path,
    VirPassDescriptor *registry,
    size_t registry_count,
    VirBackend backend,
    VirPipelineStats *stats
);

char* vir_pipeline_to_dot(
    VirPassDescriptor *registry,
    size_t registry_count
);

#endif
