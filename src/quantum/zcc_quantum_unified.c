#include "zcc_quantum_unified.h"
#include <immintrin.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────────────────────────────────
 * 1. Quantum Spectral Form Factor (SFF) & Graph Rigidity (Hardened + FMA)
 * ────────────────────────────────────────────────────────────────────────── */
zq_status_t zcc_qcfg_spectral_analyze_avx2(const float *eigenvalues,
                                          size_t n_evals,
                                          zq_spectral_metrics_t *out_metrics) {
    if (!eigenvalues || !out_metrics) return ZQ_ERR_NULL_PTR;
    if (n_evals < 4) return ZQ_ERR_DIMENSION;

    /* 1. Compute level spacing ratios r_i = min(s_i, s_{i-1}) / max(s_i, s_{i-1}) */
    float r_sum = 0.0f;
    size_t count = 0;

    for (size_t i = 1; i < n_evals - 1; i++) {
        /* NaN / Inf Guard */
        if (isnan(eigenvalues[i]) || isinf(eigenvalues[i])) continue;

        float s0 = fabsf(eigenvalues[i] - eigenvalues[i - 1]);
        float s1 = fabsf(eigenvalues[i + 1] - eigenvalues[i]);
        if (s0 > 1e-7f && s1 > 1e-7f) {
            float r = (s0 < s1) ? (s0 / s1) : (s1 / s0);
            if (!isnan(r) && !isinf(r) && r >= 0.0f && r <= 1.0f) {
                r_sum += r;
                count++;
            }
        }
    }
    float avg_r = (count > 0) ? (r_sum / (float)count) : 0.386f;
    if (avg_r < 0.0f) avg_r = 0.0f;
    if (avg_r > 1.0f) avg_r = 1.0f;

    /* 2. Compute SFF K(t) at sample time t=1.0 using AVX2 SIMD */
    float t_sample = 1.0f;
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;

    size_t i = 0;
    for (; i + 8 <= n_evals; i += 8) {
        __m256 ev = _mm256_loadu_ps(&eigenvalues[i]);
        __m256 phase = _mm256_mul_ps(ev, _mm256_set1_ps(t_sample));
        
        float ph[8] __attribute__((aligned(32)));
        _mm256_storeu_ps(ph, phase);
        for (int k = 0; k < 8; k++) {
            if (!isnan(ph[k])) {
                sum_cos += cosf(ph[k]);
                sum_sin += sinf(ph[k]);
            }
        }
    }
    for (; i < n_evals; i++) {
        if (!isnan(eigenvalues[i])) {
            float ph = eigenvalues[i] * t_sample;
            sum_cos += cosf(ph);
            sum_sin += sinf(ph);
        }
    }

    float sff = (sum_cos * sum_cos + sum_sin * sum_sin) / (float)(n_evals * n_evals);
    if (isnan(sff) || isinf(sff)) sff = 0.001f;

    /* 3. Populate hardened metrics */
    out_metrics->r_spacing_ratio = avg_r;
    out_metrics->sff_mean = sff;
    out_metrics->spectral_dim = 2.0f * (1.0f + avg_r);
    out_metrics->is_chaotic_goe = (avg_r >= 0.46f);

    return ZQ_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 2. 16-Node Quantum Walk Mutation Superposition Tournament (FMA + Hardened)
 * ────────────────────────────────────────────────────────────────────────── */
zq_status_t zcc_qmut_tournament_init(zq_mut_tournament_t *tourn,
                                     const float *conflict_mat,
                                     const float *fitness_deltas) {
    if (!tourn) return ZQ_ERR_NULL_PTR;

    memset(tourn, 0, sizeof(zq_mut_tournament_t));

    float inv_sqrt = 0.25f; /* 1/sqrt(16) */
    for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) {
        tourn->re[i] = inv_sqrt;
        tourn->im[i] = 0.0f;
        tourn->prob[i] = inv_sqrt * inv_sqrt;
    }
    tourn->total_norm = 1.0f;
    tourn->step = 0;

    if (conflict_mat) {
        memcpy(tourn->conflict_mat, conflict_mat, sizeof(float) * ZQ_MAX_MUTATIONS * ZQ_MAX_MUTATIONS);
    }
    if (fitness_deltas) {
        for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) {
            float fd = fitness_deltas[i];
            /* Tripwire clamp extreme deltas */
            if (isnan(fd) || isinf(fd)) fd = 0.0f;
            if (fd < -50.0f) fd = -50.0f;
            if (fd > 50.0f) fd = 50.0f;
            tourn->fitness_deltas[i] = fd;
        }
    }

    return ZQ_OK;
}

