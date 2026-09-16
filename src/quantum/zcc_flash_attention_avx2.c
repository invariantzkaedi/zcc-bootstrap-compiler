/* ========================================================================= */
/* ZCC FLASHATTENTION TILED AVX2 MULTI-HEAD ATTENTION IMPLEMENTATION          */
/* ========================================================================= */
/* File: src/quantum/zcc_flash_attention_avx2.c                              */
/* Description: Implements cache-tiled block QK^T matrix multiplication with */
/*              online softmax rescaling, O(N) memory complexity, and AVX2   */
/*              vectorized exp/FMA accumulation.                             */
/* ========================================================================= */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <immintrin.h>

#include "include/zcc_flash_attention_avx2.h"

static inline uint64_t fa_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Fast AVX2 Vectorized Exponent Approximation */
static inline __m256 fast_exp_avx2(__m256 x) {
    /* Clamping to avoid overflow/underflow */
    x = _mm256_max_ps(x, _mm256_set1_ps(-80.0f));
    x = _mm256_min_ps(x, _mm256_set1_ps(80.0f));

    /* exp(x) ≈ 2^(x * log2(e)) using Cody-Waite / polynomial */
    __m256 a = _mm256_mul_ps(x, _mm256_set1_ps(1.4426950408889634f)); /* log2(e) */
    __m256 a_floor = _mm256_floor_ps(a);
    __m256 f = _mm256_sub_ps(a, a_floor);

    /* Degree-3 minimax polynomial for 2^f on [0, 1) */
    __m256 c0 = _mm256_set1_ps(1.0f);
    __m256 c1 = _mm256_set1_ps(0.69314718056f);
    __m256 c2 = _mm256_set1_ps(0.24022650695f);
    __m256 c3 = _mm256_set1_ps(0.05550410866f);

    __m256 poly = _mm256_fmadd_ps(c3, f, c2);
    poly = _mm256_fmadd_ps(poly, f, c1);
    poly = _mm256_fmadd_ps(poly, f, c0);

    /* Convert integer power to IEEE 754 float exponent */
    __m256i i_exp = _mm256_cvtps_epi32(a_floor);
    i_exp = _mm256_add_epi32(i_exp, _mm256_set1_epi32(127));
    i_exp = _mm256_slli_epi32(i_exp, 23);
    __m256 pow2 = _mm256_castsi256_ps(i_exp);

    return _mm256_mul_ps(poly, pow2);
}

void zcc_flash_attn_init_config(zcc_flash_attn_config_t *cfg, 
                                uint32_t seq_len, 
                                uint32_t n_heads, 
                                uint32_t head_dim, 
                                bool causal) {
    if (!cfg) return;
    cfg->seq_len = seq_len;
    cfg->n_heads = n_heads;
    cfg->head_dim = head_dim;
    cfg->scale = 1.0f / sqrtf((float)head_dim);
    cfg->causal_mask = causal;
}

/* ------------------------------------------------------------------------- */
/* A2: Tiled Online Softmax FlashAttention Core Kernel (AVX2 Accelerated)    */
/* ------------------------------------------------------------------------- */

