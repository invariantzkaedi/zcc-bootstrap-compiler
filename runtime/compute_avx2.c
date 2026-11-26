/* SPDX-License-Identifier: MIT */
#include "zcc_compute.h"
#include <immintrin.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int is_contig_1d_or_nd(const zcc_tensor_desc_t *t, uint64_t elem_size) {
    uint64_t exp_st = elem_size;
    for (int d = (int)t->rank - 1; d >= 0; d--) {
        int64_t st = t->strides[d];
        if (st != 0 && (uint64_t)st != exp_st) return 0;
        exp_st *= (uint64_t)t->shape[d];
    }
    return 1;
}

static int64_t total_elems_avx2(const zcc_tensor_desc_t *t) {
    int64_t n = 1;
    for (uint32_t d = 0U; d < t->rank; d++) n *= t->shape[d];
    return n;
}

static float hsum256_ps(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 s4 = _mm_add_ps(lo, hi);
    __m128 s2 = _mm_add_ps(s4, _mm_movehl_ps(s4, s4));
    __m128 s1 = _mm_add_ss(s2, _mm_shuffle_ps(s2, s2, 0x55));
    return _mm_cvtss_f32(s1);
}

static float hmax256_ps(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 m4 = _mm_max_ps(lo, hi);
    __m128 m2 = _mm_max_ps(m4, _mm_movehl_ps(m4, m4));
    __m128 m1 = _mm_max_ss(m2, _mm_shuffle_ps(m2, m2, 0x55));
    return _mm_cvtss_f32(m1);
}

static float f16_to_f32_avx2_scalar(uint16_t h) {
    uint32_t sign = ((uint32_t)(h & 0x8000U)) << 16;
    uint32_t exp  = (uint32_t)((h >> 10) & 0x1FU);
    uint32_t mant = (uint32_t)(h & 0x03FFU);
    uint32_t bits;
    if (exp == 0U) {
        if (mant == 0U) {
            bits = sign;
        } else {
            uint32_t e = 113U;
            while ((mant & 0x0400U) == 0U) {
                mant <<= 1;
                e--;
            }
            mant &= 0x03FFU;
            bits = sign | (e << 23) | (mant << 13);
        }
    } else if (exp == 31U) {
        bits = (mant == 0U) ? (sign | 0x7F800000U) : (sign | 0x7FC00000U | (mant << 13));
    } else {
        bits = sign | ((exp + 112U) << 23) | (mant << 13);
    }
    float res = 0.0f;
    memcpy(&res, &bits, sizeof(res));
    return res;
}

static uint16_t ld_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t q4k_sc(const uint8_t *scales, uint32_t j) {
    return (j < 4U) ? (uint32_t)(scales[j] & 63U)
                    : (uint32_t)((scales[j + 4U] & 15U) | ((scales[j - 4U] >> 6) << 4));
}

static uint32_t q4k_mn(const uint8_t *scales, uint32_t j) {
    return (j < 4U) ? (uint32_t)(scales[j + 4U] & 63U)
                    : (uint32_t)((scales[j + 4U] >> 4) | ((scales[j] >> 6) << 4));
}

static void avx2_vec_add(const float *a, const float *b, float *out, int64_t n) {
    int64_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        _mm256_storeu_ps(out + i, _mm256_add_ps(va, vb));
    }
    for (; i < n; i++) out[i] = a[i] + b[i];
}

static void avx2_vec_mul(const float *a, const float *b, float *out, int64_t n) {
    int64_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        _mm256_storeu_ps(out + i, _mm256_mul_ps(va, vb));
    }
    for (; i < n; i++) out[i] = a[i] * b[i];
}

static void avx2_vec_fma(const float *a, const float *b, const float *c, float *out, int64_t n) {
    int64_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_loadu_ps(c + i);
        _mm256_storeu_ps(out + i, _mm256_fmadd_ps(va, vb, vc));
    }
    for (; i < n; i++) out[i] = fmaf(a[i], b[i], c[i]);
}

