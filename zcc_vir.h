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

/* Cache schema version — fold into fingerprint and record headers so that
 * stale cached artifacts are rejected when the pipeline evolves.          */
#define VIR_CACHE_SCHEMA_VERSION 1

typedef enum {
    VIR_STATE_CLEAN             = 0,
    VIR_STATE_DEGENERATE_FREE   = 1 << 0,
    VIR_STATE_ARCS_EXPANDED     = 1 << 1,
    VIR_STATE_CANONICALIZED     = 1 << 2,
    VIR_STATE_BOUNDS_VALID      = 1 << 3,
    VIR_STATE_EXACT_BOUNDS      = 1 << 4,
    VIR_STATE_NORMALIZED        = 1 << 5,
    VIR_STATE_LOCALIZED         = 1 << 6
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
    VIR_PASS_EXACT_BOUNDS,
    VIR_PASS_NORMALIZE,
    VIR_PASS_LOCALIZE
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
VirPassResult vir_path_normalize(VirPath *path);
VirPassResult vir_path_localize(VirPath *path);
int vir_paths_equivalent(const VirPath *a, const VirPath *b, float epsilon);
uint64_t vir_path_fingerprint(const VirPath *path, float epsilon);
uint64_t vir_path_canonical_fingerprint(const VirPath *path, float epsilon);
void vir_path_invalidate_bounds(VirPath *path);

void vir_cache_init(void);
void vir_cache_clear(void);
void vir_cache_shutdown(void);

typedef struct {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
} VirCacheStats;

VirCacheStats vir_cache_get_stats(void);
void vir_cache_reset_stats(void);

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

typedef enum {
    VIR_REGISTRY_OK = 0,
    VIR_REGISTRY_ERR_NULL,
    VIR_REGISTRY_ERR_DUPLICATE_PASS,
    VIR_REGISTRY_ERR_DUPLICATE_PRODUCER,
    VIR_REGISTRY_ERR_ORPHAN_REQUIRED_STATE,
    VIR_REGISTRY_ERR_UNREACHABLE_PASS,
    VIR_REGISTRY_ERR_INVALID_INVALIDATION,
    VIR_REGISTRY_ERR_CYCLE,
    VIR_REGISTRY_ERR_STATE_ALIAS
} VirRegistryValidationResult;

typedef struct {
    VirRegistryValidationResult status;
    const char *message;
    VirPass pass_id;
    uint32_t state_mask;
} VirRegistryValidationError;

VirRegistryValidationResult vir_validate_registry(
    const VirPassDescriptor *registry,
    size_t registry_count,
    VirRegistryValidationError *err
);

/* ── Artifact Manifest ─────────────────────────────────────────────────────
 * A self-describing identity envelope that binds a canonical fingerprint to
 * the full convergence state of a VirPath at capture time.  Used for cache
 * validation, provenance tracking, and regression comparisons.           */

typedef struct {
    uint64_t canonical_fingerprint; /* FNV-1a over localized geometry         */
    uint32_t schema_version;        /* VIR_CACHE_SCHEMA_VERSION at capture    */
    uint32_t state_flags;           /* path->state_flags at capture time      */
    uint32_t segment_count;         /* path->count at capture time            */
    /* Exact bounds — populated only when VIR_STATE_EXACT_BOUNDS is set in
     * state_flags; otherwise all four fields are 0.0f.                       */
    float    min_x;
    float    min_y;
    float    max_x;
    float    max_y;
} VirArtifactManifest;

/* Capture the full identity envelope for a path.
 * Uses vir_path_canonical_fingerprint internally (zero-allocation fast-path
 * when path is already VIR_STATE_LOCALIZED).
 * Bounds are copied from path->min_x/y/max_x/y only when VIR_STATE_EXACT_BOUNDS
 * is set; otherwise they are zero-filled — manifest generation is non-mutating. */
VirArtifactManifest vir_path_manifest(const VirPath *path, float epsilon);

/* Verify a live path against a previously captured manifest.
 * Checks: fingerprint, schema_version, state_flags, segment_count, bounds.
 * Returns 1 if all fields agree, 0 on the first mismatch.                   */
int vir_manifest_verify(const VirPath *path,
                        const VirArtifactManifest *manifest,
                        float epsilon);

/* ── Execution Plan ────────────────────────────────────────────────────────
 * A dependency-resolved, ordered list of passes required to bring a path
 * to a target state.  Building a plan does NOT mutate the path.           */

#define VIR_EXECUTION_PLAN_MAX 32

typedef struct {
    VirPass  passes[VIR_EXECUTION_PLAN_MAX]; /* ordered pass sequence        */
    size_t   count;                          /* number of passes in plan     */
    uint32_t target_state;   /* the requested convergence state mask         */
    uint32_t current_state;  /* path->state_flags at plan-build time         */
} VirExecutionPlan;

/* Build a dependency-resolved execution plan without mutating the path.
 * Returns 1 on success; 0 if target_state cannot be satisfied by registry
 * or if plan capacity (VIR_EXECUTION_PLAN_MAX) is exceeded.               */
int vir_build_execution_plan(const VirPath *path,
                             const VirPassDescriptor *registry,
                             size_t registry_count,
                             uint32_t target_state,
                             VirExecutionPlan *out);

/* Execute a pre-built plan against a path.
 * Returns 1 if every pass in the plan succeeds; 0 on the first failure.   */
int vir_execute_plan(VirPath *path,
                     const VirExecutionPlan *plan,
                     const VirPassDescriptor *registry,
                     size_t registry_count,
                     VirPipelineStats *stats);

/* ── Provenance Receipt ─────────────────────────────────────────────────────
 * Emit a malloc-owned JSON string capturing the canonical identity of a path
 * together with pipeline execution and cache telemetry.
 *
 * NULL stats or NULL cache are tolerated — those sections are zero-filled.
 * The caller must free() the returned string.
 * Returns NULL only on allocation failure.                                  */
char *vir_pipeline_provenance_json(const VirPath        *path,
                                   const VirPipelineStats *stats,
                                   const VirCacheStats    *cache);

/* ── State Flag Diagnostics ─────────────────────────────────────────────────
 * Human-readable names for VirPathStateFlags bitmasks.                      */

/* Return the literal name of a single power-of-two state flag, e.g.
 * VIR_STATE_CANONICALIZED → "CANONICALIZED".
 * Returns "UNKNOWN" for unrecognised bits.
 * The returned string is a string literal — do NOT free it.                 */
const char *vir_state_flag_name(uint32_t flag);

/* Build a pipe-separated string of all set flag names in `flags`, e.g.
 * "ARCS_EXPANDED | CANONICALIZED | EXACT_BOUNDS | LOCALIZED".
 * Returns "CLEAN" when flags == 0.
 * The returned string is malloc-owned — the caller must free() it.
 * Returns NULL only on allocation failure.                                  */
char *vir_state_flags_to_string(uint32_t flags);

/* ── Geometry Metrics ───────────────────────────────────────────────────────
 * Quantitative geometry diagnostics computed in a single O(N) walk.
 * The path is never mutated; works on any convergence state.               */

typedef struct {
    uint32_t move_count;   /* VIR_MOVE segments                              */
    uint32_t line_count;   /* VIR_LINE segments                              */
    uint32_t cubic_count;  /* VIR_CUBIC segments                             */
    uint32_t arc_count;    /* VIR_ARC segments                               */
    uint32_t close_count;  /* VIR_CLOSE segments                             */
    uint32_t total_count;  /* total segment count (all ops)                  */
    /* Approximate arc-length: sum of straight-line chord distances
     * (LINE: |P1-P0|, CUBIC: |P3-P0|, ARC: chord |end-start|).
     * Exact for lines; first-order approximation for curves.               */
    float approx_length;
    /* Signed area via the shoelace formula accumulated over all implicit
     * line segments (MOVE→LINE→LINE→...→CLOSE).  Positive = CCW,
     * negative = CW (screen coordinates, y-down).                         */
    float signed_area;
} VirGeometryMetrics;

/* Compute geometry metrics for a path in one O(N) walk.
 * Safe to call on any VirPath regardless of convergence state.
 * Returns a zero-filled VirGeometryMetrics when path is NULL.             */
VirGeometryMetrics vir_path_compute_metrics(const VirPath *path);

/* ── Cache Record Header ────────────────────────────────────────────────────
 * A fixed-size, self-validating envelope for persistent VIR cache artifacts.
 * The header is naturally aligned (no padding) and sized at exactly 48 bytes.
 * It can prefix any serialized VIR artifact blob on disk or in shared memory.*/

/* Magic constant — "VIRC" in little-endian. */
#define VIR_CACHE_RECORD_MAGIC 0x43524956U

typedef struct {
    uint32_t magic;              /* VIR_CACHE_RECORD_MAGIC                   */
    uint32_t schema_version;     /* VIR_CACHE_SCHEMA_VERSION at capture       */
    uint64_t canonical_fingerprint; /* vir_path_canonical_fingerprint result  */
    uint32_t state_flags;        /* path->state_flags at serialization time   */
    uint32_t segment_count;      /* path->count at serialization time         */
    float    min_x;              /* exact bounds — 0.0f when EXACT_BOUNDS absent */
    float    min_y;
    float    max_x;
    float    max_y;
    uint32_t payload_size;       /* byte size of the artifact blob after header */
    uint32_t header_crc32;       /* CRC32 (IEEE 802.3) over all preceding fields */
} VirCacheRecordHeader;

/* Populate a VirCacheRecordHeader from a VirPath.
 * Uses vir_path_manifest internally — path is never mutated.
 * payload_size is the caller-declared byte size of the artifact blob that
 * will follow the header on disk / in the cache stream.
 * Computes header_crc32 over all fields except itself.                     */
VirCacheRecordHeader vir_cache_record_header_init(const VirPath *path,
                                                   uint32_t payload_size);

/* Validate a previously written VirCacheRecordHeader.
 * Checks: magic == VIR_CACHE_RECORD_MAGIC,
 *         schema_version == VIR_CACHE_SCHEMA_VERSION,
 *         header_crc32 matches recomputed CRC over preceding fields.
 * Returns 1 if all checks pass, 0 on the first failure.                   */
int vir_cache_record_header_validate(const VirCacheRecordHeader *hdr);

#endif
