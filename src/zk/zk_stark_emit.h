/* ========================================================================= */
/* ZCC STARK PROOF SYNTHESIS & EVM CALLDATA EMISSION MODULE                  */
/* ========================================================================= */
/* File: src/zk/zk_stark_emit.h                                              */
/* Description: Direct synthesis of STARK proof calldata from ZCC SSA IR    */
/*              matching Ethereum Sepolia Profile A verifier (0x6e288921)    */
/* ========================================================================= */

#ifndef ZCC_ZK_STARK_EMIT_H
#define ZCC_ZK_STARK_EMIT_H

#include "ir.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Goldilocks Prime: p = 2^64 - 2^32 + 1 */
#define GOLDILOCKS_PRIME 0xFFFFFFFF00000001ULL

/* Synthesizes STARK proof calldata from optimized IR module and writes to output_path */
int zcc_stark_synthesize_proof(ir_module_t *mod, const char *output_path);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_ZK_STARK_EMIT_H */
