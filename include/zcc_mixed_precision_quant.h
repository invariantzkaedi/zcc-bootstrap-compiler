/* ========================================================================= */
/* ZCC MIXED-PRECISION QUANTIZATION OPTIMIZER (Q1-Q5)                        */
/* ========================================================================= */
/* File: include/zcc_mixed_precision_quant.h                                */
/* Description: Dynamic Outlier-Preserved Mixed-Precision Quantization       */
/*              (FP16 Outlier Columns + Q4_0 Bulk Weights + SmoothQuant).    */
/* ========================================================================= */

#ifndef ZCC_MIXED_PRECISION_QUANT_H
#define ZCC_MIXED_PRECISION_QUANT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/zcc_ai_tensor_ir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_OUTLIER_CHANNELS  32
#define OUTLIER_THRESHOLD     4.5f

/* Mixed Precision Layer Descriptor */
typedef struct {
    uint32_t         dim_k;
    uint32_t         dim_n;
    uint32_t         n_outliers;
    uint32_t         outlier_indices[MAX_OUTLIER_CHANNELS];
    float           *fp16_outlier_weights;     /* n_outliers x dim_n (Full precision) */
    zcc_block_q4_0_t *q4_bulk_weights;         /* (dim_k - n_outliers) quantized */
    float            smooth_scales[512];       /* SmoothQuant per-channel scaling */
} zcc_mixed_quant_layer_t;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (Q1 - Q5)                                             */
/* ------------------------------------------------------------------------- */

/* Q1: Activation Outlier Detection */
uint32_t zcc_detect_activation_outliers(const float *act, size_t dim, float threshold, uint32_t *out_indices);

/* Q2: SmoothQuant Channel Balancing */
void zcc_compute_smoothquant_scales(const float *act_max, const float *weight_max, float *out_scales, size_t dim, float alpha);

/* Q3: Mixed-Precision Layer Assembly */
bool zcc_create_mixed_quant_layer(zcc_mixed_quant_layer_t *layer, const float *weights, const float *sample_act, size_t k, size_t n);
void zcc_free_mixed_quant_layer(zcc_mixed_quant_layer_t *layer);

/* Q4: AVX2 Mixed-Precision GEMV Execution */
void zcc_mixed_gemv_avx2(const zcc_mixed_quant_layer_t *layer, const float *x, float *y);

/* Q5: Cosine Fidelity & Error Analysis */
float zcc_calculate_cosine_similarity(const float *a, const float *b, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_MIXED_PRECISION_QUANT_H */