static float avx2_dot_f32(const float *a, const float *b, int64_t n) {
    __m256 acc0 = _mm256_setzero_ps();
    __m256 acc1 = _mm256_setzero_ps();
    int64_t i = 0;
    for (; i + 16 <= n; i += 16) {
        acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i), acc0);
        acc1 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i + 8), _mm256_loadu_ps(b + i + 8), acc1);
    }
    for (; i + 8 <= n; i += 8) {
        acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i), acc0);
    }
    float total = hsum256_ps(_mm256_add_ps(acc0, acc1));
    for (; i < n; i++) total = fmaf(a[i], b[i], total);
    return total;
}

static void avx2_gemm_f32(const zcc_tensor_desc_t *A, const zcc_tensor_desc_t *B, zcc_tensor_desc_t *C) {
    int64_t M = A->shape[0];
    int64_t K = A->shape[1];
    int64_t N = B->shape[1];
    int64_t lda = (A->strides[0] > 0) ? (A->strides[0] / 4) : K;
    int64_t ldb = (B->strides[0] > 0) ? (B->strides[0] / 4) : N;
    int64_t ldc = (C->strides[0] > 0) ? (C->strides[0] / 4) : N;
    const float *pa = (const float *)A->data;
    const float *pb = (const float *)B->data;
    float *pc = (float *)C->data;

    for (int64_t m = 0; m < M; m++) {
        const float *arow = pa + m * lda;
        float *crow = pc + m * ldc;
        int64_t n = 0;
        for (; n + 8 <= N; n += 8) {
            __m256 vc = _mm256_setzero_ps();
            for (int64_t k = 0; k < K; k++) {
                __m256 va = _mm256_set1_ps(arow[k]);
                __m256 vb = _mm256_loadu_ps(pb + k * ldb + n);
                vc = _mm256_fmadd_ps(va, vb, vc);
            }
            _mm256_storeu_ps(crow + n, vc);
        }
        for (; n < N; n++) {
            float acc = 0.0f;
            for (int64_t k = 0; k < K; k++) {
                acc = fmaf(arow[k], pb[k * ldb + n], acc);
            }
            crow[n] = acc;
        }
    }
}

static void avx2_rmsnorm_f32(const zcc_tensor_desc_t *X, const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y, float eps) {
    int64_t rows = (X->rank == 1U) ? 1 : X->shape[0];
    int64_t dim  = (X->rank == 1U) ? X->shape[0] : X->shape[1];
    int64_t ldx  = (X->rank == 2U && X->strides[0] > 0) ? (X->strides[0] / 4) : dim;
    int64_t ldy  = (Y->rank == 2U && Y->strides[0] > 0) ? (Y->strides[0] / 4) : dim;
    const float *px = (const float *)X->data;
    const float *pw = (const float *)W->data;
    float *py = (float *)Y->data;

    for (int64_t r = 0; r < rows; r++) {
        const float *xr = px + r * ldx;
        float *yr = py + r * ldy;
        __m256 ss_v = _mm256_setzero_ps();
        int64_t j = 0;
        for (; j + 8 <= dim; j += 8) {
            __m256 vx = _mm256_loadu_ps(xr + j);
            ss_v = _mm256_fmadd_ps(vx, vx, ss_v);
        }
        float ss = hsum256_ps(ss_v);
        for (; j < dim; j++) ss = fmaf(xr[j], xr[j], ss);

        float inv_rms = 1.0f / sqrtf(ss / (float)dim + eps);
        __m256 v_inv = _mm256_set1_ps(inv_rms);
        j = 0;
        for (; j + 8 <= dim; j += 8) {
            __m256 vx = _mm256_loadu_ps(xr + j);
            __m256 vw = _mm256_loadu_ps(pw + j);
            _mm256_storeu_ps(yr + j, _mm256_mul_ps(_mm256_mul_ps(vx, v_inv), vw));
        }
        for (; j < dim; j++) yr[j] = (xr[j] * inv_rms) * pw[j];
    }
}

