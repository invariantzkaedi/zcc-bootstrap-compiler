#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

static uint64_t d2u(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof(u));
    return u;
}

int main(void) {
    printf("====================================================================\n");
    printf("  HARDWARE AVX2 / AVX-512 SUBNORMAL VECTOR GAUNTLET\n");
    printf("====================================================================\n");

    /* 1. 256-bit AVX2 Vector Load & Add */
    double sn_arr[4] = {5e-324, 1e-308, 2.2250738585072014e-308, -0.0};
    __m256d v_in = _mm256_loadu_pd(sn_arr);
    __m256d v_one = _mm256_set1_pd(1.0);
    __m256d v_add = _mm256_mul_pd(v_in, v_one);

    double res_add[4];
    _mm256_storeu_pd(res_add, v_add);

    int avx2_pass = 1;
    for (int i = 0; i < 4; i++) {
        uint64_t expected = d2u(sn_arr[i]);
        uint64_t actual = d2u(res_add[i]);
        int match = (expected == actual);
        if (!match) avx2_pass = 0;
        printf("[AVX2-ADD] Lane %d: 0x%016lx | %s\n", i, actual, match ? "MATCH" : "MISMATCH");
    }

    /* 2. AVX2 FMA (Fused Multiply-Add): (A * B) + C */
    /* A = 2.0, B = 5e-324, C = 5e-324 -> should yield 3 * 5e-324 (0x0000000000000003) */
    __m256d v_a = _mm256_set1_pd(2.0);
    __m256d v_b = _mm256_set1_pd(5e-324);
    __m256d v_c = _mm256_set1_pd(5e-324);
    __m256d v_fma = _mm256_fmadd_pd(v_a, v_b, v_c);

    double res_fma[4];
    _mm256_storeu_pd(res_fma, v_fma);

    int fma_pass = 1;
    for (int i = 0; i < 4; i++) {
        uint64_t actual = d2u(res_fma[i]);
        uint64_t expected = 0x0000000000000003ULL;
        int match = (actual == expected);
        if (!match) fma_pass = 0;
        printf("[AVX2-FMA] Lane %d: 0x%016lx | %s\n", i, actual, match ? "BIT-EXACT (3e-324)" : "MISMATCH");
    }

    /* 3. 512-bit AVX-512 Vector Execution */
    double sn_arr8[8] = {
        5e-324, 1.0e-323, 1.5e-323, 2.0e-323,
        2.5e-323, 3.0e-323, 3.5e-323, 4.0e-323
    };
    __m512d v512_in = _mm512_loadu_pd(sn_arr8);
    __m512d v512_two = _mm512_set1_pd(2.0);
    __m512d v512_mul = _mm512_mul_pd(v512_in, v512_two);

    double res_512[8];
    _mm512_storeu_pd(res_512, v512_mul);

    int avx512_pass = 1;
    for (int i = 0; i < 8; i++) {
        uint64_t in_u = d2u(sn_arr8[i]);
        uint64_t actual = d2u(res_512[i]);
        uint64_t expected = in_u * 2; /* In subnormal linear region, * 2 doubles integer bits! */
        int match = (actual == expected);
        if (!match) avx512_pass = 0;
        printf("[AVX-512] Lane %d: in=0x%016lx -> out=0x%016lx | %s\n", i, in_u, actual, match ? "LINEAR-DOUBLE PASS" : "MISMATCH");
    }

    /* 4. AVX-512 Horizontal Vector Reduction */
    /* Sum = (1 + 2 + 3 + 4 + 5 + 6 + 7 + 8) * 5e-324 = 36 * 5e-324 = 0x0000000000000024 */
    double red_sum = _mm512_reduce_add_pd(v512_in);
    uint64_t red_actual = d2u(red_sum);
    uint64_t red_expected = 0x0000000000000024ULL;
    int red_pass = (red_actual == red_expected);
    printf("[AVX-512 REDUCE] Sum of 8 lanes: actual=0x%016lx expected=0x%016lx | %s\n",
           red_actual, red_expected, red_pass ? "BIT-EXACT MATCH" : "MISMATCH");

    /* 5. Rule AV-1 Falsifiable Hardware Check: MXCSR FTZ/DAZ Toggle */
    /* When FTZ and DAZ are enabled, hardware treats subnormal inputs and outputs as 0.0 */
    /* Note: Must use memory barrier to prevent GCC -O2 from CSE-hoisting the multiply across _mm_setcsr */
    unsigned int orig_mxcsr = _mm_getcsr();
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    __asm__ volatile("" ::: "memory");

    /* Reload inputs dynamically through volatile pointer to force hardware execution under active MXCSR */
    volatile double *dyn_sn = sn_arr8;
    __m512d v512_ftz_in = _mm512_loadu_pd((const double *)dyn_sn);
    __m512d v512_ftz_two = _mm512_set1_pd(2.0);
    __asm__ volatile("" : "+x"(v512_ftz_in), "+x"(v512_ftz_two) : : "memory");
    __m512d v512_ftz_out = _mm512_mul_pd(v512_ftz_in, v512_ftz_two);
    __asm__ volatile("" : "+x"(v512_ftz_out) : : "memory");

    double res_ftz[8];
    _mm512_storeu_pd(res_ftz, v512_ftz_out);
    _mm_setcsr(orig_mxcsr); /* Restore original IEEE-754 mode */

    int ftz_pass = 1;
    for (int i = 0; i < 8; i++) {
        uint64_t val = d2u(res_ftz[i]);
        if (val != 0) {
            ftz_pass = 0;
            printf("[AV-1 FTZ/DAZ] Lane %d: unexpected non-zero 0x%016lx\n", i, val);
        }
    }
    printf("[AV-1 FALSIFICATION] FTZ+DAZ Mode hardware subnormal flush to 0.0: %s\n",
           ftz_pass ? "CONFIRMED (All 8 lanes flushed to 0x0)" : "FAIL (Hardware did not flush)");

    printf("====================================================================\n");
    int all_pass = avx2_pass && fma_pass && avx512_pass && red_pass && ftz_pass;
    printf("SIMD VERDICT: %s\n", all_pass ? "ALL AVX2 & AVX-512 SUBNORMAL TESTS PASS (BIT-EXACT)" : "FAIL");
    printf("====================================================================\n");
    return all_pass ? 0 : 1;
}

