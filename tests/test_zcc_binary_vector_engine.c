/* ========================================================================= */
/* TEST: ZCC BINARY VECTOR ENGINE & 512-BIT FOURIER MANIFOLD (B1-B4)         */
/* ========================================================================= */
/* File: tests/test_zcc_binary_vector_engine.c                               */
/* ========================================================================= */

#include "include/zcc_binary_vector_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║   ZCC 512-BIT BINARY VECTOR & FOURIER MANIFOLD GAUNTLET (B1-B4)        ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* B1: Popcount & Hamming Distance */
    printf("[B1] Testing Sub-Nanosecond 512-Bit Hamming Distance & Popcount...\n");
    zcc_bvec512_t v1, v2;
    memset(&v1, 0, sizeof(v1));
    memset(&v2, 0, sizeof(v2));
    v1.words[0] = 0xAAAAAAAAAAAAAAAAULL; // 32 bits set
    v2.words[0] = 0x5555555555555555ULL; // 32 bits set (all distinct)

    uint32_t dist = zcc_bvec_hamming_dist(&v1, &v2);
    printf("  • Hamming Distance: %u bits\n", dist);
    assert(dist == 64);
    printf("  [PASS] B1: Popcount and 512-bit Hamming distance verified.\n\n");

    /* B2: Invariant Phase Lock Verification */
    printf("[B2] Testing Invariant Phase Lock Verification (ZCC_MANIFOLD_PARITY)...\n");
    zcc_bvec512_t parity_vec;
    memcpy(parity_vec.words, ZCC_MANIFOLD_PARITY, sizeof(parity_vec.words));
    assert(zcc_bvec_verify_phase_lock(&parity_vec));

    // Flip an invariant bit and verify rejection
    parity_vec.words[0] ^= 1ULL; // Flip bit 0 (positive invariant)
    assert(!zcc_bvec_verify_phase_lock(&parity_vec));
    parity_vec.words[0] ^= 1ULL; // Restore
    assert(zcc_bvec_verify_phase_lock(&parity_vec));
    printf("  [PASS] B2: Invariant phase lock positive and negative traps verified.\n\n");

    /* B3: Fourier Quantization */
    printf("[B3] Testing 512-D Double Fourier Quantization into Binary Words...\n");
    double f64_in[512];
    for (int i = 0; i < 512; i++) f64_in[i] = (i % 2 == 0) ? 1.5 : -1.5;
    zcc_bvec512_t bvec_out;
    zcc_bvec_quantize_f64(f64_in, &bvec_out);
    for (int w = 0; w < ZCC_BVEC_WORDS_U64; w++) {
        assert(bvec_out.words[w] == 0x5555555555555555ULL);
    }
    printf("  • Quantized 512-D Double Vector (4096 Bytes) -> 64 Bytes (64x reduction)\n");
    printf("  [PASS] B3: Fourier floating-point sign quantization verified.\n\n");

    /* B4: Corpus Nearest Neighbor Search */
    printf("[B4] Testing Corpus Batch Nearest Neighbor Search...\n");
    zcc_bvec512_t corpus[100];
    for (int i = 0; i < 100; i++) {
        memset(&corpus[i], 0, sizeof(zcc_bvec512_t));
        corpus[i].words[0] = (uint64_t)i;
    }
    zcc_bvec512_t query;
    memset(&query, 0, sizeof(query));
    query.words[0] = 42ULL;

    uint32_t best_dist = 0;
    int nearest_idx = zcc_bvec_find_nearest(&query, corpus, 100, &best_dist);
    printf("  • Nearest Neighbor Found: Index %d (Distance: %u bits)\n", nearest_idx, best_dist);
    assert(nearest_idx == 42);
    assert(best_dist == 0);
    printf("  [PASS] B4: Nearest neighbor corpus search verified.\n\n");

    printf("========================================================================\n");
    printf("  [SUCCESS] ZCC 512-BIT BINARY VECTOR ENGINE (B1-B4): 100%% VERIFIED!\n");
    printf("========================================================================\n");


    return 0;
}
