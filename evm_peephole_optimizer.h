/*
 * evm_peephole_optimizer.h — EVM Storage Packing Peephole Optimizer
 *
 * Post-GVN IR pass for gas-optimal storage coalescing.
 * See evm_peephole_optimizer.c for implementation.
 */

#ifndef ZCC_EVM_PEEPHOLE_OPTIMIZER_H
#define ZCC_EVM_PEEPHOLE_OPTIMIZER_H

#include "ir.h"

/*
 * evm_peephole_optimize()
 *
 * Scans `fn` for adjacent storage slot operations, packs sub-256-bit
 * values into single-slot bit-masked sload/sstore sequences, and
 * emits optimized Yul into `yul_out`.
 *
 * Parameters:
 *   fn             — IR function to optimize (post-GVN)
 *   yul_out        — output buffer for Yul code
 *   yul_cap        — capacity of yul_out in bytes
 *   bribe_gwei_out — receives recommended Flashbots priority fee (gwei)
 *
 * Returns: total gas saved (0 if no optimization possible).
 *
 * INVARIANT: Zero malloc during traversal.  All state is stack-resident.
 */
int evm_peephole_optimize(ir_func_t *fn,
                           char *yul_out, int yul_cap,
                           long *bribe_gwei_out);

#endif /* ZCC_EVM_PEEPHOLE_OPTIMIZER_H */
