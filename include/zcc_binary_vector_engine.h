#ifndef ZCC_BINARY_VECTOR_ENGINE_H
#define ZCC_BINARY_VECTOR_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_BVEC_DIM_BITS  512
#define ZCC_BVEC_WORDS_U64 (ZCC_BVEC_DIM_BITS / 64) /* 8 uint64 words */

typedef struct {
    uint64_t words[ZCC_BVEC_WORDS_U64];
    uint32_t concept_id;
    char name[64];
} zcc_bvec512_t;

/* Global Invariant Parity Signature */
extern const uint64_t ZCC_MANIFOLD_PARITY[ZCC_BVEC_WORDS_U64];

/* Sub-nanosecond Popcount & Hamming Distance */
static inline uint32_t zcc_bvec_popcount64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_popcountll(x);
#else
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (uint32_t)((x * 0x0101010101010101ULL) >> 56);
#endif
}

static inline uint32_t zcc_bvec_hamming_dist(const zcc_bvec512_t *a, const zcc_bvec512_t *b) {
    uint32_t dist = 0;
    dist += zcc_bvec_popcount64(a->words[0] ^ b->words[0]);
    dist += zcc_bvec_popcount64(a->words[1] ^ b->words[1]);
    dist += zcc_bvec_popcount64(a->words[2] ^ b->words[2]);
    dist += zcc_bvec_popcount64(a->words[3] ^ b->words[3]);
    dist += zcc_bvec_popcount64(a->words[4] ^ b->words[4]);
    dist += zcc_bvec_popcount64(a->words[5] ^ b->words[5]);
    dist += zcc_bvec_popcount64(a->words[6] ^ b->words[6]);
    dist += zcc_bvec_popcount64(a->words[7] ^ b->words[7]);
    return dist;
}

/* Invariant Phase Lock Verification */
bool zcc_bvec_verify_phase_lock(const zcc_bvec512_t *v);

/* Quantize Float Fourier Vector to 512-bit Binary */
void zcc_bvec_quantize_f64(const double *f64_in, zcc_bvec512_t *bvec_out);

/* Batch Nearest Neighbor Search */
int zcc_bvec_find_nearest(const zcc_bvec512_t *query, const zcc_bvec512_t *corpus, size_t count, uint32_t *best_dist);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_BINARY_VECTOR_ENGINE_H */