static void avx2_softmax_f32(const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y) {
    int64_t rows = (X->rank == 1U) ? 1 : X->shape[0];
    int64_t dim  = (X->rank == 1U) ? X->shape[0] : X->shape[1];
    int64_t ldx  = (X->rank == 2U && X->strides[0] > 0) ? (X->strides[0] / 4) : dim;
    int64_t ldy  = (Y->rank == 2U && Y->strides[0] > 0) ? (Y->strides[0] / 4) : dim;
    const float *px = (const float *)X->data;
    float *py = (float *)Y->data;

    for (int64_t r = 0; r < rows; r++) {
        const float *xr = px + r * ldx;
        float *yr = py + r * ldy;
        float m = xr[0];
        int has_nan = (m != m) ? 1 : 0;
        if (!has_nan && dim >= 8) {
            __m256 vm = _mm256_loadu_ps(xr);
            int64_t j = 8;
            for (; j + 8 <= dim; j += 8) {
                vm = _mm256_max_ps(vm, _mm256_loadu_ps(xr + j));
            }
            m = hmax256_ps(vm);
            for (int64_t k = 0; k < dim; k++) {
                float v = xr[k];
                if (v != v) { m = v; has_nan = 1; break; }
            }
            if (!has_nan) {
                for (; j < dim; j++) {
                    if (xr[j] > m) m = xr[j];
                }
            }
        } else {
            for (int64_t j = 1; j < dim; j++) {
                float v = xr[j];
                if ((v != v) || ((m == m) && v > m)) m = v;
            }
        }

        float sum = 0.0f;
        for (int64_t j = 0; j < dim; j++) {
            float ev = expf(xr[j] - m);
            yr[j] = ev;
            sum += ev;
        }
        float inv_s = 1.0f / sum;
        __m256 v_invs = _mm256_set1_ps(inv_s);
        int64_t j = 0;
        for (; j + 8 <= dim; j += 8) {
            __m256 vy = _mm256_loadu_ps(yr + j);
            _mm256_storeu_ps(yr + j, _mm256_mul_ps(vy, v_invs));
        }
        for (; j < dim; j++) yr[j] = yr[j] * inv_s;
    }
}

static void avx2_dequant_q8_0(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t nb = K / 32;
    int64_t ldy = (Y->rank == 2U && Y->strides[0] > 0) ? (Y->strides[0] / 4) : K;
    const uint8_t *base = (const uint8_t *)W->data;
    float *py = (float *)Y->data;

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 34ULL;
        float *yrow = py + row * ldy;
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 34ULL;
            __m256 vd = _mm256_set1_ps(f16_to_f32_avx2_scalar(ld_le16(blk)));
            for (int c = 0; c < 32; c += 8) {
                __m128i qi8 = _mm_loadl_epi64((const __m128i *)(blk + 2 + c));
                __m256i qi32 = _mm256_cvtepi8_epi32(qi8);
                __m256 qf = _mm256_cvtepi32_ps(qi32);
                _mm256_storeu_ps(yrow + b * 32 + c, _mm256_mul_ps(vd, qf));
            }
        }
    }
}

static void avx2_matvec_q8_0(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    const float *px = (const float *)X->data;
    float *py = (float *)Y->data;

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 34ULL;
        __m256 acc = _mm256_setzero_ps();
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 34ULL;
            __m256 vd = _mm256_set1_ps(f16_to_f32_avx2_scalar(ld_le16(blk)));
            const float *xb = px + b * 32;
            for (int c = 0; c < 32; c += 8) {
                __m128i qi8 = _mm_loadl_epi64((const __m128i *)(blk + 2 + c));
                __m256 qf = _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(qi8));
                __m256 vw = _mm256_mul_ps(vd, qf);
                __m256 vx = _mm256_loadu_ps(xb + c);
                acc = _mm256_fmadd_ps(vw, vx, acc);
            }
        }
        py[row] = hsum256_ps(acc);
    }
}