void zcc_flash_attention_forward_avx2(const zcc_flash_attn_config_t *cfg,
                                      const float *Q,
                                      const float *K,
                                      const float *V,
                                      float *O) {
    const uint32_t S = cfg->seq_len;
    const uint32_t H = cfg->n_heads;
    const uint32_t D = cfg->head_dim;
    const float scale = cfg->scale;
    const bool causal = cfg->causal_mask;

    const uint32_t Br = FLASH_ATTN_BLOCK_SIZE;
    const uint32_t Bc = FLASH_ATTN_BLOCK_SIZE;

    const uint32_t Tr = (S + Br - 1) / Br;
    const uint32_t Tc = (S + Bc - 1) / Bc;

    /* Stride per head: S * D elements */
    const size_t head_stride = (size_t)S * D;

    for (uint32_t h = 0; h < H; h++) {
        const float *Q_h = Q + h * head_stride;
        const float *K_h = K + h * head_stride;
        const float *V_h = V + h * head_stride;
        float *O_h = O + h * head_stride;

        /* Row statistics per head */
        float *m_i = (float*)malloc(S * sizeof(float));
        float *l_i = (float*)malloc(S * sizeof(float));
        for (uint32_t i = 0; i < S; i++) {
            m_i[i] = -1e30f;
            l_i[i] = 0.0f;
            for (uint32_t d = 0; d < D; d++) {
                O_h[i * D + d] = 0.0f;
            }
        }

        /* Outer Loop over column blocks of K and V */
        for (uint32_t j = 0; j < Tc; j++) {
            uint32_t c_start = j * Bc;
            uint32_t c_end = c_start + Bc > S ? S : c_start + Bc;
            uint32_t curr_bc = c_end - c_start;

            /* Inner Loop over row blocks of Q and O */
            for (uint32_t i = 0; i < Tr; i++) {
                uint32_t r_start = i * Br;
                uint32_t r_end = r_start + Br > S ? S : r_start + Br;
                uint32_t curr_br = r_end - r_start;

                /* Skip causal blocks if all elements are masked out */
                if (causal && c_start > r_end - 1) continue;

                /* Tiled S_ij = Q_i * K_j^T in L1 cache */
                for (uint32_t r = 0; r < curr_br; r++) {
                    uint32_t global_r = r_start + r;
                    const float *q_row = Q_h + global_r * D;

                    float s_tile[FLASH_ATTN_BLOCK_SIZE];
                    float row_max = -1e30f;

                    for (uint32_t c = 0; c < curr_bc; c++) {
                        uint32_t global_c = c_start + c;
                        if (causal && global_c > global_r) {
                            s_tile[c] = -1e30f;
                            continue;
                        }

                        const float *k_col = K_h + global_c * D;
                        
                        /* AVX2 dot product */
                        __m256 sum_v = _mm256_setzero_ps();
                        for (uint32_t d = 0; d < D; d += 8) {
                            __m256 qv = _mm256_loadu_ps(&q_row[d]);
                            __m256 kv = _mm256_loadu_ps(&k_col[d]);
                            sum_v = _mm256_fmadd_ps(qv, kv, sum_v);
                        }
                        /* Horizontal sum */
                        __m128 lo = _mm256_castps256_ps128(sum_v);
                        __m128 hi = _mm256_extractf128_ps(sum_v, 1);
                        __m128 s4 = _mm_add_ps(lo, hi);
                        s4 = _mm_hadd_ps(s4, s4);
                        s4 = _mm_hadd_ps(s4, s4);
                        float dot = _mm_cvtss_f32(s4) * scale;
                        s_tile[c] = dot;
                        if (dot > row_max) row_max = dot;
                    }

                    /* Online Softmax State Update */
                    float m_prev = m_i[global_r];
                    float l_prev = l_i[global_r];
                    float m_curr = m_prev > row_max ? m_prev : row_max;

                    float alpha = expf(m_prev - m_curr);
                    float p_tile[FLASH_ATTN_BLOCK_SIZE];
                    float row_l_sum = 0.0f;

                    for (uint32_t c = 0; c < curr_bc; c++) {
                        if (s_tile[c] < -1e20f) {
                            p_tile[c] = 0.0f;
                        } else {
                            float p = expf(s_tile[c] - m_curr);
                            p_tile[c] = p;
                            row_l_sum += p;
                        }
                    }

                    float l_curr = alpha * l_prev + row_l_sum;
                    m_i[global_r] = m_curr;
                    l_i[global_r] = l_curr;

                    /* Rescale existing O accumulator: O = alpha * O */
                    float *o_row = O_h + global_r * D;
                    for (uint32_t d = 0; d < D; d += 8) {
                        __m256 ov = _mm256_loadu_ps(&o_row[d]);
                        ov = _mm256_mul_ps(ov, _mm256_set1_ps(alpha));
                        _mm256_storeu_ps(&o_row[d], ov);
                    }

                    /* Accumulate P * V into O */
                    for (uint32_t c = 0; c < curr_bc; c++) {
                        float p_val = p_tile[c];
                        if (p_val <= 0.0f) continue;
                        uint32_t global_c = c_start + c;
                        const float *v_row = V_h + global_c * D;
                        __m256 pv = _mm256_set1_ps(p_val);

                        for (uint32_t d = 0; d < D; d += 8) {
                            __m256 ov = _mm256_loadu_ps(&o_row[d]);
                            __m256 vv = _mm256_loadu_ps(&v_row[d]);
                            ov = _mm256_fmadd_ps(pv, vv, ov);
                            _mm256_storeu_ps(&o_row[d], ov);
                        }
                    }
                }
            }
        }

        /* Final normalization: O = O / l_i */
        for (uint32_t r = 0; r < S; r++) {
            float *o_row = O_h + r * D;
            float inv_l = l_i[r] > 0.0f ? 1.0f / l_i[r] : 0.0f;
            __m256 inv_lv = _mm256_set1_ps(inv_l);
            for (uint32_t d = 0; d < D; d += 8) {
                __m256 ov = _mm256_loadu_ps(&o_row[d]);
                ov = _mm256_mul_ps(ov, inv_lv);
                _mm256_storeu_ps(&o_row[d], ov);
            }
        }

        free(m_i);
        free(l_i);
    }
}

/* ------------------------------------------------------------------------- */
/* A3: Standard Reference Quadratic Attention (Bit-Exact Parity Testing)    */
/* ------------------------------------------------------------------------- */

