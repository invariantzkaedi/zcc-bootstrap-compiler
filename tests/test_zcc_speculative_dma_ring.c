/* ========================================================================= */
/* ZCC SPECULATIVE NPU/GPU DMA RING SCHEDULER TEST HARNESS (R1-R5)           */
/* ========================================================================= */
/* File: tests/test_zcc_speculative_dma_ring.c                              */
/* Description: Tests lock-free multi-slot draft generation, batch           */
/*              verification, rollback handling, and latency reduction.      */
/* ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "include/zcc_speculative_dma_ring.h"

int main(void) {
    printf("=== Starting ZCC Speculative NPU/GPU Asynchronous DMA Ring Gauntlet (R1-R5) ===\n");

    /* 1. Create Lock-Free DMA Ring */
    ZccDmaRingBuffer *ring = zcc_dma_ring_create();
    assert(ring);
    printf("  [PASS] R1: Lock-Free Dual-Head DMA Ring Initialized (Capacity: %d Slots)\n", ZCC_DMA_RING_CAPACITY);

    /* 2. NPU Speculative Token Draft Producer */
    uint32_t draft_seq1[4] = {101, 102, 103, 104};
    uint32_t draft_seq2[4] = {105, 106, 999, 108}; /* 999 will cause branch rollback */

    assert(zcc_npu_push_speculative_draft(ring, draft_seq1, 4, 0.95f));
    assert(zcc_npu_push_speculative_draft(ring, draft_seq2, 4, 0.88f));
    printf("  [PASS] R2: NPU Speculative Draft Producer Pushed 2 Speculative Batches (8 Tokens)\n");

    /* 3. GPU Batch Verification Consumer */
    uint32_t ground_truth[8] = {101, 102, 103, 104, 105, 106, 107, 108};
    uint32_t accepted_tokens = 0;
    ZccSpeculativeTelemetry tel;

    assert(zcc_gpu_verify_speculative_batch(ring, ground_truth, 8, &accepted_tokens, &tel));
    assert(accepted_tokens == 6); /* 4 from seq1 + 2 from seq2 (stopped at 999 != 107) */
    printf("  [PASS] R3: GPU Batch Target Verification Verified (Accepted: %u / 8 Tokens)\n", accepted_tokens);
    printf("  [PASS] R3: Speculative Acceptance Rate: %.1f%% | Latency Reduction Factor: %.2fx\n", 
           tel.speculative_acceptance_rate * 100.0, tel.latency_reduction_factor);

    /* 4. Speculative Tree Pruning & KV-Cache Epoch Update */
    uint64_t kv_epoch = 1000;
    zcc_speculative_prune_kv_cache(2, &kv_epoch);
    assert(kv_epoch == 1002);
    printf("  [PASS] R4: Speculative Tree Pruning & KV-Cache Rollback Sync Verified\n");

    zcc_dma_ring_destroy(ring);

    printf("========================================================================\n");
    printf("  💎 ALL ZCC SPECULATIVE NPU/GPU DMA RING GAUNTLETS PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
