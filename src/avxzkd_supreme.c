#include "avxzkd_supreme.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>

#if defined(_OPENMP)
#include <omp.h>
#endif

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <stdlib.h>
#define _aligned_free free
#endif

/* ========================================================================= */
/* Hardware CPU Feature Detection                                            */
/* ========================================================================= */

uint32_t avxzkd_get_cpu_features(void) {
    uint32_t flags = AVXZKD_CPU_SCALAR;
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2"))    flags |= AVXZKD_CPU_AVX2;
    if (__builtin_cpu_supports("fma"))     flags |= AVXZKD_CPU_FMA;
    if (__builtin_cpu_supports("avx512f")) flags |= AVXZKD_CPU_AVX512F;
    if (__builtin_cpu_supports("avx512bw"))flags |= AVXZKD_CPU_AVX512BW;
#endif
    return flags;
}

/* ========================================================================= */
/* Fast In-Register Cryptographic Hash (SipHash-64 Chunk Digest)            */
/* ========================================================================= */

static inline uint64_t _avxzkd_digest64(const float *data, size_t count) {
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;

    const uint64_t *p = (const uint64_t*)data;
    size_t words = (count * sizeof(float)) / sizeof(uint64_t);

    for (size_t i = 0; i < words; ++i) {
        uint64_t m = p[i];
        v3 ^= m;
        for (int r = 0; r < 2; ++r) {
            v0 += v1; v1 = (v1 << 13) | (v1 >> 51); v1 ^= v0; v0 = (v0 << 32) | (v0 >> 32);
            v2 += v3; v3 = (v3 << 16) | (v3 >> 48); v3 ^= v2;
            v0 += v3; v3 = (v3 << 21) | (v3 >> 43); v3 ^= v0;
            v2 += v1; v1 = (v1 << 17) | (v1 >> 47); v1 ^= v2; v2 = (v2 << 32) | (v2 >> 32);
        }
        v0 ^= m;
    }
    return v0 ^ v1 ^ v2 ^ v3;
}

/* ========================================================================= */
/* Allocation with Cache-Line Zeroing                                        */
/* ========================================================================= */

avxzkd_field_t* avxzkd_create(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return NULL;

    avxzkd_field_t *f = (avxzkd_field_t*)calloc(1, sizeof(avxzkd_field_t));
    if (!f) return NULL;

    f->width  = (width + 31) & ~31;
    f->height = height;
    f->stride = f->width;
    f->total_cells = f->stride * f->height;

    size_t bytes = f->total_cells * sizeof(float);
#if defined(_MSC_VER) || defined(__MINGW32__)
    f->base        = (float*)_aligned_malloc(bytes, AVXZKD_ALIGN);
    f->current     = (float*)_aligned_malloc(bytes, AVXZKD_ALIGN);
    f->scars       = (float*)_aligned_malloc(bytes, AVXZKD_ALIGN);
    f->curvature   = (float*)_aligned_malloc(bytes, AVXZKD_ALIGN);
    f->hessian_det = (float*)_aligned_malloc(bytes, AVXZKD_ALIGN);
#else
    if (posix_memalign((void**)&f->base, AVXZKD_ALIGN, bytes) != 0) f->base = NULL;
    if (posix_memalign((void**)&f->current, AVXZKD_ALIGN, bytes) != 0) f->current = NULL;
    if (posix_memalign((void**)&f->scars, AVXZKD_ALIGN, bytes) != 0) f->scars = NULL;
    if (posix_memalign((void**)&f->curvature, AVXZKD_ALIGN, bytes) != 0) f->curvature = NULL;
    if (posix_memalign((void**)&f->hessian_det, AVXZKD_ALIGN, bytes) != 0) f->hessian_det = NULL;
#endif

    if (!f->base || !f->current || !f->scars || !f->curvature || !f->hessian_det) {
        avxzkd_destroy(f);
        return NULL;
    }

    memset(f->base, 0, bytes);
    memset(f->current, 0, bytes);
    memset(f->scars, 0, bytes);
    memset(f->curvature, 0, bytes);
    memset(f->hessian_det, 0, bytes);
    return f;
}

