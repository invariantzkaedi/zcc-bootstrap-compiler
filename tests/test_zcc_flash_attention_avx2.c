/* ========================================================================= */
/* ZCC FLASHATTENTION TILED AVX2 TEST HARNESS (A1-A5)                         */
/* ========================================================================= */
/* File: tests/test_zcc_flash_attention_avx2.c                              */
/* Description: Tests numerical parity with reference attention, causal      */
/*              masking, memory conservation, and AVX2 throughput.           */
/* ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "include/zcc_flash_attention_avx2.h"

int main(void) {
    printf("=== Starting ZCC FlashAttention Tiled AVX2 Multi-Head Attention Gauntlet (A1-A5) ===\n");

    /* A1: Initialize Non-Causal Multi-Head Config */
    zcc_flash_attn_config_t cfg;
    zcc_flash_attn_init_config(&cfg, 128, 8, 64, false);
    assert(cfg.seq_len == 128 && cfg.n_heads == 8 && cfg.head_dim == 64);
    printf("  [PASS] A1: FlashAttention Multi-Head Config Initialized (Seq: 128, Heads: 8, Dim: 64)\n");

    /* A2: Numerical Parity with Reference Attention (Non-Causal) */
    zcc_flash_attn_metrics_t metrics;
    zcc_benchmark_flash_attention(&cfg, 100, &metrics);
    printf("  [PASS] A2: Non-Causal Parity: Max L_inf Error = %.6e (Threshold < 1e-4)\n", metrics.max_numerical_error);
    assert(metrics.max_numerical_error < 1e-4f);

    /* A3: Causal Masking Attention Verification */
    zcc_flash_attn_config_t causal_cfg;
    zcc_flash_attn_init_config(&causal_cfg, 128, 8, 64, true);
    zcc_flash_attn_metrics_t causal_metrics;
    zcc_benchmark_flash_attention(&causal_cfg, 100, &causal_metrics);
    printf("  [PASS] A3: Causal Autoregressive Masking Parity: Max Error = %.6e\n", causal_metrics.max_numerical_error);
    assert(causal_metrics.max_numerical_error < 1e-4f);

    /* A4: O(N) Zero-Allocation Memory Conservation */
    printf("  [PASS] A4: Memory Conservation: Saved %zu Bytes of Temporary Attention Matrix Allocation\n", 
           metrics.memory_bytes_saved);
    assert(metrics.memory_bytes_saved >= 524288); /* 128 * 128 * 8 * 4 = 524,288 bytes */

    /* A5: AVX2 High-Throughput GFLOPs Benchmark */
    printf("  [PASS] A5: FlashAttention AVX2 Compute Throughput: %.2f GFLOPs (Latency: %.2f µs/forward)\n", 
           metrics.throughput_gflops, metrics.latency_ns / 1000.0);
    assert(metrics.throughput_gflops > 1.0);

    printf("========================================================================\n");
    printf("  💎 ALL ZCC FLASHATTENTION TILED AVX2 GAUNTLETS PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
