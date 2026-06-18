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
    VIR_PASS_OK,
    VIR_PASS_NO_CHANGE,
    VIR_PASS_ERROR
} VirPassResult;

typedef enum {
    VIR_PASS_DEGENERATE = 0,
    VIR_PASS_EXPAND_ARCS,
    VIR_PASS_COMPUTE_BOUNDS,
    VIR_PASS_CANONICALIZE
} VirPass;


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

#endif