zq_status_t zcc_qmut_tournament_step_avx2(zq_mut_tournament_t *tourn, float dt) {
    if (!tourn) return ZQ_ERR_NULL_PTR;
    if (dt <= 0.0f || isnan(dt) || isinf(dt)) dt = 0.05f;

    float next_re[ZQ_MAX_MUTATIONS] __attribute__((aligned(64)));
    float next_im[ZQ_MAX_MUTATIONS] __attribute__((aligned(64)));
    float total_norm = 0.0f;

    /* Load entire 16-node state into 4 YMM registers */
    __m256 s_re0 = _mm256_loadu_ps(&tourn->re[0]);
    __m256 s_re1 = _mm256_loadu_ps(&tourn->re[8]);
    __m256 s_im0 = _mm256_loadu_ps(&tourn->im[0]);
    __m256 s_im1 = _mm256_loadu_ps(&tourn->im[8]);

    for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) {
        const float *row = &tourn->conflict_mat[i * ZQ_MAX_MUTATIONS];

        __m256 r0 = _mm256_loadu_ps(&row[0]);
        __m256 r1 = _mm256_loadu_ps(&row[8]);

        /* FMA vector dot product: p_re = r0*s_re0 + r1*s_re1 */
        __m256 p_re = _mm256_fmadd_ps(r0, s_re0, _mm256_mul_ps(r1, s_re1));
        __m256 p_im = _mm256_fmadd_ps(r0, s_im0, _mm256_mul_ps(r1, s_im1));

        __m128 lo_r = _mm256_castps256_ps128(p_re);
        __m128 hi_r = _mm256_extractf128_ps(p_re, 1);
        __m128 sum_r = _mm_add_ps(lo_r, hi_r);
        sum_r = _mm_hadd_ps(sum_r, sum_r);
        sum_r = _mm_hadd_ps(sum_r, sum_r);
        float hop_re = _mm_cvtss_f32(sum_r);

        __m128 lo_i = _mm256_castps256_ps128(p_im);
        __m128 hi_i = _mm256_extractf128_ps(p_im, 1);
        __m128 sum_i = _mm_add_ps(lo_i, hi_i);
        sum_i = _mm_hadd_ps(sum_i, sum_i);
        sum_i = _mm_hadd_ps(sum_i, sum_i);
        float hop_im = _mm_cvtss_f32(sum_i);

        /* Constructive fitness amplification with destructive conflict cancellation */
        float amp = 1.0f + 0.1f * tourn->fitness_deltas[i];
        if (amp < 0.05f) amp = 0.05f;
        if (amp > 10.0f) amp = 10.0f;

        float base_re = (tourn->re[i] - 0.2f * hop_re) * amp;
        float base_im = (tourn->im[i] - 0.2f * hop_im) * amp;

        float V_i = tourn->fitness_deltas[i];
        float theta = (V_i + 1.0f) * dt;
        float cos_th = cosf(theta);
        float sin_th = sinf(theta);

        next_re[i] = base_re * cos_th + base_im * sin_th;
        next_im[i] = base_im * cos_th - base_re * sin_th;

        if (isnan(next_re[i]) || isinf(next_re[i])) next_re[i] = 0.0f;
        if (isnan(next_im[i]) || isinf(next_im[i])) next_im[i] = 0.0f;

        tourn->prob[i] = next_re[i] * next_re[i] + next_im[i] * next_im[i];
        total_norm += tourn->prob[i];
    }

    /* Unitary normalization with underflow protection */
    if (total_norm > 1e-10f && !isnan(total_norm) && !isinf(total_norm)) {
        float inv_norm = 1.0f / sqrtf(total_norm);
        for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) {
            tourn->re[i] = next_re[i] * inv_norm;
            tourn->im[i] = next_im[i] * inv_norm;
            tourn->prob[i] = tourn->re[i] * tourn->re[i] + tourn->im[i] * tourn->im[i];
        }
        tourn->total_norm = 1.0f;
    } else {
        /* Safe recovery fallback to uniform superposition */
        float inv_sqrt = 0.25f;
        for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) {
            tourn->re[i] = inv_sqrt;
            tourn->im[i] = 0.0f;
            tourn->prob[i] = 0.0625f;
        }
        tourn->total_norm = 1.0f;
    }

    tourn->step++;
    return ZQ_OK;
}