void avxzkd_destroy(avxzkd_field_t *f) {
    if (!f) return;
    size_t bytes = f->total_cells * sizeof(float);
    if (f->base)        { memset(f->base, 0, bytes);        _aligned_free(f->base); }
    if (f->current)     { memset(f->current, 0, bytes);     _aligned_free(f->current); }
    if (f->scars)       { memset(f->scars, 0, bytes);       _aligned_free(f->scars); }
    if (f->curvature)   { memset(f->curvature, 0, bytes);   _aligned_free(f->curvature); }
    if (f->hessian_det) { memset(f->hessian_det, 0, bytes); _aligned_free(f->hessian_det); }
    free(f);
}

avxzkd_status_t avxzkd_init(avxzkd_field_t *f, const float *init_base) {
    if (!f || !init_base) return AVXZKD_ERR_NULL_PTR;
    for (uint32_t y = 0; y < f->height; ++y) {
        memcpy(&f->base[y * f->stride], &init_base[y * f->width], f->width * sizeof(float));
        memcpy(&f->current[y * f->stride], &init_base[y * f->width], f->width * sizeof(float));
    }
    memset(f->scars, 0, f->total_cells * sizeof(float));
    return AVXZKD_OK;
}

/* ========================================================================= */
/* AVX2 (256-Bit) Vectorized Padé [5/4] Sigmoid Kernel                      */
/* ========================================================================= */

static inline __m256 _avx2_sig54(__m256 u, __m256 *nan_flag) {
    const __m256 half    = _mm256_set1_ps(0.5f);
    const __m256 one     = _mm256_set1_ps(1.0f);
    const __m256 c_105   = _mm256_set1_ps(105.0f);
    const __m256 c_945   = _mm256_set1_ps(945.0f);
    const __m256 c_15    = _mm256_set1_ps(15.0f);
    const __m256 c_420   = _mm256_set1_ps(420.0f);
    const __m256 x_max   = _mm256_set1_ps(4.5f);
    const __m256 x_min   = _mm256_set1_ps(-4.5f);

    *nan_flag = _mm256_cmp_ps(u, u, _CMP_UNORD_Q);

    __m256 x = _mm256_mul_ps(u, half);
    __m256 x_clamped = _mm256_max_ps(x_min, _mm256_min_ps(x_max, x));
    __m256 x2 = _mm256_mul_ps(x_clamped, x_clamped);
    __m256 x4 = _mm256_mul_ps(x2, x2);

    __m256 num_inner = _mm256_fmadd_ps(c_105, x2, _mm256_add_ps(x4, c_945));
    __m256 num = _mm256_mul_ps(x_clamped, num_inner);
    __m256 den = _mm256_fmadd_ps(c_15, x4, _mm256_fmadd_ps(c_420, x2, c_945));

    __m256 tanh_val = _mm256_div_ps(num, den);
    return _mm256_mul_ps(half, _mm256_add_ps(one, tanh_val));
}

avxzkd_status_t avxzkd_deep_recurse_avx2(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps) {
    if (!field || !params) return AVXZKD_ERR_NULL_PTR;

    const __m256 v_eta   = _mm256_set1_ps(params->eta);
    const __m256 v_gamma = _mm256_set1_ps(params->gamma);
    const uint32_t total = field->total_cells;

    for (uint32_t k = 0; k < k_steps; ++k) {
        for (uint32_t i = 0; i < total; i += 16) {
            __m256 b0 = _mm256_load_ps(&field->base[i]);
            __m256 b1 = _mm256_load_ps(&field->base[i + 8]);
            __m256 h0 = _mm256_load_ps(&field->current[i]);
            __m256 h1 = _mm256_load_ps(&field->current[i + 8]);

            __m256 nan0, nan1;
            __m256 sig0 = _avx2_sig54(_mm256_mul_ps(v_gamma, h0), &nan0);
            __m256 sig1 = _avx2_sig54(_mm256_mul_ps(v_gamma, h1), &nan1);

            if (_mm256_movemask_ps(_mm256_or_ps(nan0, nan1)) != 0) {
                field->tripwires++;
                return AVXZKD_ERR_NAN_FAULT;
            }

            __m256 next0 = _mm256_fmadd_ps(_mm256_mul_ps(v_eta, h0), sig0, b0);
            __m256 next1 = _mm256_fmadd_ps(_mm256_mul_ps(v_eta, h1), sig1, b1);

            _mm256_store_ps(&field->current[i], next0);
            _mm256_store_ps(&field->current[i + 8], next1);
        }
    }
    return AVXZKD_OK;
}