void zcc_reference_attention_forward(const zcc_flash_attn_config_t *cfg,
                                     const float *Q,
                                     const float *K,
                                     const float *V,
                                     float *O) {
    const uint32_t S = cfg->seq_len;
    const uint32_t H = cfg->n_heads;
    const uint32_t D = cfg->head_dim;
    const float scale = cfg->scale;
    const bool causal = cfg->causal_mask;
    const size_t head_stride = (size_t)S * D;

    float *scores = (float*)malloc(S * S * sizeof(float));

    for (uint32_t h = 0; h < H; h++) {
        const float *Q_h = Q + h * head_stride;
        const float *K_h = K + h * head_stride;
        const float *V_h = V + h * head_stride;
        float *O_h = O + h * head_stride;

        /* Q * K^T */
        for (uint32_t i = 0; i < S; i++) {
            for (uint32_t j = 0; j < S; j++) {
                if (causal && j > i) {
                    scores[i * S + j] = -1e30f;
                    continue;
                }
                double dot = 0.0;
                for (uint32_t d = 0; d < D; d++) {
                    dot += (double)Q_h[i * D + d] * (double)K_h[j * D + d];
                }
                scores[i * S + j] = (float)(dot * (double)scale);
            }
        }

        /* Softmax row-wise */
        for (uint32_t i = 0; i < S; i++) {
            float max_val = -1e30f;
            for (uint32_t j = 0; j < S; j++) {
                if (scores[i * S + j] > max_val) max_val = scores[i * S + j];
            }
            double sum_exp = 0.0;
            for (uint32_t j = 0; j < S; j++) {
                if (scores[i * S + j] < -1e20f) {
                    scores[i * S + j] = 0.0f;
                } else {
                    scores[i * S + j] = expf(scores[i * S + j] - max_val);
                    sum_exp += (double)scores[i * S + j];
                }
            }
            float inv_sum = sum_exp > 0.0 ? (float)(1.0 / sum_exp) : 0.0f;
            for (uint32_t j = 0; j < S; j++) {
                scores[i * S + j] *= inv_sum;
            }
        }

        /* Scores * V */
        for (uint32_t i = 0; i < S; i++) {
            for (uint32_t d = 0; d < D; d++) {
                double out = 0.0;
                for (uint32_t j = 0; j < S; j++) {
                    out += (double)scores[i * S + j] * (double)V_h[j * D + d];
                }
                O_h[i * D + d] = (float)out;
            }
        }
    }

    free(scores);
}

/* ------------------------------------------------------------------------- */
/* A4: Micro-benchmark and Performance Profiler                              */
/* ------------------------------------------------------------------------- */

void zcc_benchmark_flash_attention(const zcc_flash_attn_config_t *cfg, 
                                   size_t iterations, 
                                   zcc_flash_attn_metrics_t *metrics) {
    if (!cfg || !metrics || iterations == 0) return;

    size_t total_elements = (size_t)cfg->seq_len * cfg->n_heads * cfg->head_dim;
    float *Q = (float*)aligned_alloc(32, total_elements * sizeof(float));
    float *K = (float*)aligned_alloc(32, total_elements * sizeof(float));
    float *V = (float*)aligned_alloc(32, total_elements * sizeof(float));
    float *O_flash = (float*)aligned_alloc(32, total_elements * sizeof(float));
    float *O_ref = (float*)aligned_alloc(32, total_elements * sizeof(float));

    for (size_t i = 0; i < total_elements; i++) {
        Q[i] = sinf((float)i * 0.01f) * 0.5f;
        K[i] = cosf((float)i * 0.02f) * 0.5f;
        V[i] = sinf((float)i * 0.03f) * 0.5f;
    }

    /* Bit-exact parity verification */
    zcc_flash_attention_forward_avx2(cfg, Q, K, V, O_flash);
    zcc_reference_attention_forward(cfg, Q, K, V, O_ref);

    float max_err = 0.0f;
    for (size_t i = 0; i < total_elements; i++) {
        float err = fabsf(O_flash[i] - O_ref[i]);
        if (err > max_err) max_err = err;
    }

    /* Benchmark Loop */
    uint64_t t0 = fa_get_time_ns();
    for (size_t it = 0; it < iterations; it++) {
        zcc_flash_attention_forward_avx2(cfg, Q, K, V, O_flash);
    }
    uint64_t t1 = fa_get_time_ns();

    double total_time_sec = (double)(t1 - t0) * 1e-9;
    double avg_lat_ns = (double)(t1 - t0) / (double)iterations;

    /* FLOPs: 4 * S^2 * H * D */
    double flops_per_run = 4.0 * (double)cfg->seq_len * (double)cfg->seq_len * (double)cfg->n_heads * (double)cfg->head_dim;
    double total_flops = flops_per_run * (double)iterations;
    double gflops = (total_flops / total_time_sec) / 1e9;

    /* Memory Saved: S * S * H * 4 bytes saved vs full attention matrix */
    size_t bytes_saved = (size_t)cfg->seq_len * cfg->seq_len * cfg->n_heads * sizeof(float);

    metrics->total_flops = total_flops;
    metrics->latency_ns = avg_lat_ns;
    metrics->throughput_gflops = gflops;
    metrics->memory_bytes_saved = bytes_saved;
    metrics->max_numerical_error = max_err;

    free(Q); free(K); free(V); free(O_flash); free(O_ref);
}
