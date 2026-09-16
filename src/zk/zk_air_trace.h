/* ========================================================================= */
/* ZCC VERIFIABLE COMPILATION: BABYBEAR AIR TRACE GENERATOR                  */
/* ========================================================================= */
/* File: src/zk/zk_air_trace.h                                               */
/* Description: Generates Algebraic Intermediate Representation (AIR) trace  */
/*              matrices over the BabyBear prime field (p = 2^31 - 2^27 + 1) */
/*              to prove semantic compilation correctness in Zero-Knowledge. */
/* ========================================================================= */

#ifndef ZCC_ZK_AIR_TRACE_H
#define ZCC_ZK_AIR_TRACE_H

#include "prelude.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BabyBear Prime Field: p = 2^31 - 2^27 + 1 = 2013265921 */
#define BABYBEAR_PRIME 2013265921ULL

/* AIR Matrix Dimensions */
#define ZK_AIR_TRACE_WIDTH 16
#define ZK_MAX_TRACE_STEPS 65536
#define ZK_MAX_PUBLIC_INPUTS 16
#define ZK_MAX_VREGS 256

typedef uint32_t bb_elem_t;

/* AIR Trace Column Layout */
enum ZkAirColumn {
    COL_STEP = 0,
    COL_PC = 1,
    COL_OPCODE = 2,
    COL_DST_REG = 3,
    COL_SRC0_VAL = 4,
    COL_SRC1_VAL = 5,
    COL_DST_VAL = 6,
    COL_IS_ARITH = 7,
    COL_IS_BRANCH = 8,
    COL_IS_MEM = 9,
    COL_MEM_ADDR = 10,
    COL_MEM_VAL = 11,
    COL_FLAG_BRANCH_TAKEN = 12,
    COL_AUX0 = 13, /* Source 0 register ID / Program Hash */
    COL_AUX1 = 14, /* Source 1 register ID / Branch Target */
    COL_CHECKSUM = 15
};

typedef struct {
    bb_elem_t *matrix;      /* Size: n_steps * ZK_AIR_TRACE_WIDTH */
    size_t     n_steps;
    size_t     capacity;
    uint32_t   vreg_state[MAX_INSTRS];
} ZkAirTraceContext;

/* Verification Context for Bound Program, Public Inputs, and Expected Outputs */
typedef struct {
    uint32_t   expected_program_hash;
    bb_elem_t  public_inputs[ZK_MAX_PUBLIC_INPUTS];
    size_t     n_inputs;
    bb_elem_t  expected_output;
    bool       require_output_bound;
    bool       require_program_bound;
} ZkAirVerifyContext;

/* BabyBear Field Arithmetic */
static inline bb_elem_t bb_add(bb_elem_t a, bb_elem_t b) {
    uint64_t sum = (uint64_t)a + b;
    return (bb_elem_t)(sum % BABYBEAR_PRIME);
}

static inline bb_elem_t bb_sub(bb_elem_t a, bb_elem_t b) {
    uint64_t diff = ((uint64_t)a + BABYBEAR_PRIME) - b;
    return (bb_elem_t)(diff % BABYBEAR_PRIME);
}

static inline bb_elem_t bb_mul(bb_elem_t a, bb_elem_t b) {
    uint64_t prod = (uint64_t)a * b;
    return (bb_elem_t)(prod % BABYBEAR_PRIME);
}

/* Trace Lifecycle */
ZkAirTraceContext *zk_air_trace_create(size_t capacity);
void zk_air_trace_destroy(ZkAirTraceContext *ctx);

/* Generation from Function SSA IR */
int zk_air_trace_generate_fn(ZkAirTraceContext *ctx, const Function *fn);

/* Legacy Constraint Verification (Backward Compatibility) */
bool zk_air_verify_arithmetic_constraints(const ZkAirTraceContext *ctx);
bool zk_air_verify_control_flow_constraints(const ZkAirTraceContext *ctx);

/* Remediation: Full Soundness Verifier with Def-Use & Boundary Checks */
bool zk_air_verify_full_soundness(const ZkAirTraceContext *ctx, const ZkAirVerifyContext *vctx);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_ZK_AIR_TRACE_H */