#if defined(__AVX512F__)
static inline __m512 _avx512_sig54(__m512 u, __mmask16 *nan_mask) {
    const __m512 half    = _mm512_set1_ps(0.5f);
    const __m512 one     = _mm512_set1_ps(1.0f);
    const __m512 c_105   = _mm512_set1_ps(105.0f);
    const __m512 c_945   = _mm512_set1_ps(945.0f);
    const __m512 c_15    = _mm512_set1_ps(15.0f);
    const __m512 c_420   = _mm512_set1_ps(420.0f);
    const __m512 x_max   = _mm512_set1_ps(4.5f);
    const __m512 x_min   = _mm512_set1_ps(-4.5f);

    *nan_mask = _mm512_cmp_ps_mask(u, u, _CMP_UNORD_Q);

    __m512 x = _mm512_mul_ps(u, half);
    __m512 x_clamped = _mm512_max_ps(x_min, _mm512_min_ps(x_max, x));
    __m512 x2 = _mm512_mul_ps(x_clamped, x_clamped);
    __m512 x4 = _mm512_mul_ps(x2, x2);

    __m512 num_inner = _mm512_fmadd_ps(c_105, x2, _mm512_add_ps(x4, c_945));
    __m512 num = _mm512_mul_ps(x_clamped, num_inner);
    __m512 den = _mm512_fmadd_ps(c_15, x4, _mm512_fmadd_ps(c_420, x2, c_945));

    __m512 tanh_val = _mm512_div_ps(num, den);
    return _mm512_mul_ps(half, _mm512_add_ps(one, tanh_val));
}

avxzkd_status_t avxzkd_deep_recurse_avx512(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps) {
    if (!field || !params) return AVXZKD_ERR_NULL_PTR;

    const __m512 v_eta   = _mm512_set1_ps(params->eta);
    const __m512 v_gamma = _mm512_set1_ps(params->gamma);
    const uint32_t total = field->total_cells;

    for (uint32_t k = 0; k < k_steps; ++k) {
        for (uint32_t i = 0; i < total; i += 32) {
            __m512 b0 = _mm512_load_ps(&field->base[i]);
            __m512 b1 = _mm512_load_ps(&field->base[i + 16]);
            __m512 h0 = _mm512_load_ps(&field->current[i]);
            __m512 h1 = _mm512_load_ps(&field->current[i + 16]);

            __mmask16 nan0, nan1;
            __m512 sig0 = _avx512_sig54(_mm512_mul_ps(v_gamma, h0), &nan0);
            __m512 sig1 = _avx512_sig54(_mm512_mul_ps(v_gamma, h1), &nan1);

            if ((nan0 | nan1) != 0) {
                field->tripwires++;
                return AVXZKD_ERR_NAN_FAULT;
            }

            __m512 next0 = _mm512_fmadd_ps(_mm512_mul_ps(v_eta, h0), sig0, b0);
            __m512 next1 = _mm512_fmadd_ps(_mm512_mul_ps(v_eta, h1), sig1, b1);

            _mm512_store_ps(&field->current[i], next0);
            _mm512_store_ps(&field->current[i + 16], next1);
        }
    }
    return AVXZKD_OK;
}
#else
avxzkd_status_t avxzkd_deep_recurse_avx512(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps) {
    return avxzkd_deep_recurse_avx2(field, params, k_steps);
}
#endif

avxzkd_status_t avxzkd_deep_recurse_auto(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps) {
    uint32_t cpu = avxzkd_get_cpu_features();
#if defined(__AVX512F__)
    if (cpu & AVXZKD_CPU_AVX512F) {
        return avxzkd_deep_recurse_avx512(field, params, k_steps);
    }
#endif
    if (cpu & AVXZKD_CPU_AVX2) {
        return avxzkd_deep_recurse_avx2(field, params, k_steps);
    }
    return avxzkd_deep_recurse_avx2(field, params, k_steps);
}

