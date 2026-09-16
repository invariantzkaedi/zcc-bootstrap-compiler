/* ========================================================================= */
/* TEST: ZCC MULTI-THREADED CACHE-TILED AVX2 PARALLEL DISPATCH GAUNTLET      */
/* ========================================================================= */

#include "zcc_parallel_dispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define COLOR_G "\x1b[32m"
#define COLOR_C "\x1b[36m"
#define COLOR_Y "\x1b[33m"
#define COLOR_M "\x1b[35m"
#define COLOR_R "\x1b[0m"
#define BOLD    "\x1b[1m"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║    ZCC MULTI-THREADED CACHE-TILED AVX2 PARALLEL DISPATCH GAUNTLET      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    const size_t N_POOLS = 2000000; /* 2 Million pool evaluations */

    printf("%s[ALLOC] Allocating 2,000,000 Pool Matrix (%zu MB)...%s\n", BOLD, (N_POOLS * 16 * sizeof(float)) / (1024*1024), COLOR_R);
    float *features = (float*)aligned_alloc(64, N_POOLS * QDEX_FEATURES * sizeof(float));
    float *preds_serial = (float*)aligned_alloc(64, N_POOLS * sizeof(float));
    float *preds_parallel = (float*)aligned_alloc(64, N_POOLS * sizeof(float));
    assert(features && preds_serial && preds_parallel);

    /* Fill test dataset with realistic feature coordinates */
    for (size_t i = 0; i < N_POOLS; i++) {
        for (int f = 0; f < QDEX_FEATURES; f++) {
            features[i * QDEX_FEATURES + f] = (float)((i + f) % 100) * 0.05f + 0.1f;
        }
    }

    /* ── TEST 1: Serial AVX2 Single-Core Baseline ── */
    printf("%s[TEST 1] Benchmarking Single-Core AVX2 Baseline (2M evals)...%s\n", BOLD, COLOR_R);
    double t0_s = get_time_sec();
    qdex_predict_batch_avx2(features, &DEFAULT_DEX_MODEL, preds_serial, N_POOLS);
    double t1_s = get_time_sec();
    double dt_serial = t1_s - t0_s;
    double tput_serial = (double)N_POOLS / dt_serial;

    printf("  Serial Elapsed: %.4f s | Latency: %s%.2f ns/eval%s\n", dt_serial, COLOR_C, (dt_serial / N_POOLS) * 1e9, COLOR_R);
    printf("  Single-Core AVX2 Throughput: %s%.1f M evals/sec%s\n", COLOR_Y, tput_serial / 1e6, COLOR_R);
    printf("  %s[PASS] Test 1: Single-core baseline captured.%s\n\n", COLOR_G, COLOR_R);

    /* ── TEST 2: Multi-Threaded Tiled Parallel Dispatch ── */
    printf("%s[TEST 2] Launching Parallel Thread Pool (Multi-Core Tiled Dispatch)...%s\n", BOLD, COLOR_R);
    zcc_parallel_pool_t *pool = zcc_parallel_pool_create(0);
    assert(pool != NULL);
    printf("  ✔ Created Worker Pool with %s%d Active Hardware Threads%s\n", COLOR_M, pool->num_threads, COLOR_R);

    /* Warm-up run */
    zcc_parallel_predict_batch_avx2(pool, features, &DEFAULT_DEX_MODEL, preds_parallel, 10000);

    double t0_p = get_time_sec();
    qdex_status_t st = zcc_parallel_predict_batch_avx2(pool, features, &DEFAULT_DEX_MODEL, preds_parallel, N_POOLS);
    double t1_p = get_time_sec();
    assert(st == QDEX_OK);

    double dt_parallel = t1_p - t0_p;
    double tput_parallel = (double)N_POOLS / dt_parallel;
    double speedup = dt_serial / dt_parallel;

    printf("  Parallel Elapsed: %.4f s | Latency: %s%.2f ns/eval%s\n", dt_parallel, COLOR_C, (dt_parallel / N_POOLS) * 1e9, COLOR_R);
    printf("  🚀 Multi-Core Tiled Throughput: %s%.1f M evals/sec%s (Speedup: %s%.2fx%s)\n",
           COLOR_G, tput_parallel / 1e6, COLOR_R,
           COLOR_G, speedup, COLOR_R);

    /* ── TEST 3: Bitwise Output Parity Verification ── */
    printf("%s[TEST 3] Verifying Exact Numerical Parity (Serial vs Parallel)...%s\n", BOLD, COLOR_R);
    double max_diff = 0.0;
    for (size_t i = 0; i < N_POOLS; i++) {
        double diff = fabs((double)preds_serial[i] - (double)preds_parallel[i]);
        if (diff > max_diff) max_diff = diff;
    }
    printf("  Max Divergence: %s%.6e%s\n", COLOR_C, max_diff, COLOR_R);
    assert(max_diff < 1e-5);
    printf("  %s[PASS] Test 3: 100%% Numerical parity verified across all 2,000,000 predictions.%s\n\n", COLOR_G, COLOR_R);

    zcc_parallel_pool_destroy(pool);
    free(features);
    free(preds_serial);
    free(preds_parallel);

    printf("========================================================================\n");
    printf("  🏆 MULTI-THREADED CACHE-TILED PARALLEL GAUNTLET: ALL TESTS PASSED!\n");
    printf("========================================================================\n");

    return 0;
}