zq_status_t zcc_qmut_tournament_select(const zq_mut_tournament_t *tourn,
                                       int *out_selected_indices,
                                       int max_select,
                                       int *out_count) {
    if (!tourn || !out_selected_indices || !out_count) return ZQ_ERR_NULL_PTR;
    if (max_select <= 0) {
        *out_count = 0;
        return ZQ_OK;
    }

    int order[ZQ_MAX_MUTATIONS];
    for (int i = 0; i < ZQ_MAX_MUTATIONS; i++) order[i] = i;

    /* Sort indices by probability density descending */
    for (int i = 0; i < ZQ_MAX_MUTATIONS - 1; i++) {
        for (int j = i + 1; j < ZQ_MAX_MUTATIONS; j++) {
            if (tourn->prob[order[j]] > tourn->prob[order[i]]) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    int selected = 0;
    for (int idx = 0; idx < ZQ_MAX_MUTATIONS && selected < max_select; idx++) {
        int cand = order[idx];
        bool conflict = false;

        for (int s = 0; s < selected; s++) {
            int prev = out_selected_indices[s];
            if (tourn->conflict_mat[cand * ZQ_MAX_MUTATIONS + prev] > 0.5f) {
                conflict = true;
                break;
            }
        }

        if (!conflict) {
            out_selected_indices[selected++] = cand;
        }
    }

    *out_count = selected;
    return ZQ_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 3. Transverse-Field Ising Spin-Glass Register Allocator (Hardened)
 * ────────────────────────────────────────────────────────────────────────── */
zq_status_t zcc_qising_init(zq_ising_regalloc_t *alloc,
                            uint32_t num_vars,
                            uint32_t num_colors,
                            const float *interference_mat) {
    if (!alloc) return ZQ_ERR_NULL_PTR;
    if (num_vars == 0 || num_vars > ZQ_MAX_REGISTERS || num_colors == 0 || num_colors > ZQ_MAX_REGISTERS) {
        return ZQ_ERR_DIMENSION;
    }

    memset(alloc, 0, sizeof(zq_ising_regalloc_t));
    alloc->num_vars = num_vars;
    alloc->num_colors = num_colors;
    alloc->transverse_field = 1.0f;

    if (interference_mat) {
        memcpy(alloc->interference, interference_mat, sizeof(float) * num_vars * num_vars);
    }

    /* Perturbed initial spin orientations to break symmetry */
    for (uint32_t i = 0; i < num_vars; i++) {
        alloc->spins[i] = ((float)i * 6.2831853f / (float)num_vars) + 0.2f * ((float)(i % 3) - 1.0f);
        alloc->momentum[i] = 0.0f;
    }

    return ZQ_OK;
}

zq_status_t zcc_qising_anneal_step_avx2(zq_ising_regalloc_t *alloc, float dt, float gamma_t) {
    if (!alloc) return ZQ_ERR_NULL_PTR;

    uint32_t n = alloc->num_vars;
    if (n == 0 || n > ZQ_MAX_REGISTERS) return ZQ_ERR_DIMENSION;

    alloc->transverse_field = gamma_t;

    float forces[ZQ_MAX_REGISTERS];
    memset(forces, 0, sizeof(forces));

    for (uint32_t i = 0; i < n; i++) {
        float f_i = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            if (i != j && alloc->interference[i * n + j] > 0.5f) {
                float d_theta = alloc->spins[i] - alloc->spins[j];
                f_i += sinf(d_theta);
            }
        }
        f_i += gamma_t * sinf(alloc->spins[i] * (float)alloc->num_colors);
        if (isnan(f_i) || isinf(f_i)) f_i = 0.0f;
        forces[i] = f_i;
    }

    for (uint32_t i = 0; i < n; i++) {
        alloc->momentum[i] = (alloc->momentum[i] * 0.90f) - forces[i] * dt;
        /* Momentum clamp to prevent explosive divergence */
        if (alloc->momentum[i] > 10.0f) alloc->momentum[i] = 10.0f;
        if (alloc->momentum[i] < -10.0f) alloc->momentum[i] = -10.0f;

        alloc->spins[i] += alloc->momentum[i] * dt;

        while (alloc->spins[i] < 0.0f) alloc->spins[i] += 6.2831853f;
        while (alloc->spins[i] >= 6.2831853f) alloc->spins[i] -= 6.2831853f;
    }

    return ZQ_OK;
}

zq_status_t zcc_qising_extract_colors(const zq_ising_regalloc_t *alloc,
                                      int *out_colors,
                                      int *out_spill_count) {
    if (!alloc || !out_colors) return ZQ_ERR_NULL_PTR;

    uint32_t n = alloc->num_vars;
    uint32_t k = alloc->num_colors;
    if (n == 0 || k == 0) return ZQ_ERR_DIMENSION;

    float sector = 6.2831853f / (float)k;
    for (uint32_t i = 0; i < n; i++) {
        int color = (int)(alloc->spins[i] / sector) % k;
        if (color < 0) color = 0;
        if ((uint32_t)color >= k) color = k - 1;
        out_colors[i] = color;
    }

    /* Greedy local repair */
    for (uint32_t i = 0; i < n; i++) {
        bool in_conflict = false;
        bool color_used[ZQ_MAX_REGISTERS];
        memset(color_used, 0, sizeof(color_used));

        for (uint32_t j = 0; j < n; j++) {
            if (i != j && alloc->interference[i * n + j] > 0.5f) {
                if ((uint32_t)out_colors[j] < k) {
                    color_used[out_colors[j]] = true;
                }
                if (out_colors[i] == out_colors[j]) {
                    in_conflict = true;
                }
            }
        }

        if (in_conflict) {
            for (uint32_t c = 0; c < k; c++) {
                if (!color_used[c]) {
                    out_colors[i] = c;
                    break;
                }
            }
        }
    }

    int spills = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (alloc->interference[i * n + j] > 0.5f && out_colors[i] == out_colors[j]) {
                spills++;
            }
        }
    }

    if (out_spill_count) *out_spill_count = spills;
    return ZQ_OK;
}
