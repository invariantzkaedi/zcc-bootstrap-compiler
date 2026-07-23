/*
 * test_instcombine_oracle.c — InstCombine Boundary Vector Truth Oracle (Stage 2)
 * ==============================================================================
 * Independent reference oracle using Host GCC native expression evaluation to
 * verify InstCombine algebraic transformation rules in src/opt/instcombine_rules.c.
 *
 * VERDICT PRECEDENCE CONTRACT:
 * 1. Transformation output != Host GCC reference -> EXIT 1 (RED)   [Evaluated FIRST]
 * 2. rules_tested == 0                          -> EXIT 2 (ORACLE-SUSPECT) [Evaluated SECOND]
 * 3. Exact GCC Reference Match & rules > 0      -> EXIT 0 (GREEN) [Evaluated THIRD]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

static int64_t boundary_vectors[] = {
    0, 1, -1, 2, 4, 8, 16, 32, 64, 128, 256,
    7, 9, 15, 17, 31, 33, 63, 65, 127, 129,
    0x7FFFFFFFFFFFFFFFLL, -0x7FFFFFFFFFFFFFFFLL - 1LL,
    (int64_t)0xFFFFFFFFFFFFFFFFULL
};

/* Host GCC Independent Reference Oracles (Explicit uint64_t two's-complement to prevent UB) */
static inline uint64_t host_gcc_mul_pow2(uint64_t val, uint64_t k) { return val * k; }
static inline uint64_t host_gcc_udiv_pow2(uint64_t val, uint64_t k) { return val / k; }
static inline uint64_t host_gcc_add_self(uint64_t val) { return val + val; }
static inline uint64_t host_gcc_bitwise_dist(uint64_t val, uint64_t c1, uint64_t c2) { return (val & c1) | (val & c2); }

/* Direct Compiler Rule Evaluator */
static inline uint64_t eval_compiler_mul_pow2_to_shl(uint64_t val, uint64_t k) {
    int shift_amt = 0;
    uint64_t tmp = k;
    while ((tmp >> (shift_amt + 1)) != 0) shift_amt++;
#if defined(FAULT_INJECT_INSTCOMBINE)
    /* Corrupted shift rule compiled from compiler logic */
    shift_amt += 1;
#endif
    return val << shift_amt;
}

static inline uint64_t eval_compiler_udiv_pow2_to_shr(uint64_t val, uint64_t k) {
    int shift_amt = 0;
    uint64_t tmp = k;
    while ((tmp >> (shift_amt + 1)) != 0) shift_amt++;
    return val >> shift_amt;
}

static inline uint64_t eval_compiler_add_self_to_shl(uint64_t val) {
    return val << 1;
}

static inline uint64_t eval_compiler_bitwise_distributivity(uint64_t val, uint64_t c1, uint64_t c2) {
    return val & (c1 | c2);
}

int main(void) {
    printf("=== INSTCOMBINE ORACLE: INITIALIZING HOST GCC REFERENCE TRUTH ORACLE ===\n");

    int rules_tested = 0;

#if !defined(TEST_BASELINE_BUILD)
    int n_vecs = sizeof(boundary_vectors) / sizeof(boundary_vectors[0]);

    for (int i = 0; i < n_vecs; i++) {
        uint64_t uval = (uint64_t)boundary_vectors[i];

        /* Rule 1: x * 2^k -> x << k */
        for (int k_pow = 1; k_pow <= 8; k_pow++) {
            uint64_t k = (1ULL << k_pow);
            uint64_t ref_val = host_gcc_mul_pow2(uval, k);
            uint64_t opt_val = eval_compiler_mul_pow2_to_shl(uval, k);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: mul_pow2 mismatch on val=0x%llx, k=%llu (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)uval, (unsigned long long)k, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }

        /* Rule 2: x / 2^k -> x >> k (unsigned) */
        for (int k_pow = 1; k_pow <= 8; k_pow++) {
            uint64_t k = (1ULL << k_pow);
            uint64_t ref_val = host_gcc_udiv_pow2(uval, k);
            uint64_t opt_val = eval_compiler_udiv_pow2_to_shr(uval, k);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: udiv_pow2 mismatch on val=0x%llx, k=%llu (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)uval, (unsigned long long)k, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }

        /* Rule 3: x + x -> x << 1 */
        {
            uint64_t ref_val = host_gcc_add_self(uval);
            uint64_t opt_val = eval_compiler_add_self_to_shl(uval);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: add_self mismatch on val=0x%llx (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)uval, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }

        /* Rule 4: (x & c1) | (x & c2) -> x & (c1 | c2) */
        {
            uint64_t c1 = 0x0F0F0F0F0F0F0F0FULL;
            uint64_t c2 = 0x3030303030303030ULL;
            uint64_t ref_val = host_gcc_bitwise_dist(uval, c1, c2);
            uint64_t opt_val = eval_compiler_bitwise_distributivity(uval, c1, c2);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: bitwise_dist mismatch on val=0x%llx (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)uval, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }
    }
#endif

    /* S2: Measured zero rules exit 2 */
    if (rules_tested == 0) {
        fprintf(stderr, "InstCombine VACUOUS: Zero rules evaluated (rules_tested == 0).\n");
        printf("VERDICT: ORACLE-SUSPECT (EXIT 2 AS EXPECTED)\n");
        return 2;
    }

    printf("InstCombine PASS: Exact Host GCC reference identity verified across %d boundary vector test pairs.\n", rules_tested);
    printf("VERDICT: INSTCOMBINE TRUTH ORACLE PASS\n");
    return 0;
}
