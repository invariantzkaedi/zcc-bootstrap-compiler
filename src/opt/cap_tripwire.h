/* ========================================================================= */
/* ZCC CAPABILITY TRIPWIRE & AFFINE MEMORY SAFETY (Safe-C / CapSSA)          */
/* ========================================================================= */
/* File: src/opt/cap_tripwire.h                                              */
/* Description: Compile-time memory capability tracking, loop-bound          */
/*              safety propagation, tripwire injection, and check elision.   */
/* ========================================================================= */

#ifndef ZCC_CAP_TRIPWIRE_H
#define ZCC_CAP_TRIPWIRE_H

#include "prelude.h"
#include "src/opt/loop_validator.h"
#include "zcc_opt_metrics.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAP_PROVABLY_SAFE = 0,    /* Interval strictly in [0, Capacity) -> 0 overhead */
    CAP_LOOP_BOUNDED,         /* Trip count strictly bounded by capacity */
    CAP_DYNAMIC_UNBOUNDED,    /* Dynamic unbounded offset -> inject tripwire */
    CAP_REVOKED_USE_AFTER_FREE/* Freed pointer access -> immediate trap */
} CapStatus;

typedef struct {
    RegID   base_reg;
    int64_t capacity;
    int64_t min_offset;
    int64_t max_offset;
    CapStatus status;
} AffineCapability;

typedef struct {
    uint32_t memory_accesses_inspected;
    uint32_t bounds_checks_elided;
    uint32_t tripwires_injected;
    uint32_t uaf_traps_injected;
} CapTripwireMetrics;

/* Core optimization pass */
bool opt_cap_tripwire_pass(Function *fn, OptMetricsSink *metrics);

/* Capability analysis queries */
AffineCapability analyze_pointer_capability(Function *fn, RegID ptr_reg);
bool is_access_provably_safe(const AffineCapability *cap, int64_t access_size);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_CAP_TRIPWIRE_H */