static void avx2_dequant_q4_0(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t nb = K / 32;
    int64_t ldy = (Y->rank == 2U && Y->strides[0] > 0) ? (Y->strides[0] / 4) : K;
    const uint8_t *base = (const uint8_t *)W->data;
    float *py = (float *)Y->data;
    const __m256i mask_lo = _mm256_set1_epi32(0x0F);
    const __m256i bias8   = _mm256_set1_epi32(8);

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 18ULL;
        float *yrow = py + row * ldy;
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 18ULL;
            __m256 vd = _mm256_set1_ps(f16_to_f32_avx2_scalar(ld_le16(blk)));
            for (int c = 0; c < 16; c += 8) {
                __m128i qb8 = _mm_loadl_epi64((const __m128i *)(blk + 2 + c));
                __m256i q32 = _mm256_cvtepu8_epi32(qb8);
                __m256i lo32 = _mm256_sub_epi32(_mm256_and_si256(q32, mask_lo), bias8);
                __m256i hi32 = _mm256_sub_epi32(_mm256_srli_epi32(q32, 4), bias8);
                _mm256_storeu_ps(yrow + b * 32 + c, _mm256_mul_ps(vd, _mm256_cvtepi32_ps(lo32)));
                _mm256_storeu_ps(yrow + b * 32 + 16 + c, _mm256_mul_ps(vd, _mm256_cvtepi32_ps(hi32)));
            }
        }
    }
}

static void avx2_matvec_q4_0(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    const float *px = (const float *)X->data;
    float *py = (float *)Y->data;
    const __m256i mask_lo = _mm256_set1_epi32(0x0F);
    const __m256i bias8   = _mm256_set1_epi32(8);

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 18ULL;
        __m256 acc = _mm256_setzero_ps();
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 18ULL;
            __m256 vd = _mm256_set1_ps(f16_to_f32_avx2_scalar(ld_le16(blk)));
            const float *xb = px + b * 32;
            for (int c = 0; c < 16; c += 8) {
                __m128i qb8 = _mm_loadl_epi64((const __m128i *)(blk + 2 + c));
                __m256i q32 = _mm256_cvtepu8_epi32(qb8);
                __m256i lo32 = _mm256_sub_epi32(_mm256_and_si256(q32, mask_lo), bias8);
                __m256i hi32 = _mm256_sub_epi32(_mm256_srli_epi32(q32, 4), bias8);
                __m256 w0 = _mm256_mul_ps(vd, _mm256_cvtepi32_ps(lo32));
                __m256 w1 = _mm256_mul_ps(vd, _mm256_cvtepi32_ps(hi32));
                acc = _mm256_fmadd_ps(w0, _mm256_loadu_ps(xb + c), acc);
                acc = _mm256_fmadd_ps(w1, _mm256_loadu_ps(xb + 16 + c), acc);
            }
        }
        py[row] = hsum256_ps(acc);
    }
}

