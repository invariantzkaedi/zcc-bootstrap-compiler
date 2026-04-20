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
