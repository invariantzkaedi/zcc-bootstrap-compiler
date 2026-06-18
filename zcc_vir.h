#ifndef ZCC_VIR_H
#define ZCC_VIR_H

#include "zcc_svg_path_parser.h"
#include <stddef.h>

typedef enum {
    VIR_MOVE = 0,
    VIR_LINE,
    VIR_CUBIC,
    VIR_CLOSE
} VirOp;

typedef struct {
    VirOp op;
    float coords[6]; // Space for up to 6 coordinates (Cubic Bezier control/end points)
} VirSegment;

typedef struct {
    VirSegment *segments;
    size_t count;
    size_t capacity;
} VirPath;

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
int vir_path_add_close(VirPath *path);

/**
 * Optimization passes:
 * Strips degenerate nodes (zero-length segments, null moves) to simplify geometry.
 */
void vir_path_optimize_degenerate(VirPath *path);

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

#endif
