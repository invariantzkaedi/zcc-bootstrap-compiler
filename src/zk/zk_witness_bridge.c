/* ========================================================================= */
/* ZCC ZK WITNESS BRIDGE: AIR MATRIX TO CIRCOM WITNESS JSON EXPORTER         */
/* ========================================================================= */
/* File: src/zk/zk_witness_bridge.c                                          */
/* Description: Bridges ZCC BabyBear AIR execution trace matrices to Circom  */
/*              witness inputs for recursive STARK -> SNARK verification.    */
/* ========================================================================= */

#include "src/zk/zk_witness_bridge.h"
#include <stdlib.h>
#include <string.h>

bool zk_air_to_circom_witness(const ZkAirTraceContext *air_ctx, ZkCircomWitnessInput *witness) {
    if (!air_ctx || !witness) return false;

    memset(witness, 0, sizeof(ZkCircomWitnessInput));

    /* Populate memory snapshot and invariant checks from AIR trace matrix */
    size_t steps = air_ctx->n_steps;
    if (steps > ZK_CIRCOM_N_INVARIANTS) steps = ZK_CIRCOM_N_INVARIANTS;

    for (size_t i = 0; i < steps; i++) {
        bb_elem_t dst_val = air_ctx->matrix[i * ZK_AIR_TRACE_WIDTH + COL_DST_VAL];
        bb_elem_t mem_addr = air_ctx->matrix[i * ZK_AIR_TRACE_WIDTH + COL_MEM_ADDR];
        bb_elem_t is_arith = air_ctx->matrix[i * ZK_AIR_TRACE_WIDTH + COL_IS_ARITH];

        witness->mem_snapshot[i * 100] = dst_val;
        witness->expected[i] = dst_val;
        witness->balances[i] = is_arith ? 100 : 0;
        witness->acl_state[i] = 1; /* Valid execution state */
    }

    /* Fill remaining invariants if steps < 80 */
    for (size_t i = steps; i < ZK_CIRCOM_N_INVARIANTS; i++) {
        witness->mem_snapshot[i * 100] = 0;
        witness->expected[i] = 0;
        witness->balances[i] = 0;
        witness->acl_state[i] = 1;
    }

    /* Symbolic anchor / chaos seed */
    witness->chaos_in = (air_ctx->n_steps > 0) ? (air_ctx->matrix[0] ? air_ctx->matrix[0] : 1) : 42;

    return true;
}

bool zk_export_circom_witness_json(const ZkCircomWitnessInput *witness, const char *filepath) {
    if (!witness || !filepath) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "{\n");

    /* 1. mem_snapshot */
    fprintf(f, "  \"mem_snapshot\": [");
    for (size_t i = 0; i < ZK_CIRCOM_MEM_SNAPSHOT_SIZE; i++) {
        fprintf(f, "%u%s", witness->mem_snapshot[i], (i == ZK_CIRCOM_MEM_SNAPSHOT_SIZE - 1) ? "" : ", ");
    }
    fprintf(f, "],\n");

    /* 2. expected */
    fprintf(f, "  \"expected\": [");
    for (size_t i = 0; i < ZK_CIRCOM_N_INVARIANTS; i++) {
        fprintf(f, "%u%s", witness->expected[i], (i == ZK_CIRCOM_N_INVARIANTS - 1) ? "" : ", ");
    }
    fprintf(f, "],\n");

    /* 3. balances */
    fprintf(f, "  \"balances\": [");
    for (size_t i = 0; i < ZK_CIRCOM_N_INVARIANTS; i++) {
        fprintf(f, "%u%s", witness->balances[i], (i == ZK_CIRCOM_N_INVARIANTS - 1) ? "" : ", ");
    }
    fprintf(f, "],\n");

    /* 4. acl_state */
    fprintf(f, "  \"acl_state\": [");
    for (size_t i = 0; i < ZK_CIRCOM_N_INVARIANTS; i++) {
        fprintf(f, "%u%s", witness->acl_state[i], (i == ZK_CIRCOM_N_INVARIANTS - 1) ? "" : ", ");
    }
    fprintf(f, "],\n");

    /* 5. chaos_in */
    fprintf(f, "  \"chaos_in\": %u\n", witness->chaos_in);

    fprintf(f, "}\n");
    fclose(f);
    return true;
}
