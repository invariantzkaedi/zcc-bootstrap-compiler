#ifndef ZCAEDI_AVX512_H
#define ZCAEDI_AVX512_H

// Sovereign Vector Header - ZCC Compatible
// Replaces <immintrin.h> for pure ZCC execution

typedef int __m128i __attribute__((vector_size(16)));
typedef int __m512i __attribute__((vector_size(64)));

// 128-bit operations
static inline __m128i _mm_loadu_si128(const __m128i* p) { return *p; }
static inline void _mm_storeu_si128(__m128i* p, __m128i a) { *p = a; }
static inline __m128i _mm_xor_si128(__m128i a, __m128i b) { return a ^ b; }
static inline __m128i _mm_set1_epi32(int x) { return (__m128i){x, x, x, x}; }

static inline __m128i _mm_aesenc_si128(__m128i a, __m128i key) {
    __m128i res = a;
    __asm__ volatile ("aesenc %1, %0" : "+v"(res) : "v"(key));
    return res;
}

static inline __m128i _mm_aesenclast_si128(__m128i a, __m128i key) {
    __m128i res = a;
    __asm__ volatile ("aesenclast %1, %0" : "+v"(res) : "v"(key));
    return res;
}

// 512-bit integer operations
static inline __m512i _mm512_set1_epi32(int x) { 
    return (__m512i){x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x}; 
}
static inline __m512i _mm512_add_epi32(__m512i a, __m512i b) { return a + b; }
static inline __m512i _mm512_and_epi32(__m512i a, __m512i b) { return a & b; }
static inline __m512i _mm512_xor_epi32(__m512i a, __m512i b) { return a ^ b; }
static inline __m512i _mm512_or_epi32(__m512i a, __m512i b) { return a | b; }
static inline __m512i _mm512_andnot_epi32(__m512i a, __m512i b) { return (~a) & b; }
static inline __m512i _mm512_srli_epi32(__m512i a, int imm) { return a >> imm; }
static inline __m512i _mm512_slli_epi32(__m512i a, int imm) { return a << imm; }
static inline void _mm512_storeu_si512(void* p, __m512i a) { *(__m512i*)p = a; }

#endif
