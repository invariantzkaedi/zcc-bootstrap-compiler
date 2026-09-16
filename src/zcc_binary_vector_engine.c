#include "zcc_binary_vector_engine.h"
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

const uint64_t ZCC_MANIFOLD_PARITY[ZCC_BVEC_WORDS_U64] = {
    0xFBA168AEF8848613ULL,
    0x74FE44ABE1881BDFULL,
    0x90E8EFD8DDB2943FULL,
    0x53A37B861B756E62ULL,
    0x776563EEA0CBD236ULL,
    0x4397286617732B76ULL,
    0x20A47A112018A47EULL,
    0x134D7F5155C28302ULL
};


/* Invariant positive bit indices (must be 1) */
static const uint16_t POS_INVARIANTS[] = {
    0, 43, 56, 149, 171, 179, 377, 389, 404, 428, 429, 470, 484
};
#define POS_INVARIANT_COUNT (sizeof(POS_INVARIANTS) / sizeof(POS_INVARIANTS[0]))

/* Invariant negative bit indices (must be 0) */
static const uint16_t NEG_INVARIANTS[] = {
    12, 21, 58, 84, 121, 123, 146, 161, 180, 189, 196, 222, 227, 262, 298, 303, 311, 360, 431, 487
};
#define NEG_INVARIANT_COUNT (sizeof(NEG_INVARIANTS) / sizeof(NEG_INVARIANTS[0]))

bool zcc_bvec_verify_phase_lock(const zcc_bvec512_t *v) {
    if (!v) return false;
    for (size_t i = 0; i < POS_INVARIANT_COUNT; i++) {
        uint16_t bit_idx = POS_INVARIANTS[i];
        uint32_t word_idx = bit_idx / 64;
        uint32_t bit_pos = bit_idx % 64;
        if (!((v->words[word_idx] >> bit_pos) & 1ULL)) {
            return false;
        }
    }
    for (size_t i = 0; i < NEG_INVARIANT_COUNT; i++) {
        uint16_t bit_idx = NEG_INVARIANTS[i];
        uint32_t word_idx = bit_idx / 64;
        uint32_t bit_pos = bit_idx % 64;
        if ((v->words[word_idx] >> bit_pos) & 1ULL) {
            return false;
        }
    }
    return true;
}

void zcc_bvec_quantize_f64(const double *f64_in, zcc_bvec512_t *bvec_out) {
    if (!f64_in || !bvec_out) return;
    memset(bvec_out->words, 0, sizeof(bvec_out->words));
    for (size_t w = 0; w < ZCC_BVEC_WORDS_U64; w++) {
        uint64_t word = 0;
        for (size_t b = 0; b < 64; b++) {
            size_t idx = w * 64 + b;
            if (f64_in[idx] >= 0.0) {
                word |= (1ULL << b);
            }
        }
        bvec_out->words[w] = word;
    }
}

/* AVX2 Fused 512-bit Hamming Distance Operator */
static inline uint32_t zcc_bvec_hamming_avx2(const zcc_bvec512_t *a, const zcc_bvec512_t *b) {
#if defined(__AVX2__)
    __m256i a0 = _mm256_loadu_si256((const __m256i*)&a->words[0]);
    __m256i b0 = _mm256_loadu_si256((const __m256i*)&b->words[0]);
    __m256i x0 = _mm256_xor_si256(a0, b0);

    __m256i a1 = _mm256_loadu_si256((const __m256i*)&a->words[4]);
    __m256i b1 = _mm256_loadu_si256((const __m256i*)&b->words[4]);
    __m256i x1 = _mm256_xor_si256(a1, b1);

    uint64_t r[8];
    _mm256_storeu_si256((__m256i*)&r[0], x0);
    _mm256_storeu_si256((__m256i*)&r[4], x1);

    return (uint32_t)(
        __builtin_popcountll(r[0]) + __builtin_popcountll(r[1]) +
        __builtin_popcountll(r[2]) + __builtin_popcountll(r[3]) +
        __builtin_popcountll(r[4]) + __builtin_popcountll(r[5]) +
        __builtin_popcountll(r[6]) + __builtin_popcountll(r[7])
    );
#else
    return zcc_bvec_hamming_dist(a, b);
#endif
}

int zcc_bvec_find_nearest(const zcc_bvec512_t *query, const zcc_bvec512_t *corpus, size_t count, uint32_t *best_dist) {
    if (!query || !corpus || count == 0) return -1;
    int best_idx = -1;
    uint32_t min_d = 999999;
    for (size_t i = 0; i < count; i++) {
        uint32_t d = zcc_bvec_hamming_avx2(query, &corpus[i]);
        if (d < min_d) {
            min_d = d;
            best_idx = (int)i;
        }
    }
    if (best_dist) *best_dist = min_d;
    return best_idx;
}
