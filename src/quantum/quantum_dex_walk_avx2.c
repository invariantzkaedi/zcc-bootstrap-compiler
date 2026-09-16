#include "quantum_dex_walk_avx2.h"
#include <immintrin.h>
#include <math.h>
#include <string.h>

/* Model weights loaded directly from trained_dex_weights.json */
const qdex_model_t DEFAULT_DEX_MODEL = {
    .weights = {
        4.32459f,  -0.58874f, -0.02085f,  4.60873f,
        -0.00034f,  1.72457f,  0.0f,       0.0f,
        0.0f,       0.0f,      0.0f,       0.0f,
        -0.00009f, -0.00018f,  0.0f,      -3.83465f
    },
    .r2_score = 0.9976f,
    .mse = 0.245432f,
    .l2_reg = 0.001f
};

qdex_status_t qdex_init_state(quantum_dex_state_t *state, int initial_node) {
    if (!state) return QDEX_ERR_NULL_PTR;

    memset(state, 0, sizeof(quantum_dex_state_t));

    if (initial_node >= 0 && initial_node < QDEX_NODES) {
        /* Localized delta wave packet at initial_node */
        state->re[initial_node] = 1.0f;
        state->im[initial_node] = 0.0f;
        state->prob[initial_node] = 1.0f;
        state->total_norm = 1.0f;
    } else {
        /* Equal superposition across all 16 nodes */
        float inv_sqrt16 = 0.25f; /* 1 / sqrt(16) */
        for (int i = 0; i < QDEX_NODES; i++) {
            state->re[i] = inv_sqrt16;
            state->im[i] = 0.0f;
            state->prob[i] = inv_sqrt16 * inv_sqrt16;
        }
        state->total_norm = 1.0f;
    }
    state->step = 0;
    return QDEX_OK;
}

float qdex_predict_scalar(const float *features, const qdex_model_t *model) {
    if (!features || !model) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < QDEX_FEATURES; i++) {
        sum += features[i] * model->weights[i];
    }
    return sum;
}

float qdex_predict_avx2(const float *features, const qdex_model_t *model) {
    if (!features || !model) return 0.0f;

    /* Load 16 features */
    __m256 feat0 = _mm256_loadu_ps(&features[0]);
    __m256 feat1 = _mm256_loadu_ps(&features[8]);

    /* Load 16 weights */
    __m256 w0 = _mm256_loadu_ps(&model->weights[0]);
    __m256 w1 = _mm256_loadu_ps(&model->weights[8]);

    /* Fused Multiply-Add */
    __m256 prod0 = _mm256_mul_ps(feat0, w0);
    __m256 dot   = _mm256_fmadd_ps(feat1, w1, prod0);

    /* Horizontal reduction across YMM register */
    __m128 lo = _mm256_castps256_ps128(dot);
    __m128 hi = _mm256_extractf128_ps(dot, 1);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);

    return _mm_cvtss_f32(sum128);
}

