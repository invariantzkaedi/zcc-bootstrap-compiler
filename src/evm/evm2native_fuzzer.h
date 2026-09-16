/* ========================================================================= */
/* ZCC EVM2NATIVE HIGH-THROUGHPUT MEV & 1M FUZZING HARNESS                   */
/* ========================================================================= */
/* File: src/evm/evm2native_fuzzer.h                                         */
/* Description: Automated lifting of Uniswap / Curve bytecode, sub-15ns      */
/*              vector JIT benchmark, and 1,000,000-run parity fuzzing.      */
/* ========================================================================= */

#ifndef ZCC_EVM2NATIVE_FUZZER_H
#define ZCC_EVM2NATIVE_FUZZER_H

#include "src/evm/evm2native_pass.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double interpreted_avg_ns;
    double avx2_jit_avg_ns;
    double speedup_multiplier;
    uint64_t swaps_executed;
} MevBenchmarkResult;

typedef struct {
    uint64_t total_iterations;
    uint64_t matches;
    uint64_t mismatches;
    uint64_t overflows_handled;
    bool     parity_verified;
} FuzzCampaignResult;

/* Ingest raw EVM bytecode, lift to IR, and apply EVM2Native optimizations */
ir_func_t *evm2native_lift_and_optimize(const uint8_t *bytecode, size_t len, Evm2NativeMetrics *metrics);

/* Complete bytecode ingestion and assembly emission helper */
bool evm2native_process_bytecode(const uint8_t *bytecode, size_t len, const char *out_asm_path, uint32_t *out_stack_ops);

/* Execute high-precision sub-15ns MEV swap arbitrage benchmark */
MevBenchmarkResult evm2native_benchmark_mev(uint32_t iterations);

/* Run 1,000,000 randomized transaction executions for parity checking */
FuzzCampaignResult evm2native_run_fuzz_campaign(uint64_t num_runs);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_EVM2NATIVE_FUZZER_H */
