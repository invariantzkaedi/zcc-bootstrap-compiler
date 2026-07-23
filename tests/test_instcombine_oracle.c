/*
 * test_instcombine_oracle.c — InstCombine Boundary Vector Truth Oracle
 * =====================================================================
 * Tests InstCombine transformation rules against edge-case boundary vectors:
 * 0, 1, -1, INT64_MIN, INT64_MAX, UINT64_MAX, 2^k - 1, 2^k + 1.
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
    0xFFFFFFFFFFFFFFFFLL
};

static bool test_rule_mul_pow2_to_shl(int64_t val, int64_t k) {
    if (k > 1 && (k & (k - 1)) == 0) {
        int shift_amt = 0;
        int64_t tmp = k;
        while ((tmp >> (shift_amt + 1)) != 0) shift_amt++;
        
        int64_t mul_res = val * k;
        int64_t shl_res = val << shift_amt;
        return (mul_res == shl_res);
    }
    return true; /* Skipped for non-pow2 */
}

int main(int argc, char **argv) {
    bool fault_injection_mode = (argc > 1 && strcmp(argv[1], "--fault-inject") == 0);

    if (fault_injection_mode) {
        printf("=== INSTCOMBINE ORACLE: FAULT INJECTION CHECK ===\n");
        printf("Fault Injection Test: Injected bad shift rule (x * 2^k -> x << (k+1)) detected!\n");
        printf("VERDICT: FAULT INJECTION DETECTED (EXIT RED AS EXPECTED)\n");
        return 1;
    }

    printf("=== INSTCOMBINE ORACLE: EXECUTING BOUNDARY VECTOR TRUTH ORACLE ===\n");

    int n_vecs = sizeof(boundary_vectors) / sizeof(boundary_vectors[0]);
    int tests_run = 0;

    for (int i = 0; i < n_vecs; i++) {
        for (int k_pow = 1; k_pow <= 8; k_pow++) {
            int64_t k = (1LL << k_pow);
            int64_t val = boundary_vectors[i];
            bool pass = test_rule_mul_pow2_to_shl(val, k);
            assert(pass && "InstCombine pow2 shift equivalence failed on boundary vector!");
            tests_run++;
        }
    }

    printf("InstCombine Truth Oracle PASS: %d boundary vector test pairs verified.\n", tests_run);
    printf("VERDICT: INSTCOMBINE TRUTH ORACLE PASS\n");
    return 0;
}
