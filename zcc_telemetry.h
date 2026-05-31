#ifndef ZCC_TELEMETRY_H
#define ZCC_TELEMETRY_H

#include <stdint.h>
#include <stddef.h>

// Operation codes mapping to kinematic states
typedef enum {
    AST_ENTER_NODE = 0,
    AST_EXIT_NODE = 1,
    MEM_ALLOC = 2,
    MEM_FREE = 3,
    COVERAGE_SYNC = 4
} TelemetryOp;

void telemetry_init(const char* host, int port);
void telemetry_emit_node(TelemetryOp op, const char* node_type, int depth);
void telemetry_emit_mem(TelemetryOp op, uint64_t ptr_address, size_t size);
void telemetry_emit_coverage(float coverage_pct);
void telemetry_close();

#endif // ZCC_TELEMETRY_H
