/*
 * pq_sqlite_shim.c -- sandbox-safe SQLite compatibility shim placeholder
 *
 * Dockerfile.sandbox copies this file into /app so sandbox jobs can inspect or
 * compile it alongside SQLite-facing probes.  The current sandbox image does
 * not link this file by default; keeping it valid C prevents missing-file
 * Docker build failures while leaving future SQLite hooks explicit and
 * reviewable.
 *
 * Security posture:
 *   - No dynamic code execution.
 *   - No filesystem, network, process, or environment access.
 *   - No allocator ownership transfer.
 *   - Deterministic functions only.
 */

#include <stddef.h>

#define PQ_SQLITE_SHIM_VERSION_MAJOR 0
#define PQ_SQLITE_SHIM_VERSION_MINOR 1
#define PQ_SQLITE_SHIM_VERSION_PATCH 0

const char *pq_sqlite_shim_version(void) {
    return "pq_sqlite_shim/0.1.0";
}

int pq_sqlite_shim_is_available(void) {
    return 1;
}

int pq_sqlite_shim_validate_buffer(const void *buf, size_t len) {
    if (len == 0) {
        return 1;
    }
    return buf != NULL;
}

int pq_sqlite_shim_status_code(void) {
    return 0;
}