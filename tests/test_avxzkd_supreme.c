#include "avxzkd_supreme.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define TEST_WIDTH  64
#define TEST_HEIGHT 64

int main(void) {
    printf("===================================================================\n");
    printf("        AVXZKD SUPREME v2.5 — FULL C NATIVE TEST GAUNTLET           \n");
    printf("===================================================================\n");

    /* 0. Probe CPU Features */
    uint32_t cpu_flags = avxzkd_get_cpu_features();
    printf("[0] CPU Features Detected: 0x%04X (AVX2: %s, FMA: %s, AVX512F: %s)\n",
           cpu_flags,
           (cpu_flags & AVXZKD_CPU_AVX2) ? "YES" : "NO",
           (cpu_flags & AVXZKD_CPU_FMA) ? "YES" : "NO",
           (cpu_flags & AVXZKD_CPU_AVX512F) ? "YES" : "NO");

    /* 1. Allocate Field */
    avxzkd_field_t *field = avxzkd_create(TEST_WIDTH, TEST_HEIGHT);
    assert(field != NULL);
    printf("[1] Field Allocation: PASSED (Width=%u, Stride=%u, Cells=%u)\n", 
           field->width, field->stride, field->total_cells);

    /* 2. Initialize Lattice with Saturated (+10) and Floor (-10) Zones */
    float *init_buf = (float*)malloc(field->total_cells * sizeof(float));
    for (uint32_t y = 0; y < TEST_HEIGHT; ++y) {
        for (uint32_t x = 0; x < field->width; ++x) {
            uint32_t idx = y * field->width + x;
            if (y < TEST_HEIGHT / 2) {
                init_buf[idx] = 10.0f; /* Saturated regime */
            } else {
                init_buf[idx] = -10.0f; /* Quiescent floor regime */
            }
        }
    }
    assert(avxzkd_init(field, init_buf) == AVXZKD_OK);
    free(init_buf);
    printf("[2] Field Initialization: PASSED\n");

    /* 3. Deep Recursion (K=50 steps, eta=0.4, gamma=0.3) */
    avxzkd_params_t params = {
        .eta = 0.4f,
        .gamma = 0.3f,
        .beta = 0.1f,
        .eps = 0.0f,
        .kick = 2.0f,
        .kappa = 0.0f,
        .momentum = 0.25f,
        .seed = {0x12345678, 0x9abcdef0, 0x13579bdf, 0x2468ace0}
    };

    avxzkd_status_t status = avxzkd_deep_recurse_auto(field, &params, 50);
    assert(status == AVXZKD_OK);
    printf("[3] AVX2/AVX-512 Auto Deep Recursion (K=50 steps): PASSED\n");

    /* 4. Multi-Threaded Parallel Recursion */
    assert(avxzkd_deep_recurse_parallel(field, &params, 20, 4) == AVXZKD_OK);
    printf("[4] OpenMP Parallel Field Recursion (4 threads): PASSED\n");

    /* 5. Invariant Audit (INV-1 Saturated Gain & INV-2 Floor Stability) */
    avxzkd_audit_t audit;
    assert(avxzkd_audit(field, &params, &audit) == AVXZKD_OK);
    printf("[5] Invariant Audit:\n");
    printf("    - Measured Saturated Gain : %.5f (Expected: %.5f, diff=%.5f)\n", 
           audit.measured_gain, (1.0f / (1.0f - params.eta)), fabsf(audit.measured_gain - (1.0f / (1.0f - params.eta))));
    printf("    - Measured Floor Drift    : %.8f (Expected: 0.00000000)\n", audit.floor_drift);
    printf("    - Cryptographic State Digest: 0x%016llX\n", (unsigned long long)audit.state_digest);
    printf("    - Invariant Gates Status  : %s\n", audit.pass_all_invariants ? "ALL GATES PASS" : "GATE FAILED");
    fflush(stdout);
    assert(audit.pass_all_invariants);

    /* 6. Topological Curvature (Laplacian & Hessian Discriminator) */
    assert(avxzkd_compute_topology_avx2(field) == AVXZKD_OK);
    printf("[6] Vectorized Laplacian & Hessian Curvature: PASSED\n");

    /* 7. Two-Regime Walker Navigation */
    avxzkd_walker_t *walker = avxzkd_walker_create(0, 0, TEST_WIDTH - 1, TEST_HEIGHT - 1, 4096);
    assert(walker != NULL);
    int32_t steps = avxzkd_walker_solve(walker, field, &params, 2000);
    printf("[7] Two-Regime Walker:\n");
    printf("    - Solved Target Reached   : %s\n", walker->solved ? "YES" : "NO");
    printf("    - Total Steps Taken       : %d\n", steps);
    printf("    - Pruned Simple Path Len  : %u\n", walker->path_len);
    assert(walker->solved);

    /* 8. Layer 1 Quantum DTQW & Decoherence */
    avxzkd_dtqw_t *qw = avxzkd_dtqw_create();
    assert(qw != NULL);
    assert(avxzkd_dtqw_step_auto(qw, 50) == AVXZKD_OK);
    
    double norm = 0.0;
    for (int i = 0; i < 16; ++i) norm += qw->node_probs[i];
    printf("[8] Layer 1 Quantum DTQW:\n");
    printf("    - 16-Node Hilbert Norm    : %.8f (Conserved: %s)\n", norm, (fabs(norm - 1.0) < 1e-5) ? "YES" : "NO");
    printf("    - Coin Entanglement S(q0) : %.6f bits\n", qw->s_q0);
    printf("    - Initial Coherence       : %.4f\n", qw->coherence);
    assert(fabs(norm - 1.0) < 1e-5);
    assert(qw->s_q0 > 0.0);

    /* Apply Lindblad Decoherence */
    assert(avxzkd_dtqw_dephase(qw, 0.25f) == AVXZKD_OK);
    printf("    - Dephased Coherence      : %.4f (Decayed: %s)\n", qw->coherence, (qw->coherence < 1.0) ? "YES" : "NO");
    assert(qw->coherence < 1.0);

    /* Cleanup */
    avxzkd_dtqw_destroy(qw);
    avxzkd_walker_destroy(walker);
    avxzkd_destroy(field);

    printf("===================================================================\n");
    printf("     VERDICT: AVXZKD SUPREME v2.5 ALL C TESTS 100%% VERIFIED        \n");
    printf("===================================================================\n");
    return 0;
}
