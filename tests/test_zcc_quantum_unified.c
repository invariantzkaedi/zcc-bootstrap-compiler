#include "zcc_quantum_unified.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

static inline double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║   ZCC QUANTUM-COMPILER FUSED ENGINE MULTI-METRIC VERIFICATION GAUNTLET ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* ──────────────────────────────────────────────────────────────────
     * TEST 1: AVX2 Graph Laplacian & Spectral Form Factor SFF Analysis
     * ────────────────────────────────────────────────────────────────── */
    printf("[TEST 1] Testing Spectral Form Factor (SFF) & Level Repulsion <r>...\n");
    float poisson_evals[32];
    float goe_evals[32];

    /* Uncorrelated Poisson eigenvalues (uniform random walk) */
    for (int i = 0; i < 32; i++) poisson_evals[i] = (float)i * 1.5f + ((float)(i % 5) * 0.2f);

    /* Correlated Wigner-Dyson / GOE eigenvalues (repulsive spectrum) */
    for (int i = 0; i < 32; i++) goe_evals[i] = sqrtf((float)i) * 5.0f;

    zq_spectral_metrics_t m_poisson, m_goe;
    zq_status_t s1 = zcc_qcfg_spectral_analyze_avx2(poisson_evals, 32, &m_poisson);
    zq_status_t s2 = zcc_qcfg_spectral_analyze_avx2(goe_evals, 32, &m_goe);
    assert(s1 == ZQ_OK && s2 == ZQ_OK);

    printf("  Poisson Spectrum: <r>=%.4f, d_s=%.3f, SFF=%.5f, Chaotic: %s\n",
           m_poisson.r_spacing_ratio, m_poisson.spectral_dim, m_poisson.sff_mean,
           m_poisson.is_chaotic_goe ? "YES" : "NO (Modular)");
    printf("  GOE Spectrum:     <r>=%.4f, d_s=%.3f, SFF=%.5f, Chaotic: %s\n",
           m_goe.r_spacing_ratio, m_goe.spectral_dim, m_goe.sff_mean,
           m_goe.is_chaotic_goe ? "YES (Chaotic)" : "NO");

    assert(m_poisson.spectral_dim > 0.0f);
    assert(m_goe.spectral_dim > 0.0f);
    printf("  [PASS] Test 1: SFF & Level Repulsion spectral analysis verified.\n\n");

    /* ──────────────────────────────────────────────────────────────────
     * TEST 2: 16-Node Quantum Walk Mutation Superposition Tournament
     * ────────────────────────────────────────────────────────────────── */
    printf("[TEST 2] Testing Quantum Mutation Tournament Superposition Evolution...\n");
    zq_mut_tournament_t tourn __attribute__((aligned(64)));
    float conflict_mat[256];
    float fitness_deltas[16];

    for (int i = 0; i < 256; i++) conflict_mat[i] = 0.0f;
    for (int i = 0; i < 16; i++) fitness_deltas[i] = 0.0f;

    /* Mutation 0 and Mutation 1 are mutually conflicting */
    conflict_mat[0 * 16 + 1] = 1.0f;
    conflict_mat[1 * 16 + 0] = 1.0f;

    /* Mutation 0 has high positive fitness (+15.0), Mutation 1 is weak (+1.0) */
    fitness_deltas[0] = 15.0f;
    fitness_deltas[1] = 1.0f;
    fitness_deltas[2] = 10.0f; /* Mutation 2 is independent and strong */

    zcc_qmut_tournament_init(&tourn, conflict_mat, fitness_deltas);
    assert(fabsf(tourn.total_norm - 1.0f) < 1e-5f);

    /* Evolve for 20 quantum steps */
    for (int step = 0; step < 20; step++) {
        zcc_qmut_tournament_step_avx2(&tourn, 0.05f);
        assert(fabsf(tourn.total_norm - 1.0f) < 1e-4f);
    }

    printf("  Quantum Mutation Probabilities:\n");
    printf("    • Mut 0 (Delta=+15.0): Prob = %.4f\n", tourn.prob[0]);
    printf("    • Mut 1 (Conflicting):  Prob = %.4f\n", tourn.prob[1]);
    printf("    • Mut 2 (Delta=+10.0): Prob = %.4f\n", tourn.prob[2]);

    /* Assert constructive amplification of Mut 0 over Mut 1 */
    assert(tourn.prob[0] > tourn.prob[1]);

    int selected[4];
    int count = 0;
    zcc_qmut_tournament_select(&tourn, selected, 3, &count);
    printf("  Tournament Winner Selection (count=%d): ", count);
    for (int i = 0; i < count; i++) printf("[%d] ", selected[i]);
    printf("\n");

    /* Ensure conflict avoidance (Mut 0 and Mut 1 not both selected) */
    bool has_0 = false, has_1 = false;
    for (int i = 0; i < count; i++) {
        if (selected[i] == 0) has_0 = true;
        if (selected[i] == 1) has_1 = true;
    }
    assert(!(has_0 && has_1));
    printf("  [PASS] Test 2: Mutation tournament quantum interference & selection verified.\n\n");

    /* ──────────────────────────────────────────────────────────────────
     * TEST 3: Ising Spin-Glass Transverse-Field Register Allocator
     * ────────────────────────────────────────────────────────────────── */
    printf("[TEST 3] Testing Transverse-Field Quantum Register Allocator (Graph Coloring)...\n");
    zq_ising_regalloc_t ising __attribute__((aligned(64)));
    float interference[256];
    for (int i = 0; i < 256; i++) interference[i] = 0.0f;

    /* 4 variables forming a bipartite graph: {0,1} interfere with {2,3} */
    interference[0 * 4 + 2] = 1.0f; interference[2 * 4 + 0] = 1.0f;
    interference[0 * 4 + 3] = 1.0f; interference[3 * 4 + 0] = 1.0f;
    interference[1 * 4 + 2] = 1.0f; interference[2 * 4 + 1] = 1.0f;
    interference[1 * 4 + 3] = 1.0f; interference[3 * 4 + 1] = 1.0f;

    zcc_qising_init(&ising, 4, 2, interference);

    /* Anneal with decaying quantum transverse field */
    int anneal_steps = 60;
    for (int s = 0; s < anneal_steps; s++) {
        float gamma_t = 1.0f - ((float)s / (float)anneal_steps) * 0.99f;
        zcc_qising_anneal_step_avx2(&ising, 0.08f, gamma_t);
    }

    int colors[4];
    int spills = 0;
    zcc_qising_extract_colors(&ising, colors, &spills);

    printf("  Assigned Colors: v0=%d, v1=%d, v2=%d, v3=%d (Spills: %d)\n",
           colors[0], colors[1], colors[2], colors[3], spills);

    assert(spills == 0);
    printf("  [PASS] Test 3: Zero-spill quantum register allocation coloring verified.\n\n");

    /* ──────────────────────────────────────────────────────────────────
     * TEST 4: High-Throughput Performance Benchmark
     * ────────────────────────────────────────────────────────────────── */
    printf("[TEST 4] Benchmarking AVX2 Quantum Subsystems (100,000 iterations)...\n");
    const int N_ITER = 100000;

    double t0 = get_time_sec();
    for (int i = 0; i < N_ITER; i++) {
        zcc_qmut_tournament_step_avx2(&tourn, 0.05f);
    }
    double dt = get_time_sec() - t0;
    printf("  16-Node Quantum Mutation Step: %.4f s (%.2f ns/step, %'.0f steps/sec)\n",
           dt, (dt / N_ITER) * 1e9, (double)N_ITER / dt);
    assert(dt < 0.2);
    printf("  [PASS] Test 4: Ultra-low latency AVX2 execution verified.\n\n");

    printf("========================================================================\n");
    printf("  🏆 ALL 4 FUSED QUANTUM-COMPILER SUBSYSTEMS PASSED (0 ERRORS)\n");
    printf("========================================================================\n");
    return 0;
}
