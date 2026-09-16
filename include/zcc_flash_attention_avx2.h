/* ========================================================================= */
/* ZCC FLASHATTENTION TILED AVX2 MULTI-HEAD ATTENTION SUBSTRATE (A1-A5)       */
/* ========================================================================= */
/* File: include/zcc_flash_attention_avx2.h                                  */
/* Description: Exact L1 cache-tiled multi-head attention with online softmax*/
/*              rescaling and zero temporary quadratic buffer allocations.   */
/* ========================================================================= */

#ifndef ZCC_FLASH_ATTENTION_AVX2_H
#define ZCC_FLASH_ATTENTION_AVX2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_ATTN_BLOCK_SIZE 32
#define FLASH_ATTN_HEAD_DIM   64

/* Multi-Head Attention Configuration */
typedef struct {
    uint32_t seq_len;
    uint32_t n_heads;
    uint32_t head_dim;
    float    scale; /* 1.0 / sqrt(head_dim) */
    bool     causal_mask;
} zcc_flash_attn_config_t;

/* FlashAttention Execution Metrics */
typedef struct {
    double total_flops;
    double latency_ns;
    double throughput_gflops;
    size_t memory_bytes_saved;
    float  max_numerical_error;
} zcc_flash_attn_metrics_t;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (A1 - A5)                                             */
/* ------------------------------------------------------------------------- */

/* A1: Initialize FlashAttention Configuration */
void zcc_flash_attn_init_config(zcc_flash_attn_config_t *cfg, 
                                uint32_t seq_len, 
                                uint32_t n_heads, 
                                uint32_t head_dim, 
                                bool causal);

/* A2: Tiled Online Softmax FlashAttention Core Kernel (AVX2 Accelerated) */
void zcc_flash_attention_forward_avx2(const zcc_flash_attn_config_t *cfg,
                                      const float *Q, /* [seq_len, n_heads, head_dim] */
                                      const float *K, /* [seq_len, n_heads, head_dim] */
                                      const float *V, /* [seq_len, n_heads, head_dim] */
                                      float *O);      /* [seq_len, n_heads, head_dim] */

/* A3: Standard Reference Quadratic Attention (For Bit-Exact Parity Testing) */
void zcc_reference_attention_forward(const zcc_flash_attn_config_t *cfg,
                                     const float *Q,
                                     const float *K,
                                     const float *V,
                                     float *O);

/* A4: Micro-benchmark and Performance Profiler */
void zcc_benchmark_flash_attention(const zcc_flash_attn_config_t *cfg, 
                                   size_t iterations, 
                                   zcc_flash_attn_metrics_t *metrics);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_FLASH_ATTENTION_AVX2_H */
