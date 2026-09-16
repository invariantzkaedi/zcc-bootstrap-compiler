/* ========================================================================= */
/* ZCC SPECULATIVE NPU / GPU ASYNCHRONOUS DMA RING SCHEDULER (R1-R5)         */
/* ========================================================================= */
/* File: include/zcc_speculative_dma_ring.h                                  */
/* Description: Shared Zero-Copy Lock-Free POSIX DMA Ring Buffer, NPU        */
/*              Speculative Draft Token Producer, GPU/AVX2 Batch Target      */
/*              Verifier, and Dynamic Speculative Tree Pruner.               */
/* ========================================================================= */

#ifndef ZCC_SPECULATIVE_DMA_RING_H
#define ZCC_SPECULATIVE_DMA_RING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_DMA_RING_CAPACITY  64
#define ZCC_MAX_SPEC_BRANCHES  8
#define ZCC_MAX_TOKEN_DRAFT    16

/* Computational Target Hardware Affinity */
typedef enum {
    TARGET_AFFINITY_NPU_DRAFT = 0, /* Low-power / Ultra-low latency NPU */
    TARGET_AFFINITY_GPU_VERIFY = 1, /* High-throughput RTX 5070 / AVX-512 */
    TARGET_AFFINITY_CPU_AVX2   = 2  /* Cache-tiled SIMD Fallback */
} HardwareAffinity;

/* DMA Speculative Slot Descriptor */
typedef struct {
    uint32_t slot_id;
    uint32_t seq_id;
    uint32_t draft_tokens[ZCC_MAX_TOKEN_DRAFT];
    uint32_t n_drafts;
    float    acceptance_prob;
    bool     is_verified;
    uint32_t accepted_count;
    uint64_t timestamp_ns;
} DmaSpeculativeSlot;

/* Lock-Free Dual-Head POSIX Shared DMA Ring Buffer */
typedef struct {
    DmaSpeculativeSlot slots[ZCC_DMA_RING_CAPACITY];
    _Atomic uint32_t head;       /* Producer (NPU Draft Generator) */
    _Atomic uint32_t tail;       /* Consumer (GPU Batch Verifier) */
    _Atomic uint64_t total_drafts_produced;
    _Atomic uint64_t total_tokens_accepted;
    _Atomic uint64_t total_rollbacks;
} ZccDmaRingBuffer;

/* Speculative Tree Verification Statistics */
typedef struct {
    double speculative_acceptance_rate;
    double latency_reduction_factor;
    double npu_throughput_tps;
    double gpu_throughput_tps;
    uint64_t dma_pcie_transfers_avoided;
} ZccSpeculativeTelemetry;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (R1 - R5)                                             */
/* ------------------------------------------------------------------------- */

/* R1: Initialize Zero-Copy DMA Ring Buffer */
ZccDmaRingBuffer* zcc_dma_ring_create(void);
void zcc_dma_ring_destroy(ZccDmaRingBuffer *ring);

/* R2: NPU Speculative Draft Producer (Pushes draft tokens without locking) */
bool zcc_npu_push_speculative_draft(ZccDmaRingBuffer *ring, 
                                    const uint32_t *tokens, 
                                    uint32_t n_tokens, 
                                    float confidence);

/* R3: GPU / AVX2 Target Verification Consumer (Verifies draft sequence) */
bool zcc_gpu_verify_speculative_batch(ZccDmaRingBuffer *ring, 
                                      const uint32_t *ground_truth_seq,
                                      uint32_t seq_len,
                                      uint32_t *out_accepted_tokens,
                                      ZccSpeculativeTelemetry *telemetry);

/* R4: Speculative Tree Pruning & KV-Cache Synchronizer */
void zcc_speculative_prune_kv_cache(uint32_t rollback_len, uint64_t *kv_cache_epoch);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_SPECULATIVE_DMA_RING_H */
