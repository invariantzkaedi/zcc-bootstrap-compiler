#ifndef QUANTUM_DEX_WALK_AVX2_H
#define QUANTUM_DEX_WALK_AVX2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QDEX_ALIGN 64
#define QDEX_NODES 16
#define QDEX_FEATURES 16

/* Status codes */
typedef enum {
    QDEX_OK = 0,
    QDEX_ERR_NULL_PTR = -1,
    QDEX_ERR_ALIGNMENT = -2,
    QDEX_ERR_NON_FINITE = -3,
    QDEX_ERR_NORM_DRIFT = -4
} qdex_status_t;

/* 64-byte aligned 16-node Complex Quantum State */
typedef struct {
    /* 16 complex amplitudes: psi[i] = re[i] + I * im[i] */
    float re[QDEX_NODES] __attribute__((aligned(QDEX_ALIGN)));
    float im[QDEX_NODES] __attribute__((aligned(QDEX_ALIGN)));
    /* Probability density: prob[i] = re[i]^2 + im[i]^2 */
    float prob[QDEX_NODES] __attribute__((aligned(QDEX_ALIGN)));
    /* Current step count */
    uint32_t step;
    /* Conserved total probability (norm squared) */
    float total_norm;
} quantum_dex_state_t;

/* Model parameters and weights */
typedef struct {
    float weights[QDEX_FEATURES] __attribute__((aligned(QDEX_ALIGN)));
    float r2_score;
    float mse;
    float l2_reg;
} qdex_model_t;

/* Global default model loaded from trained_dex_weights.json */
extern const qdex_model_t DEFAULT_DEX_MODEL;

/* Initialize 16-node quantum walk state with localized or uniform wave packet */
qdex_status_t qdex_init_state(quantum_dex_state_t *state, int initial_node);

/* Scalar DEX Profit Predictor: dot product w · x */
float qdex_predict_scalar(const float *features, const qdex_model_t *model);

/* AVX2/FMA Accelerated DEX Profit Predictor: 256-bit SIMD */
float qdex_predict_avx2(const float *features, const qdex_model_t *model);

/* Batch AVX2 DEX Profit Predictor across N pools */
qdex_status_t qdex_predict_batch_avx2(const float *features_matrix,
                                      const qdex_model_t *model,
                                      float *out_predictions,
                                      size_t n_pools);

/* Scalar 1-Step Quantum Walk with On-Site DEX Potential Landscape */
qdex_status_t qdex_walk_step_scalar(quantum_dex_state_t *state,
                                   const float *features,
                                   const qdex_model_t *model,
                                   float dt);

/* AVX2/FMA 1-Step Quantum Walk with On-Site DEX Potential Landscape */
qdex_status_t qdex_walk_step_avx2(quantum_dex_state_t *state,
                                 const float *features,
                                 const qdex_model_t *model,
                                 float dt);

/* Compute Spatial Variance (Spreading Measure) sigma^2(t) */
float qdex_compute_variance(const quantum_dex_state_t *state);

/* Find Top Wave-Packet Resonant Node (Argmax Probability) */
int qdex_get_peak_node(const quantum_dex_state_t *state, float *out_max_prob);

#ifdef __cplusplus
}
#endif

#endif /* QUANTUM_DEX_WALK_AVX2_H */
