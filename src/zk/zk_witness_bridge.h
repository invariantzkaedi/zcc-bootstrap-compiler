/* ========================================================================= */
/* ZCC ZK WITNESS BRIDGE: AIR MATRIX TO CIRCOM WITNESS JSON EXPORTER         */
/* ========================================================================= */
/* File: src/zk/zk_witness_bridge.h                                          */
/* Description: Bridges ZCC BabyBear AIR execution trace matrices to Circom  */
/*              witness inputs for recursive STARK -> SNARK verification.    */
/* ========================================================================= */

#ifndef ZCC_ZK_WITNESS_BRIDGE_H
#define ZCC_ZK_WITNESS_BRIDGE_H

#include "src/zk/zk_air_trace.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZK_CIRCOM_N_INVARIANTS 80
#define ZK_CIRCOM_MEM_SNAPSHOT_SIZE 8000

typedef struct {
    uint32_t mem_snapshot[ZK_CIRCOM_MEM_SNAPSHOT_SIZE];
    uint32_t expected[ZK_CIRCOM_N_INVARIANTS];
    uint32_t balances[ZK_CIRCOM_N_INVARIANTS];
    uint32_t acl_state[ZK_CIRCOM_N_INVARIANTS];
    uint32_t chaos_in;
} ZkCircomWitnessInput;

/* Convert AIR trace context to Circom witness structure */
bool zk_air_to_circom_witness(const ZkAirTraceContext *air_ctx, ZkCircomWitnessInput *witness);

/* Export witness structure to JSON file formatted for Circom / SnarkJS */
bool zk_export_circom_witness_json(const ZkCircomWitnessInput *witness, const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_ZK_WITNESS_BRIDGE_H */
