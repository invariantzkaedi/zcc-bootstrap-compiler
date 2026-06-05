/*
 * ir_telemetry_stub.c — No-op stubs for self-host build.
 *
 * The real ir_telemetry.c uses POSIX socket headers (<sys/socket.h>,
 * <netinet/in.h>) which ZCC cannot parse during self-hosting.
 * These stubs satisfy the link requirements of ir_pass_manager.c
 * without any POSIX dependency.
 *
 * The real ir_telemetry.c is compiled separately by GCC and linked
 * into the production compiler_passes_ir.c path only.
 */
#include "ir_telemetry.h"
#include "zcc_telemetry.h"
#ifndef ZCC_REAL_TELEMETRY
void ir_telem_init(void) {}
void ir_telem_pass(const char *pass_name,
                   int func_count,
                   int nodes_before,
                   int nodes_after,
                   int nodes_deleted,
                   int nodes_modified) {
    (void)pass_name; (void)func_count; (void)nodes_before;
    (void)nodes_after; (void)nodes_deleted; (void)nodes_modified;
}
void ir_telem_summary(int total_funcs,
                      int total_nodes_before,
                      int total_nodes_after,
                      int pass_count,
                      const char **pass_names) {
    (void)total_funcs; (void)total_nodes_before; (void)total_nodes_after;
    (void)pass_count; (void)pass_names;
}
void ir_telem_shutdown(void) {}
void ir_telemetry_enable_stdout(void) {}
void zcc_telem_phase(int phase, const char *phase_name, const char *status, int duration_us,
                     const char *metric_key1, long long metric_val1,
                     const char *metric_key2, long long metric_val2,
                     const char *metric_key3, long long metric_val3) {
    (void)phase; (void)phase_name; (void)status; (void)duration_us;
    (void)metric_key1; (void)metric_val1;
    (void)metric_key2; (void)metric_val2;
    (void)metric_key3; (void)metric_val3;
}
void telemetry_init(const char* host, int port) {
    (void)host;
    (void)port;
}
void telemetry_emit_node(TelemetryOp op, const char* node_type, int depth) {
    (void)op;
    (void)node_type;
    (void)depth;
}
void telemetry_emit_mem(TelemetryOp op, uint64_t ptr_address, size_t size) {
    (void)op;
    (void)ptr_address;
    (void)size;
}
void telemetry_emit_coverage(float coverage_pct) {
    (void)coverage_pct;
}
void telemetry_close(void) {
}
void zcc_pass_transient_locks(void) { /* G332 stub */ }
#endif

/* G332 — must be outside #ifndef ZCC_REAL_TELEMETRY so it links
   regardless of telemetry mode */
void zcc_pass_transient_locks(void) {}
