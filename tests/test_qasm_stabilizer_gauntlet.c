/* ========================================================================= */
/* ZCC BIT-PACKED STABILIZER TABLEAU SIMULATOR GAUNTLET (S1-S5)              */
/* ========================================================================= */
/* S1: Tableau ground state, Bell & GHZ stabilizer generators                */
/* S2: Random Clifford differential cross-validation vs 2^N statevector     */
/* S3: 1,024-qubit GHZ scale, entropy & global measurement collapse          */
/* S4: Quantum teleportation with classically conditioned corrections        */
/* S5: Seeded determinism of random projective measurements                  */
/* ========================================================================= */

#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>

/* Deterministic test-side PRNG (splitmix64, independent of engine seeds) */
static uint64_t test_rng = 0xC0FFEE123456789ULL;
static uint64_t trand(void) {
    uint64_t z = (test_rng += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static ZCCQasmCircuit *parse_or_die(const char *src) {
    char err[512] = {0};
    ZCCQasmCircuit *circ = zcc_qasm_parse_string(src, "gauntlet.qasm", err, sizeof(err));
    if (!circ) {
        fprintf(stderr, "parse failure: %s\n", err);
        exit(1);
    }
    if (!zcc_qasm_validate(circ, err, sizeof(err))) {
        fprintf(stderr, "validate failure: %s\n", err);
        exit(1);
    }
    return circ;
}

/* Statevector <Z_q> expectation from exact amplitudes */
static double sv_expect_z(const ZCCQasmSimulator *sim, size_t q) {
    double e = 0.0;
    size_t bit = (size_t)1 << q;
    size_t i;
    for (i = 0; i < sim->num_amplitudes; i++) {
        double p = sim->amplitudes[i].real * sim->amplitudes[i].real +
                   sim->amplitudes[i].imag * sim->amplitudes[i].imag;
        e += ((i & bit) != 0) ? -p : p;
    }
    return e;
}

int main(void) {
    printf("========================================================================\n");
    printf("  ZCC BIT-PACKED STABILIZER TABLEAU (CHP) SIMULATOR GAUNTLET (S1-S5)\n");
    printf("========================================================================\n\n");

    /* --------------------------------------------------------------------- */
    printf("[S1] Ground State, Bell Pair & GHZ-4 Stabilizer Generators...\n");
    {
        ZCCQasmStabilizer *st = zcc_qasm_stab_create(2, 0, 42);
        assert(st);
        /* |00>: stabilizers +ZI, +IZ; both qubits deterministic +1 */
        assert(zcc_qasm_stab_peek_z(st, 0) == 1);
        assert(zcc_qasm_stab_peek_z(st, 1) == 1);
        assert(zcc_qasm_stab_entropy_1q(st, 0) == 0.0);
        zcc_qasm_stab_free(st);

        ZCCQasmCircuit *bell = parse_or_die(
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[2];\n"
            "h q[0];\n"
            "cx q[0], q[1];\n");
        st = zcc_qasm_stab_create(2, 0, 42);
        assert(st && zcc_qasm_stab_apply_circuit(st, bell));

        /* Bell state: both Z outcomes random, 1 ebit of entanglement */
        assert(zcc_qasm_stab_peek_z(st, 0) == 0);
        assert(zcc_qasm_stab_peek_z(st, 1) == 0);
        assert(zcc_qasm_stab_entropy_1q(st, 0) == 1.0);
        assert(zcc_qasm_stab_entropy_1q(st, 1) == 1.0);

        /* Projective collapse: both qubits must agree */
        int det0 = 0, det1 = 0;
        int m0 = zcc_qasm_stab_measure(st, 0, &det0);
        int m1 = zcc_qasm_stab_measure(st, 1, &det1);
        assert(det0 == 0);  /* First measurement coin-flips */
        assert(det1 == 1);  /* Second is forced by entanglement */
        assert(m0 == m1);
        assert(zcc_qasm_stab_entropy_1q(st, 0) == 0.0);

        char *dump = zcc_qasm_stab_dump_state(st);
        assert(dump && strstr(dump, "S["));
        free(dump);
        zcc_qasm_stab_free(st);
        zcc_qasm_circuit_free(bell);
        printf("  [PASS] S1: Tableau preparation, stabilizers, entropy & collapse verified.\n\n");
    }

    /* --------------------------------------------------------------------- */
    printf("[S2] Random Clifford Differential Cross-Validation vs Statevector...\n");
    {
        const char *g1[] = { "h", "s", "sdg", "x", "y", "z" };
        const char *g2[] = { "cx", "cy", "cz", "swap" };
        int trials = 40;
        int checked_qubits = 0;
        int t;
        for (t = 0; t < trials; t++) {
            size_t n = 2 + (size_t)(trand() % 9); /* 2..10 qubits */
            int gates = 24 + (int)(trand() % 17); /* 24..40 gates */

            char src[8192];
            int len = snprintf(src, sizeof(src),
                               "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[%zu];\n", n);
            int g;
            for (g = 0; g < gates; g++) {
                if (n >= 2 && (trand() & 1)) {
                    size_t a = (size_t)(trand() % n);
                    size_t b = (size_t)(trand() % n);
                    if (a == b) b = (a + 1) % n;
                    len += snprintf(src + len, sizeof(src) - (size_t)len,
                                    "%s q[%zu], q[%zu];\n", g2[trand() % 4], a, b);
                } else {
                    len += snprintf(src + len, sizeof(src) - (size_t)len,
                                    "%s q[%zu];\n", g1[trand() % 6], (size_t)(trand() % n));
                }
            }

            ZCCQasmCircuit *circ = parse_or_die(src);
            assert(zcc_qasm_circuit_is_clifford(circ));

            ZCCQasmStabilizer *st = zcc_qasm_stab_create(n, 0, 1234);
            ZCCQasmSimulator *sv = zcc_qasm_sim_create(n, 0, 1234);
            assert(st && sv);
            assert(zcc_qasm_stab_apply_circuit(st, circ));
            assert(zcc_qasm_sim_apply_circuit(sv, circ));

            size_t q;
            for (q = 0; q < n; q++) {
                int peek = zcc_qasm_stab_peek_z(st, q);
                double ez = sv_expect_z(sv, q);
                if (peek == 0) {
                    assert(fabs(ez) < 1e-9);
                } else {
                    assert(fabs(ez - (double)peek) < 1e-9);
                }

                double se = zcc_qasm_stab_entropy_1q(st, q);
                double ve = zcc_qasm_sim_entropy_1q(sv, q);
                assert(se == 0.0 || se == 1.0);
                assert(fabs(se - ve) < 1e-9);
                checked_qubits++;
            }

            zcc_qasm_stab_free(st);
            zcc_qasm_sim_free(sv);
            zcc_qasm_circuit_free(circ);
        }
        printf("  %d random Clifford circuits, %d qubit observables cross-verified exactly.\n",
               trials, checked_qubits);
        printf("  [PASS] S2: Stabilizer engine agrees with exact 2^N statevector oracle.\n\n");
    }

    /* --------------------------------------------------------------------- */
    printf("[S3] 1,024-Qubit GHZ State: Scale, Entropy & Global Collapse...\n");
    {
        size_t n = 1024;
        ZCCQasmStabilizer *st = zcc_qasm_stab_create(n, 0, 777);
        assert(st);

        clock_t t0 = clock();
        /* Build GHZ-1024 directly through the gate API via a parsed mega-circuit */
        size_t cap = 64 * n + 256;
        char *src = (char *)malloc(cap);
        assert(src);
        int len = snprintf(src, cap, "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[%zu];\nh q[0];\n", n);
        size_t q;
        for (q = 0; q + 1 < n; q++) {
            len += snprintf(src + len, cap - (size_t)len, "cx q[%zu], q[%zu];\n", q, q + 1);
        }
        ZCCQasmCircuit *ghz = parse_or_die(src);
        free(src);

        clock_t t1 = clock();
        assert(zcc_qasm_stab_apply_circuit(st, ghz));
        clock_t t2 = clock();
        double sim_ms = 1000.0 * (double)(t2 - t1) / CLOCKS_PER_SEC;
        printf("  GHZ-%zu applied: %d ops in %.3f ms (parse %.1f ms)\n",
               n, ghz->num_ops, sim_ms,
               1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC);
        assert(sim_ms < 2000.0);

        /* Every qubit: random Z outcome, maximal single-qubit entropy */
        assert(zcc_qasm_stab_peek_z(st, 0) == 0);
        assert(zcc_qasm_stab_peek_z(st, n / 2) == 0);
        assert(zcc_qasm_stab_peek_z(st, n - 1) == 0);
        assert(zcc_qasm_stab_entropy_1q(st, 0) == 1.0);
        assert(zcc_qasm_stab_entropy_1q(st, n - 1) == 1.0);

        /* One projective measurement collapses the entire cat state */
        int det = 0;
        int m0 = zcc_qasm_stab_measure(st, 0, &det);
        assert(det == 0);
        assert(zcc_qasm_stab_peek_z(st, n / 2) == (m0 ? -1 : 1));
        assert(zcc_qasm_stab_peek_z(st, n - 1) == (m0 ? -1 : 1));
        assert(zcc_qasm_stab_entropy_1q(st, n / 2) == 0.0);

        zcc_qasm_stab_free(st);
        zcc_qasm_circuit_free(ghz);
        printf("  [PASS] S3: 1,024-qubit entangled dynamics far beyond the 28-qubit dense limit.\n\n");
    }

    /* --------------------------------------------------------------------- */
    printf("[S4] Quantum Teleportation with Classical Feed-Forward...\n");
    {
        const char *teleport_src =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[3];\n"
            "creg c0[1];\n"
            "creg c1[1];\n"
            "creg c2[1];\n"
            "x q[0];\n"              /* Payload |1> */
            "h q[1];\n"
            "cx q[1], q[2];\n"       /* Bell channel */
            "cx q[0], q[1];\n"
            "h q[0];\n"
            "measure q[0] -> c0[0];\n"
            "measure q[1] -> c1[0];\n"
            "if (c1 == 1) x q[2];\n"
            "if (c0 == 1) z q[2];\n"
            "measure q[2] -> c2[0];\n";

        uint64_t seed;
        for (seed = 1; seed <= 16; seed++) {
            ZCCQasmCircuit *circ = parse_or_die(teleport_src);
            assert(zcc_qasm_circuit_is_clifford(circ));
            ZCCQasmStabilizer *st = zcc_qasm_stab_create(3, 3, seed);
            assert(st && zcc_qasm_stab_apply_circuit(st, circ));
            /* c2 holds the teleported |1> regardless of the random Bell bits */
            assert(st->classical_bits[2] == 1);
            zcc_qasm_stab_free(st);
            zcc_qasm_circuit_free(circ);
        }
        printf("  Teleported |1> across 16 random Bell-measurement branches: all exact.\n");
        printf("  [PASS] S4: Conditional Clifford feed-forward semantics verified.\n\n");
    }

    /* --------------------------------------------------------------------- */
    printf("[S5] Seeded Determinism of Random Projective Measurements...\n");
    {
        const char *src =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[8];\n"
            "creg c[8];\n"
            "h q[0];\ncx q[0], q[1];\ncx q[1], q[2];\n"
            "h q[3];\ns q[3];\nh q[4];\nsdg q[4];\ncz q[3], q[4];\n"
            "h q[5];\nswap q[5], q[6];\ncy q[6], q[7];\n"
            "measure q[0] -> c[0];\nmeasure q[1] -> c[1];\nmeasure q[2] -> c[2];\n"
            "measure q[3] -> c[3];\nmeasure q[4] -> c[4];\nmeasure q[5] -> c[5];\n"
            "measure q[6] -> c[6];\nmeasure q[7] -> c[7];\n";

        unsigned int run_a[8], run_b[8];
        int differs_somewhere = 0;
        int rep;
        for (rep = 0; rep < 2; rep++) {
            unsigned int *dst = rep ? run_b : run_a;
            ZCCQasmCircuit *circ = parse_or_die(src);
            ZCCQasmStabilizer *st = zcc_qasm_stab_create(8, 8, 0xDEADBEEFULL);
            assert(st && zcc_qasm_stab_apply_circuit(st, circ));
            memcpy(dst, st->classical_bits, 8 * sizeof(unsigned int));
            zcc_qasm_stab_free(st);
            zcc_qasm_circuit_free(circ);
        }
        assert(memcmp(run_a, run_b, sizeof(run_a)) == 0);
        /* GHZ correlation inside the register */
        assert(run_a[0] == run_a[1] && run_a[1] == run_a[2]);

        uint64_t s;
        for (s = 1; s <= 32 && !differs_somewhere; s++) {
            ZCCQasmCircuit *circ = parse_or_die(src);
            ZCCQasmStabilizer *st = zcc_qasm_stab_create(8, 8, s);
            assert(st && zcc_qasm_stab_apply_circuit(st, circ));
            if (memcmp(run_a, st->classical_bits, 8 * sizeof(unsigned int)) != 0) {
                differs_somewhere = 1;
            }
            zcc_qasm_stab_free(st);
            zcc_qasm_circuit_free(circ);
        }
        assert(differs_somewhere); /* Outcomes genuinely depend on the seed */
        printf("  Same seed -> identical collapse; varying seeds explore the Born ensemble.\n");
        printf("  [PASS] S5: SplitMix64-seeded projective measurement determinism verified.\n\n");
    }

    printf("========================================================================\n");
    printf("  STABILIZER TABLEAU ENGINE: ALL 5 MILESTONES (S1-S5) 100%% PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