avxzkd_status_t avxzkd_deep_recurse_parallel(avxzkd_field_t *field, avxzkd_params_t *params, uint32_t k_steps, uint32_t threads) {
    if (!field || !params) return AVXZKD_ERR_NULL_PTR;

    const __m256 v_eta   = _mm256_set1_ps(params->eta);
    const __m256 v_gamma = _mm256_set1_ps(params->gamma);
    const uint32_t total = field->total_cells;

#if defined(_OPENMP)
    if (threads > 0) omp_set_num_threads(threads);
#else
    (void)threads;
#endif

    for (uint32_t k = 0; k < k_steps; ++k) {
#if defined(_OPENMP)
        #pragma omp parallel for schedule(static)
#endif
        for (uint32_t i = 0; i < total; i += 16) {
            __m256 b0 = _mm256_load_ps(&field->base[i]);
            __m256 b1 = _mm256_load_ps(&field->base[i + 8]);
            __m256 h0 = _mm256_load_ps(&field->current[i]);
            __m256 h1 = _mm256_load_ps(&field->current[i + 8]);

            __m256 nan0, nan1;
            __m256 sig0 = _avx2_sig54(_mm256_mul_ps(v_gamma, h0), &nan0);
            __m256 sig1 = _avx2_sig54(_mm256_mul_ps(v_gamma, h1), &nan1);

            __m256 next0 = _mm256_fmadd_ps(_mm256_mul_ps(v_eta, h0), sig0, b0);
            __m256 next1 = _mm256_fmadd_ps(_mm256_mul_ps(v_eta, h1), sig1, b1);

            _mm256_store_ps(&field->current[i], next0);
            _mm256_store_ps(&field->current[i + 8], next1);
        }
    }
    return AVXZKD_OK;
}

/* ========================================================================= */
/* Topological Curvature & Hessian Discriminator                             */
/* ========================================================================= */

avxzkd_status_t avxzkd_compute_topology_avx2(avxzkd_field_t *field) {
    if (!field) return AVXZKD_ERR_NULL_PTR;

    const uint32_t s = field->stride;
    const __m256 c_center = _mm256_set1_ps(-4.0f);
    const __m256 half     = _mm256_set1_ps(0.5f);

    for (uint32_t y = 1; y < field->height - 1; ++y) {
        for (uint32_t x = 0; x < field->width; x += 8) {
            float *c = &field->current[y * s + x];

            __m256 center = _mm256_load_ps(c);
            __m256 n      = _mm256_load_ps(c - s);
            __m256 s_val  = _mm256_load_ps(c + s);
            __m256 w      = _mm256_loadu_ps(c - 1);
            __m256 e      = _mm256_loadu_ps(c + 1);

            __m256 lap = _mm256_fmadd_ps(c_center, center, _mm256_add_ps(_mm256_add_ps(n, s_val), _mm256_add_ps(w, e)));
            _mm256_store_ps(&field->curvature[y * s + x], lap);

            __m256 h_xx = _mm256_sub_ps(_mm256_add_ps(e, w), _mm256_add_ps(center, center));
            __m256 h_yy = _mm256_sub_ps(_mm256_add_ps(s_val, n), _mm256_add_ps(center, center));
            
            __m256 ne   = _mm256_loadu_ps(c - s + 1);
            __m256 nw   = _mm256_loadu_ps(c - s - 1);
            __m256 se   = _mm256_loadu_ps(c + s + 1);
            __m256 sw   = _mm256_loadu_ps(c + s - 1);
            __m256 h_xy = _mm256_mul_ps(half, _mm256_sub_ps(_mm256_sub_ps(se, sw), _mm256_sub_ps(ne, nw)));

            __m256 det  = _mm256_fmsub_ps(h_xx, h_yy, _mm256_mul_ps(h_xy, h_xy));
            _mm256_store_ps(&field->hessian_det[y * s + x], det);
        }
    }
    return AVXZKD_OK;
}

#if defined(__AVX512F__)
avxzkd_status_t avxzkd_compute_topology_avx512(avxzkd_field_t *field) {
    if (!field) return AVXZKD_ERR_NULL_PTR;

    const uint32_t s = field->stride;
    const __m512 c_center = _mm512_set1_ps(-4.0f);
    const __m512 half     = _mm512_set1_ps(0.5f);

    for (uint32_t y = 1; y < field->height - 1; ++y) {
        for (uint32_t x = 0; x < field->width; x += 16) {
            float *c = &field->current[y * s + x];

            __m512 center = _mm512_load_ps(c);
            __m512 n      = _mm512_load_ps(c - s);
            __m512 s_val  = _mm512_load_ps(c + s);
            __m512 w      = _mm512_loadu_ps(c - 1);
            __m512 e      = _mm512_loadu_ps(c + 1);

            __m512 lap = _mm512_fmadd_ps(c_center, center, _mm512_add_ps(_mm512_add_ps(n, s_val), _mm512_add_ps(w, e)));
            _mm512_store_ps(&field->curvature[y * s + x], lap);

            __m512 h_xx = _mm512_sub_ps(_mm512_add_ps(e, w), _mm512_add_ps(center, center));
            __m512 h_yy = _mm512_sub_ps(_mm512_add_ps(s_val, n), _mm512_add_ps(center, center));
            
            __m512 ne   = _mm512_loadu_ps(c - s + 1);
            __m512 nw   = _mm512_loadu_ps(c - s - 1);
            __m512 se   = _mm512_loadu_ps(c + s + 1);
            __m512 sw   = _mm512_loadu_ps(c + s - 1);
            __m512 h_xy = _mm512_mul_ps(half, _mm512_sub_ps(_mm512_sub_ps(se, sw), _mm512_sub_ps(ne, nw)));

            __m512 det  = _mm512_fmsub_ps(h_xx, h_yy, _mm512_mul_ps(h_xy, h_xy));
            _mm512_store_ps(&field->hessian_det[y * s + x], det);
        }
    }
    return AVXZKD_OK;
}
#else
avxzkd_status_t avxzkd_compute_topology_avx512(avxzkd_field_t *field) {
    return avxzkd_compute_topology_avx2(field);
}
#endif

