/* ========================================================================= */
/* ZCC MIXED-PRECISION QUANTIZATION OPTIMIZER (Q1-Q5)                        */
/* ========================================================================= */
/* File: src/quantum/zcc_mixed_precision_quant.c                            */
/* Description: Dynamic Outlier-Preserved Mixed-Precision Quantization.      */
/* ========================================================================= */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>

#include "include/zcc_mixed_precision_quant.h"

uint32_t zcc_detect_activation_outliers(const float *act, size_t dim, float threshold, uint32_t *out_indices) {
    uint32_t count = 0;
    for (size_t i = 0; i < dim && count < MAX_OUTLIER_CHANNELS; i++) {
        if (fabsf(act[i]) > threshold) {
            out_indices[count++] = (uint32_t)i;
        }
    }
    return count;
}

void zcc_compute_smoothquant_scales(const float *act_max, const float *weight_max, float *out_scales, size_t dim, float alpha) {
    for (size_t i = 0; i < dim; i++) {
        float a = act_max[i] > 1e-5f ? act_max[i] : 1e-5f;
        float w = weight_max[i] > 1e-5f ? weight_max[i] : 1e-5f;
        out_scales[i] = powf(a, alpha) / powf(w, 1.0f - alpha);
    }
}

bool zcc_create_mixed_quant_layer(zcc_mixed_quant_layer_t *layer, const float *weights, const float *sample_act, size_t k, size_t n) {
    memset(layer, 0, sizeof(*layer));
    layer->dim_k = (uint32_t)k;
    layer->dim_n = (uint32_t)n;

    layer->n_outliers = zcc_detect_activation_outliers(sample_act, k, OUTLIER_THRESHOLD, layer->outlier_indices);

    /* Allocate FP16/FP32 outlier columns */
    if (layer->n_outliers > 0) {
        layer->fp16_outlier_weights = (float*)aligned_alloc(32, layer->n_outliers * n * sizeof(float));
        for (uint32_t oi = 0; oi < layer->n_outliers; oi++) {
            uint32_t col = layer->outlier_indices[oi];
            for (size_t r = 0; r < n; r++) {
                layer->fp16_outlier_weights[oi * n + r] = weights[r * k + col];
            }
        }
    }

    /* Allocate bulk Q4_0 weights with outlier channels set to zero */
    size_t nb = k / Q4_0_BLOCK_SIZE;
    layer->q4_bulk_weights = (zcc_block_q4_0_t*)aligned_alloc(32, n * nb * sizeof(zcc_block_q4_0_t));
    float *row_tmp = (float*)malloc(k * sizeof(float));
    for (size_t r = 0; r < n; r++) {
        memcpy(row_tmp, weights + r * k, k * sizeof(float));
        for (uint32_t oi = 0; oi < layer->n_outliers; oi++) {
            row_tmp[layer->outlier_indices[oi]] = 0.0f;
        }
        zcc_quantize_row_q4_0(row_tmp, layer->q4_bulk_weights + r * nb, k);
    }
    free(row_tmp);

    for (size_t i = 0; i < k && i < 512; i++) layer->smooth_scales[i] = 1.0f;
    return true;
}

void zcc_free_mixed_quant_layer(zcc_mixed_quant_layer_t *layer) {
    if (layer->fp16_outlier_weights) free(layer->fp16_outlier_weights);
    if (layer->q4_bulk_weights) free(layer->q4_bulk_weights);
    memset(layer, 0, sizeof(*layer));
}

void zcc_mixed_gemv_avx2(const zcc_mixed_quant_layer_t *layer, const float *x, float *y) {
    /* 1. Bulk Q4_0 Multiply */
    zcc_gemv_q4_0_avx2(layer->q4_bulk_weights, x, y, layer->dim_n, layer->dim_k);

    /* 2. Add Outlier FP16/FP32 Exact Channels */
    for (uint32_t oi = 0; oi < layer->n_outliers; oi++) {
        uint32_t col = layer->outlier_indices[oi];
        float x_val = x[col];
        const float *outlier_col_w = layer->fp16_outlier_weights + oi * layer->dim_n;
        for (size_t r = 0; r < layer->dim_n; r++) {
            y[r] += x_val * outlier_col_w[r];
        }
    }
}

float zcc_calculate_cosine_similarity(const float *a, const float *b, size_t len) {
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < len; i++) {
        dot += (double)a[i] * (double)b[i];
        norm_a += (double)a[i] * (double)a[i];
        norm_b += (double)b[i] * (double)b[i];
    }
    if (norm_a <= 0.0 || norm_b <= 0.0) return 0.0f;
    return (float)(dot / (sqrt(norm_a) * sqrt(norm_b)));
}
