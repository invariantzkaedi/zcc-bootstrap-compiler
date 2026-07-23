/*
 * test_instcombine_oracle.c — InstCombine Boundary Vector Truth Oracle (Stage 2)
 * ==============================================================================
 * Independent reference oracle using Host GCC native expression evaluation to
 * verify InstCombine algebraic transformation rules against boundary vectors.
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

/* Host GCC Independent Reference Oracles */
static inline int64_t host_gcc_mul_pow2(int64_t val, int64_t k) { return val * k; }
static inline uint64_t host_gcc_udiv_pow2(uint64_t val, uint64_t k) { return val / k; }
static inline int64_t host_gcc_add_self(int64_t val) { return val + val; }
static inline int64_t host_gcc_bitwise_dist(int64_t val, int64_t c1, int64_t c2) { return (val & c1) | (val & c2); }

/* InstCombine Rule Implementations */
static inline int64_t rule_mul_pow2_to_shl(int64_t val, int64_t k) {
    int shift_amt = 0;
    int64_t tmp = k;
    while ((tmp >> (shift_amt + 1)) != 0) shift_amt++;
#if defined(FAULT_INJECT_INSTCOMBINE)
    /* Corrupted shift rule: off-by-one shift amount */
    shift_amt += 1;
#endif
    return val << shift_amt;
}

static inline uint64_t rule_udiv_pow2_to_shr(uint64_t val, uint64_t k) {
    int shift_amt = 0;
    uint64_t tmp = k;
    while ((tmp >> (shift_amt + 1)) != 0) shift_amt++;
    return val >> shift_amt;
}

static inline int64_t rule_add_self_to_shl(int64_t val) {
    return val << 1;
}

static inline int64_t rule_bitwise_distributivity(int64_t val, int64_t c1, int64_t c2) {
    return val & (c1 | c2);
}

int main(void) {
    printf("=== INSTCOMBINE ORACLE: INITIALIZING HOST GCC REFERENCE TRUTH ORACLE ===\n");

#if defined(TEST_BASELINE_BUILD)
    printf("InstCombine Oracle VACUOUS: Zero rules registered on baseline harness.\n");
    printf("VERDICT: ORACLE-SUSPECT (EXIT 2 AS EXPECTED)\n");
    return 2;
#endif

    int n_vecs = sizeof(boundary_vectors) / sizeof(boundary_vectors[0]);
    int rules_tested = 0;

    for (int i = 0; i < n_vecs; i++) {
        int64_t val = boundary_vectors[i];

        /* Rule 1: x * 2^k -> x << k */
        for (int k_pow = 1; k_pow <= 8; k_pow++) {
            int64_t k = (1LL << k_pow);
            int64_t ref_val = host_gcc_mul_pow2(val, k);
            int64_t opt_val = rule_mul_pow2_to_shl(val, k);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: mul_pow2 mismatch on val=0x%llx, k=%lld (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)val, (long long)k, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }

        /* Rule 2: x / 2^k -> x >> k (unsigned) */
        for (int k_pow = 1; k_pow <= 8; k_pow++) {
            uint64_t k = (1ULL << k_pow);
            uint64_t uval = (uint64_t)val;
            uint64_t ref_val = host_gcc_udiv_pow2(uval, k);
            uint64_t opt_val = rule_udiv_pow2_to_shr(uval, k);
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
            int64_t ref_val = host_gcc_add_self(val);
            int64_t opt_val = rule_add_self_to_shl(val);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: add_self mismatch on val=0x%llx (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)val, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }

        /* Rule 4: (x & c1) | (x & c2) -> x & (c1 | c2) */
        {
            int64_t c1 = 0x0F0F0F0F0F0F0F0FLL;
            int64_t c2 = 0x3030303030303030LL;
            int64_t ref_val = host_gcc_bitwise_dist(val, c1, c2);
            int64_t opt_val = rule_bitwise_distributivity(val, c1, c2);
            rules_tested++;

            if (ref_val != opt_val) {
                fprintf(stderr, "InstCombine FAULT DETECTED: bitwise_dist mismatch on val=0x%llx (ref: 0x%llx, opt: 0x%llx)\n",
                        (unsigned long long)val, (unsigned long long)ref_val, (unsigned long long)opt_val);
                printf("VERDICT: INSTCOMBINE FAULT DETECTED (EXIT RED AS EXPECTED)\n");
                return 1;
            }
        }
    }

    if (rules_tested == 0) {
        fprintf(stderr, "InstCombine VACUOUS: Zero rules evaluated.\n");
        printf("VERDICT: ORACLE-SUSPECT (EXIT 2 AS EXPECTED)\n");
        return 2;
    }

    printf("InstCombine PASS: Exact Host GCC reference identity verified across %d boundary vector test pairs.\n", rules_tested);
    printf("VERDICT: INSTCOMBINE TRUTH ORACLE PASS\n");
    return 0;
}