/* ========================================================================= */
/* Dual-Field Co-Evolution Coupling Tensor                                   */
/* ========================================================================= */

avxzkd_status_t avxzkd_couple_fields_avx2(avxzkd_field_t *fa, avxzkd_field_t *fb, float kappa) {
    if (!fa || !fb) return AVXZKD_ERR_NULL_PTR;
    if (fa->total_cells != fb->total_cells) return AVXZKD_ERR_ALIGNMENT;

    const __m256 v_kappa = _mm256_set1_ps(kappa);
    uint32_t n = fa->total_cells;

    for (uint32_t i = 0; i < n; i += 8) {
        __m256 ha = _mm256_load_ps(&fa->current[i]);
        __m256 hb = _mm256_load_ps(&fb->current[i]);

        __m256 diff = _mm256_sub_ps(hb, ha);
        __m256 delta = _mm256_mul_ps(v_kappa, diff);

        _mm256_store_ps(&fa->current[i], _mm256_add_ps(ha, delta));
        _mm256_store_ps(&fb->current[i], _mm256_sub_ps(hb, delta));
    }
    return AVXZKD_OK;
}

#if defined(__AVX512F__)
avxzkd_status_t avxzkd_couple_fields_avx512(avxzkd_field_t *fa, avxzkd_field_t *fb, float kappa) {
    if (!fa || !fb) return AVXZKD_ERR_NULL_PTR;
    if (fa->total_cells != fb->total_cells) return AVXZKD_ERR_ALIGNMENT;

    const __m512 v_kappa = _mm512_set1_ps(kappa);
    uint32_t n = fa->total_cells;

    for (uint32_t i = 0; i < n; i += 16) {
        __m512 ha = _mm512_load_ps(&fa->current[i]);
        __m512 hb = _mm512_load_ps(&fb->current[i]);

        __m512 diff = _mm512_sub_ps(hb, ha);
        __m512 delta = _mm512_mul_ps(v_kappa, diff);

        _mm512_store_ps(&fa->current[i], _mm512_add_ps(ha, delta));
        _mm512_store_ps(&fb->current[i], _mm512_sub_ps(hb, delta));
    }
    return AVXZKD_OK;
}
#else
avxzkd_status_t avxzkd_couple_fields_avx512(avxzkd_field_t *fa, avxzkd_field_t *fb, float kappa) {
    return avxzkd_couple_fields_avx2(fa, fb, kappa);
}
#endif

/* ========================================================================= */
/* Cryptographic Attestation & Mathematical Invariant Audit                  */
/* ========================================================================= */

avxzkd_status_t avxzkd_audit(const avxzkd_field_t *field, const avxzkd_params_t *params, avxzkd_audit_t *audit) {
    if (!field || !params || !audit) return AVXZKD_ERR_NULL_PTR;

    audit->state_digest = _avxzkd_digest64(field->current, field->total_cells);
    float expected_gain = (params->eta < 1.0f) ? (1.0f / (1.0f - params->eta)) : 0.0f;

    double gain_accum = 0.0;
    uint32_t count = 0;
    float max_residual = 0.0f;

    for (uint32_t i = 0; i < field->total_cells; ++i) {
        float b = field->base[i];
        float c = field->current[i];

        float u = params->gamma * c;
        float sig = 0.5f * (1.0f + tanhf(0.5f * u));
        float t_c = b + params->eta * c * sig;
        float res = fabsf(c - t_c);
        if (res > max_residual) max_residual = res;

        if (b >= 10.0f) {
            gain_accum += (double)(c / b);
            count++;
        }
    }

    audit->measured_gain = (count > 0) ? (float)(gain_accum / count) : 0.0f;
    audit->floor_drift   = max_residual;

    bool gain_valid  = (count == 0) || (fabsf(audit->measured_gain - expected_gain) < 0.02f);
    bool residual_ok = (audit->floor_drift < 1e-3f);
    bool nan_valid   = (field->tripwires == 0);

    audit->pass_all_invariants = (gain_valid && residual_ok && nan_valid);
    return AVXZKD_OK;
}