static void avx2_dequant_q4_k(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t sb_per_row = K / 256;
    int64_t ldy = (Y->rank == 2U && Y->strides[0] > 0) ? (Y->strides[0] / 4) : K;
    const uint8_t *base = (const uint8_t *)W->data;
    float *py = (float *)Y->data;
    const __m256i mask15 = _mm256_set1_epi32(15);

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)sb_per_row * 144ULL;
        float *yrow = py + row * ldy;
        for (int64_t sb = 0; sb < sb_per_row; sb++) {
            const uint8_t *blk = wrow + (uint64_t)sb * 144ULL;
            float d    = f16_to_f32_avx2_scalar(ld_le16(blk));
            float dmin = f16_to_f32_avx2_scalar(ld_le16(blk + 2));
            const uint8_t *scales = blk + 4;
            for (uint32_t c = 0U; c < 4U; c++) {
                __m256 ds0 = _mm256_set1_ps(d * (float)q4k_sc(scales, c * 2U));
                __m256 dm0 = _mm256_set1_ps(-(dmin * (float)q4k_mn(scales, c * 2U)));
                __m256 ds1 = _mm256_set1_ps(d * (float)q4k_sc(scales, c * 2U + 1U));
                __m256 dm1 = _mm256_set1_ps(-(dmin * (float)q4k_mn(scales, c * 2U + 1U)));
                const uint8_t *qc = blk + 16U + c * 32U;
                float *yo = yrow + sb * 256 + c * 64U;
                for (int l = 0; l < 32; l += 8) {
                    __m128i qb8 = _mm_loadl_epi64((const __m128i *)(qc + l));
                    __m256i q32 = _mm256_cvtepu8_epi32(qb8);
                    __m256 q0 = _mm256_cvtepi32_ps(_mm256_and_si256(q32, mask15));
                    __m256 q1 = _mm256_cvtepi32_ps(_mm256_srli_epi32(q32, 4));
                    _mm256_storeu_ps(yo + l, _mm256_fmadd_ps(ds0, q0, dm0));
                    _mm256_storeu_ps(yo + 32 + l, _mm256_fmadd_ps(ds1, q1, dm1));
                }
            }
        }
    }
}

static void avx2_matvec_q4_k(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t sb_per_row = K / 256;
    const uint8_t *base = (const uint8_t *)W->data;
    const float *px = (const float *)X->data;
    float *py = (float *)Y->data;
    const __m256i mask15 = _mm256_set1_epi32(15);

    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)sb_per_row * 144ULL;
        __m256 acc = _mm256_setzero_ps();
        for (int64_t sb = 0; sb < sb_per_row; sb++) {
            const uint8_t *blk = wrow + (uint64_t)sb * 144ULL;
            float d    = f16_to_f32_avx2_scalar(ld_le16(blk));
            float dmin = f16_to_f32_avx2_scalar(ld_le16(blk + 2));
            const uint8_t *scales = blk + 4;
            const float *xsb = px + sb * 256;
            for (uint32_t c = 0U; c < 4U; c++) {
                __m256 ds0 = _mm256_set1_ps(d * (float)q4k_sc(scales, c * 2U));
                __m256 dm0 = _mm256_set1_ps(-(dmin * (float)q4k_mn(scales, c * 2U)));
                __m256 ds1 = _mm256_set1_ps(d * (float)q4k_sc(scales, c * 2U + 1U));
                __m256 dm1 = _mm256_set1_ps(-(dmin * (float)q4k_mn(scales, c * 2U + 1U)));
                const uint8_t *qc = blk + 16U + c * 32U;
                const float *xc = xsb + c * 64U;
                for (int l = 0; l < 32; l += 8) {
                    __m128i qb8 = _mm_loadl_epi64((const __m128i *)(qc + l));
                    __m256i q32 = _mm256_cvtepu8_epi32(qb8);
                    __m256 q0 = _mm256_cvtepi32_ps(_mm256_and_si256(q32, mask15));
                    __m256 q1 = _mm256_cvtepi32_ps(_mm256_srli_epi32(q32, 4));
                    __m256 w0 = _mm256_fmadd_ps(ds0, q0, dm0);
                    __m256 w1 = _mm256_fmadd_ps(ds1, q1, dm1);
                    acc = _mm256_fmadd_ps(w0, _mm256_loadu_ps(xc + l), acc);
                    acc = _mm256_fmadd_ps(w1, _mm256_loadu_ps(xc + 32 + l), acc);
                }
            }
        }
        py[row] = hsum256_ps(acc);
    }
}

