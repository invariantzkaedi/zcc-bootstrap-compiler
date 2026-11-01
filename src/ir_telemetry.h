/*
 * ir_telemetry.h — ZCC IR Pass Telemetry Emitter
 * ================================================
 * Fire-and-forget UDP telemetry to Gods Eye (port 41337).
 * Emits per-pass node-count metrics after each optimization pass.
 *
 * Gated by ZCC_EMIT_TELEMETRY=1 environment variable.
 * When unset, all functions are no-ops (zero overhead).
 *
 * Compiled by GCC only (linked separately, NOT in zcc.c).
 */

#ifndef ZCC_IR_TELEMETRY_H
#define ZCC_IR_TELEMETRY_H

/* Initialize telemetry UDP socket.
 * Reads ZCC_EMIT_TELEMETRY env var. If unset or "0", all
 * subsequent calls are no-ops.
 * Called once at pass-manager startup. */
void ir_telem_init(void);

/* Emit per-pass metrics after each pass completes.
 * Called from ir_pm_run() after each pass iteration. */
void ir_telem_pass(const char *pass_name,
                   int func_count,
                   int nodes_before,
                   int nodes_after,
                   int nodes_deleted,
                   int nodes_modified);

/* Emit compilation summary after ir_pm_run() completes. */
void ir_telem_summary(int total_funcs,
                      int total_nodes_before,
                      int total_nodes_after,
                      int pass_count,
                      const char **pass_names);

/* Shutdown — close socket. */
void ir_telem_shutdown(void);

/* Enable standard output redirection for local corpus harvesting */
void ir_telemetry_enable_stdout(void);

/* ------------------------------------------------------------------ *
 * PRIME manifold telemetry event IDs (ir_manifold.c)
 * Clustered under 0x4D** ("M") -- safe from existing class collision.
 * These are consumed by the manifold engine's internal mf_emit() shim;
 * Phase 4 will route through ir_telemetry_emit() with HMAC envelope.
 * ------------------------------------------------------------------ */
#define IR_TELEM_MANIFOLD_INIT          0x4D00
#define IR_TELEM_MANIFOLD_DESTROY       0x4D01
#define IR_TELEM_MANIFOLD_PROJECT_BEGIN 0x4D02
#define IR_TELEM_MANIFOLD_PROJECT_END   0x4D03
#define IR_TELEM_MANIFOLD_EVOLVE_STEP   0x4D04  /* payload: ir_manifold_stats_t */
#define IR_TELEM_MANIFOLD_CONVERGE      0x4D05
#define IR_TELEM_MANIFOLD_DIVERGE       0x4D06  /* clamp hit -- investigate      */
#define IR_TELEM_MANIFOLD_BIFURCATION   0x4D07  /* sign flip in gradient field   */
#define IR_TELEM_MANIFOLD_SCORE_QUERY   0x4D08  /* sampled, rate-limited         */
#define IR_TELEM_MANIFOLD_PASS_BEGIN    0x4D09
#define IR_TELEM_MANIFOLD_PASS_END      0x4D0A  /* payload: removed/added delta  */
#define IR_TELEM_MANIFOLD_EXPORT        0x4D0B
#define IR_TELEM_MANIFOLD_GUARD_SKIP    0x4D0C  /* CG-IR-011 no-op fired         */

/* ------------------------------------------------------------------ *
 * Peephole Telemetry
 * ------------------------------------------------------------------ */
#define IR_TELEM_PEEPHOLE_METRICS       0x5000

/* Emit peephole optimizations fixpoint data */
void ir_telem_peephole(const char *func_name, int iterations, int total_fires, int exhausted);

#endif /* ZCC_IR_TELEMETRY_H */