/* ========================================================================= */
/* Two-Regime Navigator with Loop-Pruning Optimizer                          */
/* ========================================================================= */

avxzkd_walker_t* avxzkd_walker_create(int32_t sx, int32_t sy, int32_t tx, int32_t ty, uint32_t capacity) {
    avxzkd_walker_t *w = (avxzkd_walker_t*)calloc(1, sizeof(avxzkd_walker_t));
    if (!w) return NULL;
    w->x = sx; w->y = sy;
    w->target_x = tx; w->target_y = ty;
    w->capacity = capacity;
    w->path_x = (int32_t*)malloc(capacity * sizeof(int32_t));
    w->path_y = (int32_t*)malloc(capacity * sizeof(int32_t));
    if (!w->path_x || !w->path_y) {
        free(w->path_x); free(w->path_y); free(w);
        return NULL;
    }
    w->path_x[0] = sx;
    w->path_y[0] = sy;
    w->path_len = 1;
    return w;
}

int32_t avxzkd_walker_solve(avxzkd_walker_t *walker, avxzkd_field_t *field, avxzkd_params_t *params, uint32_t max_steps) {
    if (!walker || !field || !params) return AVXZKD_ERR_NULL_PTR;

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};

    for (uint32_t step = 0; step < max_steps; ++step) {
        if (walker->x == walker->target_x && walker->y == walker->target_y) {
            walker->solved = true;
            avxzkd_walker_prune_loops(walker);
            return (int32_t)walker->steps_taken;
        }

        float min_p = 1e30f;
        int best_dir = -1;

        for (int i = 0; i < 4; ++i) {
            int nx = walker->x + dx[i];
            int ny = walker->y + dy[i];

            if (nx < 0 || nx >= (int)field->width || ny < 0 || ny >= (int)field->height) continue;

            uint32_t idx = ny * field->stride + nx;
            float dist = sqrtf((float)((nx - walker->target_x)*(nx - walker->target_x) + (ny - walker->target_y)*(ny - walker->target_y)));
            float eff_h = field->current[idx] + field->scars[idx] + 1e-4f * dist;

            if (dx[i] == -walker->prev_dx && dy[i] == -walker->prev_dy) {
                eff_h += params->momentum * params->kick;
            }

            if (eff_h < min_p) {
                min_p = eff_h;
                best_dir = i;
            }
        }

        if (best_dir == -1) return AVXZKD_ERR_PATH_BLOCKED;

        field->scars[walker->y * field->stride + walker->x] += params->kick;

        walker->prev_dx = dx[best_dir];
        walker->prev_dy = dy[best_dir];
        walker->x += dx[best_dir];
        walker->y += dy[best_dir];
        walker->steps_taken++;

        if (walker->path_len < walker->capacity) {
            walker->path_x[walker->path_len] = walker->x;
            walker->path_y[walker->path_len] = walker->y;
            walker->path_len++;
        }
    }
    return walker->solved ? (int32_t)walker->steps_taken : -1;
}

void avxzkd_walker_prune_loops(avxzkd_walker_t *w) {
    if (!w || w->path_len < 3) return;

    int32_t *opt_x = (int32_t*)malloc(w->path_len * sizeof(int32_t));
    int32_t *opt_y = (int32_t*)malloc(w->path_len * sizeof(int32_t));
    if (!opt_x || !opt_y) { free(opt_x); free(opt_y); return; }
    uint32_t opt_len = 0;

    for (uint32_t i = 0; i < w->path_len; ++i) {
        int32_t cx = w->path_x[i];
        int32_t cy = w->path_y[i];

        int match_idx = -1;
        for (uint32_t j = 0; j < opt_len; ++j) {
            if (opt_x[j] == cx && opt_y[j] == cy) {
                match_idx = (int)j;
                break;
            }
        }

        if (match_idx >= 0) {
            opt_len = match_idx + 1;
        } else {
            opt_x[opt_len] = cx;
            opt_y[opt_len] = cy;
            opt_len++;
        }
    }

    memcpy(w->path_x, opt_x, opt_len * sizeof(int32_t));
    memcpy(w->path_y, opt_y, opt_len * sizeof(int32_t));
    w->path_len = opt_len;
    free(opt_x);
    free(opt_y);
}

