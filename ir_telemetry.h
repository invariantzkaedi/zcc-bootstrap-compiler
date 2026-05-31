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

/* Emit compiler phase telemetry (e.g. lexer, parser, codegen, peephole, linking) */
void zcc_telem_phase(int phase, const char *phase_name, const char *status, int duration_us,
                     const char *metric_key1, long long metric_val1,
                     const char *metric_key2, long long metric_val2,
                     const char *metric_key3, long long metric_val3);

#endif /* ZCC_IR_TELEMETRY_H */