qdex_status_t qdex_predict_batch_avx2(const float *features_matrix,
                                      const qdex_model_t *model,
                                      float *out_predictions,
                                      size_t n_pools) {
    if (!features_matrix || !model || !out_predictions) return QDEX_ERR_NULL_PTR;

    const __m256 w0 = _mm256_loadu_ps(&model->weights[0]);
    const __m256 w1 = _mm256_loadu_ps(&model->weights[8]);

    size_t p = 0;
    /* 4-Way SIMD Unrolled Super-Vector Loop */
    for (; p + 3 < n_pools; p += 4) {
        const float *f0 = &features_matrix[(p + 0) * QDEX_FEATURES];
        const float *f1 = &features_matrix[(p + 1) * QDEX_FEATURES];
        const float *f2 = &features_matrix[(p + 2) * QDEX_FEATURES];
        const float *f3 = &features_matrix[(p + 3) * QDEX_FEATURES];

        __m256 feat0_0 = _mm256_loadu_ps(&f0[0]);
        __m256 feat1_0 = _mm256_loadu_ps(&f0[8]);
        __m256 feat0_1 = _mm256_loadu_ps(&f1[0]);
        __m256 feat1_1 = _mm256_loadu_ps(&f1[8]);
        __m256 feat0_2 = _mm256_loadu_ps(&f2[0]);
        __m256 feat1_2 = _mm256_loadu_ps(&f2[8]);
        __m256 feat0_3 = _mm256_loadu_ps(&f3[0]);
        __m256 feat1_3 = _mm256_loadu_ps(&f3[8]);

        /* Interleaved Dual-FMA Pipe Execution */
        __m256 dot0 = _mm256_fmadd_ps(feat1_0, w1, _mm256_mul_ps(feat0_0, w0));
        __m256 dot1 = _mm256_fmadd_ps(feat1_1, w1, _mm256_mul_ps(feat0_1, w0));
        __m256 dot2 = _mm256_fmadd_ps(feat1_2, w1, _mm256_mul_ps(feat0_2, w0));
        __m256 dot3 = _mm256_fmadd_ps(feat1_3, w1, _mm256_mul_ps(feat0_3, w0));

        __m128 sum0 = _mm_add_ps(_mm256_castps256_ps128(dot0), _mm256_extractf128_ps(dot0, 1));
        __m128 sum1 = _mm_add_ps(_mm256_castps256_ps128(dot1), _mm256_extractf128_ps(dot1, 1));
        __m128 sum2 = _mm_add_ps(_mm256_castps256_ps128(dot2), _mm256_extractf128_ps(dot2, 1));
        __m128 sum3 = _mm_add_ps(_mm256_castps256_ps128(dot3), _mm256_extractf128_ps(dot3, 1));

        __m128 hsum01 = _mm_hadd_ps(sum0, sum1);
        __m128 hsum23 = _mm_hadd_ps(sum2, sum3);
        __m128 final4 = _mm_hadd_ps(hsum01, hsum23);

        _mm_storeu_ps(&out_predictions[p], final4);
    }

    /* Scalar Tail Cleanup */
    for (; p < n_pools; p++) {
        const float *feat = &features_matrix[p * QDEX_FEATURES];
        __m256 feat0 = _mm256_loadu_ps(&feat[0]);
        __m256 feat1 = _mm256_loadu_ps(&feat[8]);

        __m256 prod0 = _mm256_mul_ps(feat0, w0);
        __m256 dot   = _mm256_fmadd_ps(feat1, w1, prod0);

        __m128 lo = _mm256_castps256_ps128(dot);
        __m128 hi = _mm256_extractf128_ps(dot, 1);
        __m128 sum128 = _mm_add_ps(lo, hi);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);

        out_predictions[p] = _mm_cvtss_f32(sum128);
    }
    return QDEX_OK;
}

qdex_status_t qdex_walk_step_scalar(quantum_dex_state_t *state,
                                   const float *features,
                                   const qdex_model_t *model,
                                   float dt) {
    if (!state || !features || !model) return QDEX_ERR_NULL_PTR;

    float next_re[QDEX_NODES];
    float next_im[QDEX_NODES];
    float total_norm = 0.0f;

    /* 1. Kinetic hopping across 16-node ring with periodic boundary */
    for (int i = 0; i < QDEX_NODES; i++) {
        int left  = (i - 1 + QDEX_NODES) % QDEX_NODES;
        int right = (i + 1) % QDEX_NODES;

        float hop_re = 0.5f * (state->re[left] + state->re[right]);
        float hop_im = 0.5f * (state->im[left] + state->im[right]);

        /* 2. On-site DEX potential V_i = w_i * x_i */
        float V_i = model->weights[i] * features[i];

        /* Phase rotation angle theta = (V_i + 1.0) * dt */
        float theta = (V_i + 1.0f) * dt;
        float cos_th = cosf(theta);
        float sin_th = sinf(theta);

        /* Unitary evolution: exp(-i * theta) * (hop) */
        next_re[i] = hop_re * cos_th + hop_im * sin_th;
        next_im[i] = hop_im * cos_th - hop_re * sin_th;

        state->prob[i] = next_re[i] * next_re[i] + next_im[i] * next_im[i];
        total_norm += state->prob[i];
    }

    /* Renormalize to ensure exact unitary conservation */
    if (total_norm > 1e-8f) {
        float inv_norm = 1.0f / sqrtf(total_norm);
        for (int i = 0; i < QDEX_NODES; i++) {
            state->re[i] = next_re[i] * inv_norm;
            state->im[i] = next_im[i] * inv_norm;
            state->prob[i] = state->re[i] * state->re[i] + state->im[i] * state->im[i];
        }
        state->total_norm = 1.0f;
    } else {
        return QDEX_ERR_NORM_DRIFT;
    }

    state->step++;
    return QDEX_OK;
}

