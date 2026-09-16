/* ========================================================================= */
/* ZCC EVM2NATIVE ACCELERATOR: STACK ELIMINATION & AVX2 LOWERING PASS       */
/* ========================================================================= */
/* File: src/evm/evm2native_pass.h                                           */
/* Description: Reconstructs high-level 3-address SSA, eliminates redundant  */
/*              EVM stack operations, and lowers 256-bit operations to AVX2. */
/* ========================================================================= */

#ifndef ZCC_EVM2NATIVE_PASS_H
#define ZCC_EVM2NATIVE_PASS_H

#include "ir.h"
#include "evm_lifter.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t stack_ops_eliminated;
    uint32_t avx2_vector_ops_emitted;
    uint32_t storage_memoizations;
    double   estimated_speedup_factor;
} Evm2NativeMetrics;

/* Core pass functions */
bool evm2native_optimize_ir(ir_func_t *func, Evm2NativeMetrics *metrics);

/* Native x86-64 AVX2 assembly emitter for lifted EVM functions */
int evm2native_emit_avx2_asm(const ir_func_t *func, char *out_buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_EVM2NATIVE_PASS_H */