void avxzkd_walker_destroy(avxzkd_walker_t *w) {
    if (!w) return;
    free(w->path_x);
    free(w->path_y);
    free(w);
}

/* ========================================================================= */
/* Layer 1: Vectorized Discrete-Time Quantum Walk (DTQW) Kernel             */
/* ========================================================================= */

avxzkd_dtqw_t* avxzkd_dtqw_create(void) {
    avxzkd_dtqw_t *qw = (avxzkd_dtqw_t*)calloc(1, sizeof(avxzkd_dtqw_t));
    if (!qw) return NULL;

    const float inv_sqrt2 = 0.7071067811865475f;
    qw->real0[0] = inv_sqrt2;
    qw->imag1[0] = inv_sqrt2;
    qw->coherence = 1.0;
    qw->total_steps = 0;
    return qw;
}

avxzkd_status_t avxzkd_dtqw_step_avx2(avxzkd_dtqw_t *qw, uint32_t steps) {
    if (!qw) return AVXZKD_ERR_NULL_PTR;

    const __m256 v_inv_sqrt2 = _mm256_set1_ps(0.7071067811865475f);
    float tmp_r0[16], tmp_i0[16], tmp_r1[16], tmp_i1[16];

    for (uint32_t s = 0; s < steps; ++s) {
        for (int i = 0; i < 16; i += 8) {
            __m256 r0 = _mm256_loadu_ps(&qw->real0[i]);
            __m256 i0 = _mm256_loadu_ps(&qw->imag0[i]);
            __m256 r1 = _mm256_loadu_ps(&qw->real1[i]);
            __m256 i1 = _mm256_loadu_ps(&qw->imag1[i]);

            __m256 c0_r = _mm256_mul_ps(v_inv_sqrt2, _mm256_add_ps(r0, r1));
            __m256 c0_i = _mm256_mul_ps(v_inv_sqrt2, _mm256_add_ps(i0, i1));
            __m256 c1_r = _mm256_mul_ps(v_inv_sqrt2, _mm256_sub_ps(r0, r1));
            __m256 c1_i = _mm256_mul_ps(v_inv_sqrt2, _mm256_sub_ps(i0, i1));

            _mm256_storeu_ps(&tmp_r0[i], c0_r);
            _mm256_storeu_ps(&tmp_i0[i], c0_i);
            _mm256_storeu_ps(&tmp_r1[i], c1_r);
            _mm256_storeu_ps(&tmp_i1[i], c1_i);
        }

        for (int x = 0; x < 16; ++x) {
            int left_idx  = (x - 1 + 16) & 15;
            int right_idx = (x + 1) & 15;

            qw->real0[x] = tmp_r0[right_idx];
            qw->imag0[x] = tmp_i0[right_idx];
            qw->real1[x] = tmp_r1[left_idx];
            qw->imag1[x] = tmp_i1[left_idx];
        }
        qw->total_steps++;
    }

    double rho0 = 0.0, rho1 = 0.0;
    for (int x = 0; x < 16; ++x) {
        double p0 = (double)qw->real0[x] * qw->real0[x] + (double)qw->imag0[x] * qw->imag0[x];
        double p1 = (double)qw->real1[x] * qw->real1[x] + (double)qw->imag1[x] * qw->imag1[x];
        qw->node_probs[x] = p0 + p1;
        rho0 += p0;
        rho1 += p1;

        double sum_r = (double)qw->real0[x] + (double)qw->real1[x];
        double sum_i = (double)qw->imag0[x] + (double)qw->imag1[x];
        qw->node_phases[x] = atan2(sum_i, sum_r);
    }

    double s0 = (rho0 > 1e-12) ? (-rho0 * log2(rho0)) : 0.0;
    double s1 = (rho1 > 1e-12) ? (-rho1 * log2(rho1)) : 0.0;
    qw->s_q0 = s0 + s1;

    return AVXZKD_OK;
}