qdex_status_t qdex_walk_step_avx2(quantum_dex_state_t *state,
                                 const float *features,
                                 const qdex_model_t *model,
                                 float dt) {
    if (!state || !features || !model) return QDEX_ERR_NULL_PTR;

    /* Load current 16-node state */
    __m256 re0 = _mm256_loadu_ps(&state->re[0]);
    __m256 re1 = _mm256_loadu_ps(&state->re[8]);
    __m256 im0 = _mm256_loadu_ps(&state->im[0]);
    __m256 im1 = _mm256_loadu_ps(&state->im[8]);

    float left_re[QDEX_NODES], right_re[QDEX_NODES];
    float left_im[QDEX_NODES], right_im[QDEX_NODES];

    for (int i = 0; i < QDEX_NODES; i++) {
        int l = (i - 1 + QDEX_NODES) % QDEX_NODES;
        int r = (i + 1) % QDEX_NODES;
        left_re[i] = state->re[l];
        right_re[i] = state->re[r];
        left_im[i] = state->im[l];
        right_im[i] = state->im[r];
    }

    __m256 hop_re0 = _mm256_mul_ps(_mm256_add_ps(_mm256_loadu_ps(&left_re[0]), _mm256_loadu_ps(&right_re[0])), _mm256_set1_ps(0.5f));
    __m256 hop_re1 = _mm256_mul_ps(_mm256_add_ps(_mm256_loadu_ps(&left_re[8]), _mm256_loadu_ps(&right_re[8])), _mm256_set1_ps(0.5f));
    __m256 hop_im0 = _mm256_mul_ps(_mm256_add_ps(_mm256_loadu_ps(&left_im[0]), _mm256_loadu_ps(&right_im[0])), _mm256_set1_ps(0.5f));
    __m256 hop_im1 = _mm256_mul_ps(_mm256_add_ps(_mm256_loadu_ps(&left_im[8]), _mm256_loadu_ps(&right_im[8])), _mm256_set1_ps(0.5f));

    /* Compute V_i = w_i * x_i */
    __m256 w0 = _mm256_loadu_ps(&model->weights[0]);
    __m256 w1 = _mm256_loadu_ps(&model->weights[8]);
    __m256 f0 = _mm256_loadu_ps(&features[0]);
    __m256 f1 = _mm256_loadu_ps(&features[8]);

    __m256 v0 = _mm256_mul_ps(w0, f0);
    __m256 v1 = _mm256_mul_ps(w1, f1);

    /* Angle theta = (V_i + 1.0) * dt */
    __m256 dt_vec = _mm256_set1_ps(dt);
    __m256 one_vec = _mm256_set1_ps(1.0f);
    __m256 th0 = _mm256_mul_ps(_mm256_add_ps(v0, one_vec), dt_vec);
    __m256 th1 = _mm256_mul_ps(_mm256_add_ps(v1, one_vec), dt_vec);

    float th_arr[QDEX_NODES];
    float cos_arr[QDEX_NODES];
    float sin_arr[QDEX_NODES];
    _mm256_storeu_ps(&th_arr[0], th0);
    _mm256_storeu_ps(&th_arr[8], th1);

    for (int i = 0; i < QDEX_NODES; i++) {
        cos_arr[i] = cosf(th_arr[i]);
        sin_arr[i] = sinf(th_arr[i]);
    }

    __m256 cos0 = _mm256_loadu_ps(&cos_arr[0]);
    __m256 cos1 = _mm256_loadu_ps(&cos_arr[8]);
    __m256 sin0 = _mm256_loadu_ps(&sin_arr[0]);
    __m256 sin1 = _mm256_loadu_ps(&sin_arr[8]);

    __m256 n_re0 = _mm256_fmadd_ps(hop_im0, sin0, _mm256_mul_ps(hop_re0, cos0));
    __m256 n_re1 = _mm256_fmadd_ps(hop_im1, sin1, _mm256_mul_ps(hop_re1, cos1));
    __m256 n_im0 = _mm256_fmsub_ps(hop_im0, cos0, _mm256_mul_ps(hop_re0, sin0));
    __m256 n_im1 = _mm256_fmsub_ps(hop_im1, cos1, _mm256_mul_ps(hop_re1, sin1));

    __m256 p0 = _mm256_fmadd_ps(n_re0, n_re0, _mm256_mul_ps(n_im0, n_im0));
    __m256 p1 = _mm256_fmadd_ps(n_re1, n_re1, _mm256_mul_ps(n_im1, n_im1));

    __m256 p_sum = _mm256_add_ps(p0, p1);
    __m128 lo = _mm256_castps256_ps128(p_sum);
    __m128 hi = _mm256_extractf128_ps(p_sum, 1);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float total_norm = _mm_cvtss_f32(sum128);

    if (total_norm > 1e-8f) {
        /* AVX2 Reciprocal Square-Root with single Newton-Raphson FMA refinement step */
        __m128 norm_ss = _mm_set_ss(total_norm);
        __m128 rsqrt0  = _mm_rsqrt_ss(norm_ss);
        /* x1 = x0 * (1.5 - 0.5 * a * x0 * x0) via FMA */
        __m128 three_halfs = _mm_set_ss(1.5f);
        __m128 half_norm   = _mm_mul_ss(norm_ss, _mm_set_ss(0.5f));
        __m128 rsqrt0_sq   = _mm_mul_ss(rsqrt0, rsqrt0);
        __m128 rsqrt1      = _mm_mul_ss(rsqrt0, _mm_fnmadd_ss(half_norm, rsqrt0_sq, three_halfs));
        float inv_norm = _mm_cvtss_f32(rsqrt1);

        __m256 inv_norm_vec = _mm256_set1_ps(inv_norm);

        _mm256_storeu_ps(&state->re[0], _mm256_mul_ps(n_re0, inv_norm_vec));
        _mm256_storeu_ps(&state->re[8], _mm256_mul_ps(n_re1, inv_norm_vec));
        _mm256_storeu_ps(&state->im[0], _mm256_mul_ps(n_im0, inv_norm_vec));
        _mm256_storeu_ps(&state->im[8], _mm256_mul_ps(n_im1, inv_norm_vec));

        _mm256_storeu_ps(&state->prob[0], _mm256_mul_ps(p0, _mm256_set1_ps(inv_norm * inv_norm)));
        _mm256_storeu_ps(&state->prob[8], _mm256_mul_ps(p1, _mm256_set1_ps(inv_norm * inv_norm)));
        state->total_norm = 1.0f;
    } else {
        return QDEX_ERR_NORM_DRIFT;
    }

    state->step++;
    return QDEX_OK;
}

float qdex_compute_variance(const quantum_dex_state_t *state) {
    if (!state) return 0.0f;

    /* Copy prob array unaligned to ensure no fault on unaligned callers */
    float prob[QDEX_NODES];
    memcpy(prob, state->prob, sizeof(prob));

    float mean = 0.0f;
    for (int i = 0; i < QDEX_NODES; i++) {
        mean += (float)i * prob[i];
    }
    float var = 0.0f;
    for (int i = 0; i < QDEX_NODES; i++) {
        float diff = (float)i - mean;
        var += diff * diff * prob[i];
    }
    return var;
}

int qdex_get_peak_node(const quantum_dex_state_t *state, float *out_max_prob) {
    if (!state) return -1;
    float prob[QDEX_NODES];
    memcpy(prob, state->prob, sizeof(prob));

    int peak_idx = 0;
    float max_p = prob[0];
    for (int i = 1; i < QDEX_NODES; i++) {
        if (prob[i] > max_p) {
            max_p = prob[i];
            peak_idx = i;
        }
    }
    if (out_max_prob) *out_max_prob = max_p;
    return peak_idx;
}

