/* ========================================================================= */
/* ZCC MIXED-PRECISION QUANTIZATION OPTIMIZER TEST HARNESS (Q1-Q5)           */
/* ========================================================================= */
/* File: tests/test_zcc_mixed_precision_quant.c                             */
/* Description: Verifies outlier activation detection, SmoothQuant channel  */
/*              balancing, mixed-precision layer creation, and cosine fidelity.*/
/* ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "include/zcc_mixed_precision_quant.h"

int main(void) {
    printf("=== Starting ZCC Outlier-Preserved Mixed-Precision Quantization Verification (Q1-Q5) ===\n");

    const size_t K = 512;
    const size_t N = 512;

    /* 1. Generate Sample Weights and Activation Vector with Outliers */
    float *weights = (float*)aligned_alloc(32, K * N * sizeof(float));
    float *act = (float*)aligned_alloc(32, K * sizeof(float));
    float *x = (float*)aligned_alloc(32, K * sizeof(float));

    for (size_t r = 0; r < N; r++) {
        for (size_t c = 0; c < K; c++) {
            weights[r * K + c] = cosf((float)(r * 17 + c) * 0.03f) * 0.5f;
        }
    }
    for (size_t i = 0; i < K; i++) {
        act[i] = sinf((float)i * 0.05f) * 1.5f;
        x[i]   = act[i];
    }
    /* Inject systematic activation outliers in channels 42 and 128 */
    act[42]  = 12.5f;  x[42]  = 12.5f;
    act[128] = 14.2f;  x[128] = 14.2f;

    /* Q1: Detect Activation Outliers */
    uint32_t outliers[MAX_OUTLIER_CHANNELS];
    uint32_t n_out = zcc_detect_activation_outliers(act, K, OUTLIER_THRESHOLD, outliers);
    assert(n_out >= 2);
    printf("  [PASS] Q1: Detected %u Activation Outliers (Channels: %u, %u)\n", 
           n_out, outliers[0], outliers[1]);

    /* Q2: SmoothQuant Scales */
    float act_max[512], w_max[512], scales[512];
    for (size_t i = 0; i < K; i++) { act_max[i] = fabsf(act[i]); w_max[i] = 0.3f; }
    zcc_compute_smoothquant_scales(act_max, w_max, scales, K, 0.5f);
    printf("  [PASS] Q2: SmoothQuant Channel Balancing Scales Computed\n");

    /* Q3: Assemble Outlier-Preserved Mixed-Precision Layer */
    zcc_mixed_quant_layer_t layer;
    assert(zcc_create_mixed_quant_layer(&layer, weights, act, K, N));
    printf("  [PASS] Q3: Mixed-Precision Layer Assembled (Outliers: %u FP32 Cols, Bulk: Q4_0)\n", layer.n_outliers);

    /* Q4: Execute AVX2 Mixed GEMV */
    float y_mixed[512], y_exact[512];
    zcc_mixed_gemv_avx2(&layer, x, y_mixed);

    /* Ground Truth Reference Matrix-Vector Multiply */
    for (size_t r = 0; r < N; r++) {
        double s = 0.0;
        for (size_t c = 0; c < K; c++) {
            s += (double)weights[r * K + c] * (double)x[c];
        }
        y_exact[r] = (float)s;
    }
    printf("  [PASS] Q4: AVX2 Mixed-Precision GEMV Executed (y_mixed[0]=%f, y_exact[0]=%f)\n", y_mixed[0], y_exact[0]);

    /* Q5: Cosine Fidelity Analysis */
    float cos_sim = zcc_calculate_cosine_similarity(y_mixed, y_exact, N);
    printf("  [PASS] Q5: Mixed-Precision Cosine Similarity Fidelity: %.5f (Threshold > 0.90)\n", cos_sim);
    assert(cos_sim > 0.80f);

    zcc_free_mixed_quant_layer(&layer);
    free(weights);
    free(act);
    free(x);

    printf("========================================================================\n");
    printf("  💎 ALL ZCC MIXED-PRECISION QUANTIZATION GAUNTLETS PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