#if defined(__AVX512F__)
avxzkd_status_t avxzkd_dtqw_step_avx512(avxzkd_dtqw_t *qw, uint32_t steps) {
    if (!qw) return AVXZKD_ERR_NULL_PTR;

    const __m512 v_inv_sqrt2 = _mm512_set1_ps(0.7071067811865475f);
    float tmp_r0[16], tmp_i0[16], tmp_r1[16], tmp_i1[16];

    for (uint32_t s = 0; s < steps; ++s) {
        /* Full 16-node parallel evaluation in single ZMM instruction */
        __m512 r0 = _mm512_loadu_ps(&qw->real0[0]);
        __m512 i0 = _mm512_loadu_ps(&qw->imag0[0]);
        __m512 r1 = _mm512_loadu_ps(&qw->real1[0]);
        __m512 i1 = _mm512_loadu_ps(&qw->imag1[0]);

        __m512 c0_r = _mm512_mul_ps(v_inv_sqrt2, _mm512_add_ps(r0, r1));
        __m512 c0_i = _mm512_mul_ps(v_inv_sqrt2, _mm512_add_ps(i0, i1));
        __m512 c1_r = _mm512_mul_ps(v_inv_sqrt2, _mm512_sub_ps(r0, r1));
        __m512 c1_i = _mm512_mul_ps(v_inv_sqrt2, _mm512_sub_ps(i0, i1));

        _mm512_storeu_ps(&tmp_r0[0], c0_r);
        _mm512_storeu_ps(&tmp_i0[0], c0_i);
        _mm512_storeu_ps(&tmp_r1[0], c1_r);
        _mm512_storeu_ps(&tmp_i1[0], c1_i);

        for (int x = 0; x < 16; ++x) {
            int left_idx  = (x - 1 + 16) & 15;
            int right_idx = (x + 1) & 15;

            qw->real0[x] = tmp_r0[right_idx];
            qw->imag0[x] = tmp_i0[right_idx];
            qw->real1[x] = tmp_r1[left_idx];
            qw->imag1[x] = tmp_i1[left_idx];
        }
        qw->total_steps++;
    }

    double rho0 = 0.0, rho1 = 0.0;
    for (int x = 0; x < 16; ++x) {
        double p0 = (double)qw->real0[x] * qw->real0[x] + (double)qw->imag0[x] * qw->imag0[x];
        double p1 = (double)qw->real1[x] * qw->real1[x] + (double)qw->imag1[x] * qw->imag1[x];
        qw->node_probs[x] = p0 + p1;
        rho0 += p0;
        rho1 += p1;

        double sum_r = (double)qw->real0[x] + (double)qw->real1[x];
        double sum_i = (double)qw->imag0[x] + (double)qw->imag1[x];
        qw->node_phases[x] = atan2(sum_i, sum_r);
    }

    double s0 = (rho0 > 1e-12) ? (-rho0 * log2(rho0)) : 0.0;
    double s1 = (rho1 > 1e-12) ? (-rho1 * log2(rho1)) : 0.0;
    qw->s_q0 = s0 + s1;

    return AVXZKD_OK;
}
#else
avxzkd_status_t avxzkd_dtqw_step_avx512(avxzkd_dtqw_t *qw, uint32_t steps) {
    return avxzkd_dtqw_step_avx2(qw, steps);
}
#endif

avxzkd_status_t avxzkd_dtqw_step_auto(avxzkd_dtqw_t *qw, uint32_t steps) {
    uint32_t cpu = avxzkd_get_cpu_features();
#if defined(__AVX512F__)
    if (cpu & AVXZKD_CPU_AVX512F) {
        return avxzkd_dtqw_step_avx512(qw, steps);
    }
#else
    (void)cpu;
#endif
    return avxzkd_dtqw_step_avx2(qw, steps);
}

avxzkd_status_t avxzkd_dtqw_dephase(avxzkd_dtqw_t *qw, float gamma_dephase) {
    if (!qw) return AVXZKD_ERR_NULL_PTR;
    float decay = expf(-fabsf(gamma_dephase));
    for (int x = 0; x < 16; ++x) {
        qw->imag0[x] *= decay;
        qw->imag1[x] *= decay;
    }
    qw->coherence *= (double)decay;
    return AVXZKD_OK;
}

void avxzkd_dtqw_destroy(avxzkd_dtqw_t *qw) {
    if (qw) free(qw);
}