zcc_status_t zcc_avx2_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
) {
    (void)num_inputs;
    /* Defense-in-depth: re-verify live CPUID + OSXSAVE + XCR0 + FMA at AVX2 boundary */
    zcc_cpu_caps_t live_caps;
    if (zcc_probe_cpu_caps(&live_caps) != ZCC_STATUS_OK || !zcc_cpu_has_avx2_fma(&live_caps)) {
        if (err_buf && err_cap > 0U) {
            snprintf(err_buf, err_cap, "AVX2_FMA blocked: CPUID+OSXSAVE+XCR0+FMA check failed");
        }
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    if (opts && opts->accum_policy != ZCC_ACCUM_FP32_FMA) {
        if (err_buf && err_cap > 0U) {
            snprintf(err_buf, err_cap, "AVX2_FMA backend supports ZCC_ACCUM_FP32_FMA only");
        }
        return ZCC_STATUS_ERR_INVALID_ARG;
    }

    /* Fall back cleanly to scalar helper when inner dimension has non-contiguous element stride */
    for (uint32_t i = 0U; i < num_inputs; i++) {
        if (inputs[i].dtype == ZCC_DTYPE_F32 && !is_contig_1d_or_nd(&inputs[i], 4ULL)) {
            int row_strided_only = (inputs[i].rank == 2U && (inputs[i].strides[1] == 0 || inputs[i].strides[1] == 4) &&
                                    (inputs[i].strides[0] % 4 == 0));
            if (!(op == ZCC_OP_GEMM_F32 || op == ZCC_OP_RMSNORM_F32 || op == ZCC_OP_SOFTMAX_F32) || !row_strided_only) {
                return zcc_scalar_exec(op, inputs, num_inputs, output, opts, err_buf, err_cap);
            }
        }
    }
    if (output->dtype == ZCC_DTYPE_F32 && !is_contig_1d_or_nd(output, 4ULL)) {
        int row_strided_only = (output->rank == 2U && (output->strides[1] == 0 || output->strides[1] == 4) &&
                                (output->strides[0] % 4 == 0));
        if (!(op == ZCC_OP_GEMM_F32 || op == ZCC_OP_RMSNORM_F32 || op == ZCC_OP_SOFTMAX_F32 ||
              op == ZCC_OP_DEQUANT_Q8_0 || op == ZCC_OP_DEQUANT_Q4_0 || op == ZCC_OP_DEQUANT_Q4_K) || !row_strided_only) {
            return zcc_scalar_exec(op, inputs, num_inputs, output, opts, err_buf, err_cap);
        }
    }

    float eps = (opts && opts->rmsnorm_eps > 0.0f) ? opts->rmsnorm_eps : 1e-5f;

    switch (op) {
        case ZCC_OP_VEC_ADD_F32:
            avx2_vec_add((const float *)inputs[0].data, (const float *)inputs[1].data, (float *)output->data, total_elems_avx2(&inputs[0]));
            return ZCC_STATUS_OK;
        case ZCC_OP_VEC_MUL_F32:
            avx2_vec_mul((const float *)inputs[0].data, (const float *)inputs[1].data, (float *)output->data, total_elems_avx2(&inputs[0]));
            return ZCC_STATUS_OK;
        case ZCC_OP_VEC_FMA_F32:
            avx2_vec_fma((const float *)inputs[0].data, (const float *)inputs[1].data, (const float *)inputs[2].data, (float *)output->data, total_elems_avx2(&inputs[0]));
            return ZCC_STATUS_OK;
        case ZCC_OP_DOT_F32:
            ((float *)output->data)[0] = avx2_dot_f32((const float *)inputs[0].data, (const float *)inputs[1].data, total_elems_avx2(&inputs[0]));
            return ZCC_STATUS_OK;
        case ZCC_OP_GEMM_F32:
            avx2_gemm_f32(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_RMSNORM_F32:
            avx2_rmsnorm_f32(&inputs[0], &inputs[1], output, eps);
            return ZCC_STATUS_OK;
        case ZCC_OP_SOFTMAX_F32:
            avx2_softmax_f32(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q8_0:
            avx2_dequant_q8_0(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q8_0:
            avx2_matvec_q8_0(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q4_0:
            avx2_dequant_q4_0(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q4_0:
            avx2_matvec_q4_0(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q4_K:
            avx2_dequant_q4_k(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q4_K:
            avx2_matvec_q4_k(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        default:
            return ZCC_STATUS_ERR_INVALID_OP;
    }
}
