/* SPDX-License-Identifier: MIT */
#include "zcc_compute.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern float fmaf(float a, float b, float c);

static uint32_t read_u32_bits(float v) {
    uint32_t u = 0U;
    memcpy(&u, &v, sizeof(u));
    return u;
}

static float u32_bits_to_f32(uint32_t u) {
    float v = 0.0f;
    memcpy(&v, &u, sizeof(v));
    return v;
}

static float t21_f16_to_f32(uint16_t h) {
    uint32_t sign = ((uint32_t)(h & 0x8000U)) << 16;
    uint32_t exp  = (uint32_t)((h >> 10) & 0x1FU);
    uint32_t mant = (uint32_t)(h & 0x03FFU);
    if (exp == 0U) {
        if (mant == 0U) {
            return u32_bits_to_f32(sign);
        }
        uint32_t e = 113U;
        while ((mant & 0x0400U) == 0U) {
            mant <<= 1;
            e--;
        }
        mant &= 0x03FFU;
        return u32_bits_to_f32(sign | (e << 23) | (mant << 13));
    }
    if (exp == 31U) {
        if (mant == 0U) {
            return u32_bits_to_f32(sign | 0x7F800000U);
        }
        return u32_bits_to_f32(sign | 0x7FC00000U | (mant << 13));
    }
    uint32_t e = exp + 112U;
    return u32_bits_to_f32(sign | (e << 23) | (mant << 13));
}

static uint16_t load_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t q4k_scale_at(const uint8_t *scales, uint32_t j) {
    if (j < 4U) {
        return (uint32_t)(scales[j] & 63U);
    }
    return (uint32_t)((scales[j + 4U] & 15U) | ((scales[j - 4U] >> 6) << 4));
}

static uint32_t q4k_min_at(const uint8_t *scales, uint32_t j) {
    if (j < 4U) {
        return (uint32_t)(scales[j + 4U] & 63U);
    }
    return (uint32_t)((scales[j + 4U] >> 4) | ((scales[j] >> 6) << 4));
}

static int64_t total_elems_desc(const zcc_tensor_desc_t *t) {
    int64_t n = 1;
    for (uint32_t d = 0U; d < t->rank; d++) {
        n *= t->shape[d];
    }
    return n;
}

static uint64_t linear_byte_offset(const zcc_tensor_desc_t *t, int64_t idx, uint64_t elem_size) {
    if (t->rank == 1U) {
        int64_t s0 = (t->strides[0] > 0) ? t->strides[0] : (int64_t)elem_size;
        return (uint64_t)idx * (uint64_t)s0;
    }
    uint64_t off = 0ULL;
    int64_t rem = idx;
    uint64_t expected_stride = elem_size;
    for (int d = (int)t->rank - 1; d >= 0; d--) {
        int64_t dim = t->shape[d];
        int64_t coord = rem % dim;
        rem /= dim;
        uint64_t st = (t->strides[d] > 0) ? (uint64_t)t->strides[d] : expected_stride;
        off += (uint64_t)coord * st;
        expected_stride = st * (uint64_t)dim;
    }
    return off;
}

static int is_contiguous_dense(const zcc_tensor_desc_t *t, uint64_t elem_size) {
    uint64_t exp_st = elem_size;
    for (int d = (int)t->rank - 1; d >= 0; d--) {
        int64_t st = t->strides[d];
        if (st != 0 && (uint64_t)st != exp_st) return 0;
        exp_st *= (uint64_t)t->shape[d];
    }
    return 1;
}

static float read_f32_at(const zcc_tensor_desc_t *t, int64_t idx) {
    float v = 0.0f;
    uint64_t off = linear_byte_offset(t, idx, 4ULL);
    memcpy(&v, (const uint8_t *)t->data + off, sizeof(v));
    return v;
}

static void write_f32_at(zcc_tensor_desc_t *t, int64_t idx, float v) {
    uint64_t off = linear_byte_offset(t, idx, 4ULL);
    memcpy((uint8_t *)t->data + off, &v, sizeof(v));
}

static float read_f32_2d(const zcc_tensor_desc_t *t, int64_t r, int64_t c) {
    uint64_t s1 = (t->strides[1] > 0) ? (uint64_t)t->strides[1] : 4ULL;
    uint64_t s0 = (t->strides[0] > 0) ? (uint64_t)t->strides[0] : ((uint64_t)t->shape[1] * s1);
    float v = 0.0f;
    memcpy(&v, (const uint8_t *)t->data + (uint64_t)r * s0 + (uint64_t)c * s1, sizeof(v));
    return v;
}

static void write_f32_2d(zcc_tensor_desc_t *t, int64_t r, int64_t c, float v) {
    uint64_t s1 = (t->strides[1] > 0) ? (uint64_t)t->strides[1] : 4ULL;
    uint64_t s0 = (t->strides[0] > 0) ? (uint64_t)t->strides[0] : ((uint64_t)t->shape[1] * s1);
    memcpy((uint8_t *)t->data + (uint64_t)r * s0 + (uint64_t)c * s1, &v, sizeof(v));
}

static void exec_vec_add(const zcc_tensor_desc_t *a, const zcc_tensor_desc_t *b, zcc_tensor_desc_t *out) {
    int64_t n = total_elems_desc(a);
    if (is_contiguous_dense(a, 4ULL) && is_contiguous_dense(b, 4ULL) && is_contiguous_dense(out, 4ULL)) {
        const float *pa = (const float *)a->data;
        const float *pb = (const float *)b->data;
        float *po = (float *)out->data;
        for (int64_t i = 0; i < n; i++) {
            po[i] = pa[i] + pb[i];
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            write_f32_at(out, i, read_f32_at(a, i) + read_f32_at(b, i));
        }
    }
}

static void exec_vec_mul(const zcc_tensor_desc_t *a, const zcc_tensor_desc_t *b, zcc_tensor_desc_t *out) {
    int64_t n = total_elems_desc(a);
    if (is_contiguous_dense(a, 4ULL) && is_contiguous_dense(b, 4ULL) && is_contiguous_dense(out, 4ULL)) {
        const float *pa = (const float *)a->data;
        const float *pb = (const float *)b->data;
        float *po = (float *)out->data;
        for (int64_t i = 0; i < n; i++) {
            po[i] = pa[i] * pb[i];
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            write_f32_at(out, i, read_f32_at(a, i) * read_f32_at(b, i));
        }
    }
}

static void exec_vec_fma(const zcc_tensor_desc_t *a, const zcc_tensor_desc_t *b, const zcc_tensor_desc_t *c, zcc_tensor_desc_t *out, uint32_t accum) {
    int64_t n = total_elems_desc(a);
    int use_strict = (accum == ZCC_ACCUM_STRICT_FP32);
    int use_fp64   = (accum == ZCC_ACCUM_FP64_REF);
    if (is_contiguous_dense(a, 4ULL) && is_contiguous_dense(b, 4ULL) &&
        is_contiguous_dense(c, 4ULL) && is_contiguous_dense(out, 4ULL)) {
        const float *pa = (const float *)a->data;
        const float *pb = (const float *)b->data;
        const float *pc = (const float *)c->data;
        float *po = (float *)out->data;
        for (int64_t i = 0; i < n; i++) {
            if (use_fp64) {
                po[i] = (float)((double)pa[i] * (double)pb[i] + (double)pc[i]);
            } else if (use_strict) {
                float prod = pa[i] * pb[i];
                po[i] = prod + pc[i];
            } else {
                po[i] = fmaf(pa[i], pb[i], pc[i]);
            }
        }
    } else {
        for (int64_t i = 0; i < n; i++) {
            float va = read_f32_at(a, i);
            float vb = read_f32_at(b, i);
            float vc = read_f32_at(c, i);
            float res;
            if (use_fp64) {
                res = (float)((double)va * (double)vb + (double)vc);
            } else if (use_strict) {
                float prod = va * vb;
                res = prod + vc;
            } else {
                res = fmaf(va, vb, vc);
            }
            write_f32_at(out, i, res);
        }
    }
}

static void exec_dot(const zcc_tensor_desc_t *a, const zcc_tensor_desc_t *b, zcc_tensor_desc_t *out, uint32_t accum) {
    int64_t n = total_elems_desc(a);
    float out_val = 0.0f;
    if (accum == ZCC_ACCUM_FP64_REF) {
        double acc = 0.0;
        for (int64_t i = 0; i < n; i++) {
            acc += (double)read_f32_at(a, i) * (double)read_f32_at(b, i);
        }
        out_val = (float)acc;
    } else if (accum == ZCC_ACCUM_STRICT_FP32) {
        float acc = 0.0f;
        for (int64_t i = 0; i < n; i++) {
            float p = read_f32_at(a, i) * read_f32_at(b, i);
            acc = acc + p;
        }
        out_val = acc;
    } else {
        float acc = 0.0f;
        for (int64_t i = 0; i < n; i++) {
            acc = fmaf(read_f32_at(a, i), read_f32_at(b, i), acc);
        }
        out_val = acc;
    }
    write_f32_at(out, 0, out_val);
}

static void exec_gemm(const zcc_tensor_desc_t *A, const zcc_tensor_desc_t *B, zcc_tensor_desc_t *C, uint32_t accum) {
    int64_t M = A->shape[0];
    int64_t K = A->shape[1];
    int64_t N = B->shape[1];
    int contig = (is_contiguous_dense(A, 4ULL) && is_contiguous_dense(B, 4ULL) && is_contiguous_dense(C, 4ULL));
    if (contig && accum == ZCC_ACCUM_FP64_REF) {
        const float *pa = (const float *)A->data;
        const float *pb = (const float *)B->data;
        float *pc = (float *)C->data;
        for (int64_t m = 0; m < M; m++) {
            for (int64_t n = 0; n < N; n++) {
                double acc = 0.0;
                for (int64_t k = 0; k < K; k++) {
                    acc += (double)pa[m * K + k] * (double)pb[k * N + n];
                }
                pc[m * N + n] = (float)acc;
            }
        }
        return;
    }
    for (int64_t m = 0; m < M; m++) {
        for (int64_t n = 0; n < N; n++) {
            if (accum == ZCC_ACCUM_FP64_REF) {
                double acc = 0.0;
                for (int64_t k = 0; k < K; k++) {
                    acc += (double)read_f32_2d(A, m, k) * (double)read_f32_2d(B, k, n);
                }
                write_f32_2d(C, m, n, (float)acc);
            } else if (accum == ZCC_ACCUM_STRICT_FP32) {
                float acc = 0.0f;
                for (int64_t k = 0; k < K; k++) {
                    float p = read_f32_2d(A, m, k) * read_f32_2d(B, k, n);
                    acc = acc + p;
                }
                write_f32_2d(C, m, n, acc);
            } else {
                float acc = 0.0f;
                for (int64_t k = 0; k < K; k++) {
                    acc = fmaf(read_f32_2d(A, m, k), read_f32_2d(B, k, n), acc);
                }
                write_f32_2d(C, m, n, acc);
            }
        }
    }
}

static void exec_rmsnorm(const zcc_tensor_desc_t *X, const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y, float eps, uint32_t accum) {
    int64_t rows = (X->rank == 1U) ? 1 : X->shape[0];
    int64_t dim  = (X->rank == 1U) ? X->shape[0] : X->shape[1];
    for (int64_t r = 0; r < rows; r++) {
        if (accum == ZCC_ACCUM_FP64_REF) {
            double ss = 0.0;
            for (int64_t j = 0; j < dim; j++) {
                double v = (X->rank == 1U) ? (double)read_f32_at(X, j) : (double)read_f32_2d(X, r, j);
                ss += v * v;
            }
            double inv_rms = 1.0 / sqrt(ss / (double)dim + (double)eps);
            for (int64_t j = 0; j < dim; j++) {
                double xv = (X->rank == 1U) ? (double)read_f32_at(X, j) : (double)read_f32_2d(X, r, j);
                double wv = (double)read_f32_at(W, j);
                float res = (float)(xv * inv_rms * wv);
                if (Y->rank == 1U) write_f32_at(Y, j, res);
                else write_f32_2d(Y, r, j, res);
            }
        } else {
            float ss = 0.0f;
            for (int64_t j = 0; j < dim; j++) {
                float v = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
                if (accum == ZCC_ACCUM_STRICT_FP32) {
                    float sq = v * v;
                    ss = ss + sq;
                } else {
                    ss = fmaf(v, v, ss);
                }
            }
            float inv_rms = 1.0f / sqrtf(ss / (float)dim + eps);
            for (int64_t j = 0; j < dim; j++) {
                float xv = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
                float wv = read_f32_at(W, j);
                float res = (xv * inv_rms) * wv;
                if (Y->rank == 1U) write_f32_at(Y, j, res);
                else write_f32_2d(Y, r, j, res);
            }
        }
    }
}

static void exec_softmax(const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y, uint32_t accum) {
    int64_t rows = (X->rank == 1U) ? 1 : X->shape[0];
    int64_t dim  = (X->rank == 1U) ? X->shape[0] : X->shape[1];
    for (int64_t r = 0; r < rows; r++) {
        float m = (X->rank == 1U) ? read_f32_at(X, 0) : read_f32_2d(X, r, 0);
        for (int64_t j = 1; j < dim; j++) {
            float v = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
            if ((v != v) || ((m == m) && v > m)) {
                m = v;
            }
        }
        if (accum == ZCC_ACCUM_FP64_REF) {
            double sum = 0.0;
            for (int64_t j = 0; j < dim; j++) {
                float v = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
                double ev = exp((double)v - (double)m);
                if (Y->rank == 1U) write_f32_at(Y, j, (float)ev);
                else write_f32_2d(Y, r, j, (float)ev);
                sum += ev;
            }
            for (int64_t j = 0; j < dim; j++) {
                float v = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
                float res = (float)(exp((double)v - (double)m) / sum);
                if (Y->rank == 1U) write_f32_at(Y, j, res);
                else write_f32_2d(Y, r, j, res);
            }
        } else {
            float sum = 0.0f;
            for (int64_t j = 0; j < dim; j++) {
                float v = (X->rank == 1U) ? read_f32_at(X, j) : read_f32_2d(X, r, j);
                float ev = expf(v - m);
                if (Y->rank == 1U) write_f32_at(Y, j, ev);
                else write_f32_2d(Y, r, j, ev);
                sum += ev;
            }
            for (int64_t j = 0; j < dim; j++) {
                float ev = (Y->rank == 1U) ? read_f32_at(Y, j) : read_f32_2d(Y, r, j);
                float res = ev / sum;
                if (Y->rank == 1U) write_f32_at(Y, j, res);
                else write_f32_2d(Y, r, j, res);
            }
        }
    }
}

static void exec_dequant_q8_0(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 34ULL;
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 34ULL;
            float d = t21_f16_to_f32(load_le16(blk));
            for (int64_t l = 0; l < 32; l++) {
                int8_t q = (int8_t)blk[2 + l];
                float val = d * (float)q;
                int64_t col = b * 32 + l;
                if (Y->rank == 1U) write_f32_at(Y, col, val);
                else write_f32_2d(Y, row, col, val);
            }
        }
    }
}

static void exec_matvec_q8_0(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y, uint32_t accum) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 34ULL;
        if (accum == ZCC_ACCUM_FP64_REF) {
            double acc = 0.0;
            for (int64_t b = 0; b < nb; b++) {
                const uint8_t *blk = wrow + (uint64_t)b * 34ULL;
                double d = (double)t21_f16_to_f32(load_le16(blk));
                for (int64_t l = 0; l < 32; l++) {
                    int8_t q = (int8_t)blk[2 + l];
                    acc += (d * (double)q) * (double)read_f32_at(X, b * 32 + l);
                }
            }
            write_f32_at(Y, row, (float)acc);
        } else {
            float acc = 0.0f;
            for (int64_t b = 0; b < nb; b++) {
                const uint8_t *blk = wrow + (uint64_t)b * 34ULL;
                float d = t21_f16_to_f32(load_le16(blk));
                for (int64_t l = 0; l < 32; l++) {
                    int8_t q = (int8_t)blk[2 + l];
                    float w = d * (float)q;
                    float xv = read_f32_at(X, b * 32 + l);
                    if (accum == ZCC_ACCUM_STRICT_FP32) {
                        float p = w * xv;
                        acc = acc + p;
                    } else {
                        acc = fmaf(w, xv, acc);
                    }
                }
            }
            write_f32_at(Y, row, acc);
        }
    }
}

static void exec_dequant_q4_0(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 18ULL;
        for (int64_t b = 0; b < nb; b++) {
            const uint8_t *blk = wrow + (uint64_t)b * 18ULL;
            float d = t21_f16_to_f32(load_le16(blk));
            for (int64_t l = 0; l < 16; l++) {
                uint8_t qb = blk[2 + l];
                int q0 = (int)(qb & 0x0FU) - 8;
                int q1 = (int)(qb >> 4) - 8;
                float v0 = d * (float)q0;
                float v1 = d * (float)q1;
                if (Y->rank == 1U) {
                    write_f32_at(Y, b * 32 + l, v0);
                    write_f32_at(Y, b * 32 + l + 16, v1);
                } else {
                    write_f32_2d(Y, row, b * 32 + l, v0);
                    write_f32_2d(Y, row, b * 32 + l + 16, v1);
                }
            }
        }
    }
}

static void exec_matvec_q4_0(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y, uint32_t accum) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t nb = K / 32;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)nb * 18ULL;
        if (accum == ZCC_ACCUM_FP64_REF) {
            double acc = 0.0;
            for (int64_t b = 0; b < nb; b++) {
                const uint8_t *blk = wrow + (uint64_t)b * 18ULL;
                double d = (double)t21_f16_to_f32(load_le16(blk));
                for (int64_t l = 0; l < 16; l++) {
                    uint8_t qb = blk[2 + l];
                    int q0 = (int)(qb & 0x0FU) - 8;
                    int q1 = (int)(qb >> 4) - 8;
                    acc += (d * (double)q0) * (double)read_f32_at(X, b * 32 + l);
                    acc += (d * (double)q1) * (double)read_f32_at(X, b * 32 + l + 16);
                }
            }
            write_f32_at(Y, row, (float)acc);
        } else {
            float acc = 0.0f;
            for (int64_t b = 0; b < nb; b++) {
                const uint8_t *blk = wrow + (uint64_t)b * 18ULL;
                float d = t21_f16_to_f32(load_le16(blk));
                for (int64_t l = 0; l < 16; l++) {
                    uint8_t qb = blk[2 + l];
                    int q0 = (int)(qb & 0x0FU) - 8;
                    int q1 = (int)(qb >> 4) - 8;
                    float w0 = d * (float)q0;
                    float w1 = d * (float)q1;
                    float x0 = read_f32_at(X, b * 32 + l);
                    float x1 = read_f32_at(X, b * 32 + l + 16);
                    if (accum == ZCC_ACCUM_STRICT_FP32) {
                        float p0 = w0 * x0;
                        float p1 = w1 * x1;
                        acc = (acc + p0) + p1;
                    } else {
                        acc = fmaf(w0, x0, acc);
                        acc = fmaf(w1, x1, acc);
                    }
                }
            }
            write_f32_at(Y, row, acc);
        }
    }
}

static void exec_dequant_q4_k(const zcc_tensor_desc_t *W, zcc_tensor_desc_t *Y) {
    int64_t N = (W->rank == 1U) ? 1 : W->shape[0];
    int64_t K = (W->rank == 1U) ? W->shape[0] : W->shape[1];
    int64_t sb_per_row = K / 256;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)sb_per_row * 144ULL;
        for (int64_t sb = 0; sb < sb_per_row; sb++) {
            const uint8_t *blk = wrow + (uint64_t)sb * 144ULL;
            float d    = t21_f16_to_f32(load_le16(blk));
            float dmin = t21_f16_to_f32(load_le16(blk + 2));
            const uint8_t *scales = blk + 4;
            for (uint32_t c = 0U; c < 4U; c++) {
                uint32_t sc0 = q4k_scale_at(scales, c * 2U);
                uint32_t mn0 = q4k_min_at(scales, c * 2U);
                uint32_t sc1 = q4k_scale_at(scales, c * 2U + 1U);
                uint32_t mn1 = q4k_min_at(scales, c * 2U + 1U);
                float ds0 = d * (float)sc0;
                float dm0 = dmin * (float)mn0;
                float ds1 = d * (float)sc1;
                float dm1 = dmin * (float)mn1;
                for (uint32_t l = 0U; l < 32U; l++) {
                    uint8_t qb = blk[16U + c * 32U + l];
                    float v0 = ds0 * (float)(qb & 15U) - dm0;
                    float v1 = ds1 * (float)(qb >> 4) - dm1;
                    int64_t col0 = sb * 256 + (int64_t)(c * 64U + l);
                    int64_t col1 = col0 + 32;
                    if (Y->rank == 1U) {
                        write_f32_at(Y, col0, v0);
                        write_f32_at(Y, col1, v1);
                    } else {
                        write_f32_2d(Y, row, col0, v0);
                        write_f32_2d(Y, row, col1, v1);
                    }
                }
            }
        }
    }
}

static void exec_matvec_q4_k(const zcc_tensor_desc_t *W, const zcc_tensor_desc_t *X, zcc_tensor_desc_t *Y, uint32_t accum) {
    int64_t N = W->shape[0];
    int64_t K = W->shape[1];
    int64_t sb_per_row = K / 256;
    const uint8_t *base = (const uint8_t *)W->data;
    for (int64_t row = 0; row < N; row++) {
        const uint8_t *wrow = base + (uint64_t)row * (uint64_t)sb_per_row * 144ULL;
        if (accum == ZCC_ACCUM_FP64_REF) {
            double acc = 0.0;
            for (int64_t sb = 0; sb < sb_per_row; sb++) {
                const uint8_t *blk = wrow + (uint64_t)sb * 144ULL;
                double d    = (double)t21_f16_to_f32(load_le16(blk));
                double dmin = (double)t21_f16_to_f32(load_le16(blk + 2));
                const uint8_t *scales = blk + 4;
                for (uint32_t c = 0U; c < 4U; c++) {
                    double ds0 = d * (double)q4k_scale_at(scales, c * 2U);
                    double dm0 = dmin * (double)q4k_min_at(scales, c * 2U);
                    double ds1 = d * (double)q4k_scale_at(scales, c * 2U + 1U);
                    double dm1 = dmin * (double)q4k_min_at(scales, c * 2U + 1U);
                    for (uint32_t l = 0U; l < 32U; l++) {
                        uint8_t qb = blk[16U + c * 32U + l];
                        double w0 = ds0 * (double)(qb & 15U) - dm0;
                        double w1 = ds1 * (double)(qb >> 4) - dm1;
                        int64_t x0_idx = sb * 256 + (int64_t)(c * 64U + l);
                        acc += w0 * (double)read_f32_at(X, x0_idx);
                        acc += w1 * (double)read_f32_at(X, x0_idx + 32);
                    }
                }
            }
            write_f32_at(Y, row, (float)acc);
        } else {
            float acc = 0.0f;
            for (int64_t sb = 0; sb < sb_per_row; sb++) {
                const uint8_t *blk = wrow + (uint64_t)sb * 144ULL;
                float d    = t21_f16_to_f32(load_le16(blk));
                float dmin = t21_f16_to_f32(load_le16(blk + 2));
                const uint8_t *scales = blk + 4;
                for (uint32_t c = 0U; c < 4U; c++) {
                    float ds0 = d * (float)q4k_scale_at(scales, c * 2U);
                    float dm0 = -(dmin * (float)q4k_min_at(scales, c * 2U));
                    float ds1 = d * (float)q4k_scale_at(scales, c * 2U + 1U);
                    float dm1 = -(dmin * (float)q4k_min_at(scales, c * 2U + 1U));
                    for (uint32_t l = 0U; l < 32U; l++) {
                        uint8_t qb = blk[16U + c * 32U + l];
                        int64_t x0_idx = sb * 256 + (int64_t)(c * 64U + l);
                        float x0 = read_f32_at(X, x0_idx);
                        float x1 = read_f32_at(X, x0_idx + 32);
                        if (accum == ZCC_ACCUM_STRICT_FP32) {
                            float w0 = ds0 * (float)(qb & 15U) + dm0;
                            float w1 = ds1 * (float)(qb >> 4) + dm1;
                            float p0 = w0 * x0;
                            float p1 = w1 * x1;
                            acc = (acc + p0) + p1;
                        } else {
                            float w0 = fmaf(ds0, (float)(qb & 15U), dm0);
                            float w1 = fmaf(ds1, (float)(qb >> 4), dm1);
                            acc = fmaf(w0, x0, acc);
                            acc = fmaf(w1, x1, acc);
                        }
                    }
                }
            }
            write_f32_at(Y, row, acc);
        }
    }
}

zcc_status_t zcc_scalar_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_exec_options_t *opts,
    char *err_buf,
    size_t err_cap
) {
    (void)num_inputs;
    (void)err_buf;
    (void)err_cap;
    (void)read_u32_bits;
    uint32_t accum = opts ? opts->accum_policy : (uint32_t)ZCC_ACCUM_FP32_FMA;
    float eps = (opts && opts->rmsnorm_eps > 0.0f) ? opts->rmsnorm_eps : 1e-5f;

    switch (op) {
        case ZCC_OP_VEC_ADD_F32:
            exec_vec_add(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_VEC_MUL_F32:
            exec_vec_mul(&inputs[0], &inputs[1], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_VEC_FMA_F32:
            exec_vec_fma(&inputs[0], &inputs[1], &inputs[2], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_DOT_F32:
            exec_dot(&inputs[0], &inputs[1], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_GEMM_F32:
            exec_gemm(&inputs[0], &inputs[1], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_RMSNORM_F32:
            exec_rmsnorm(&inputs[0], &inputs[1], output, eps, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_SOFTMAX_F32:
            exec_softmax(&inputs[0], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q8_0:
            exec_dequant_q8_0(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q8_0:
            exec_matvec_q8_0(&inputs[0], &inputs[1], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q4_0:
            exec_dequant_q4_0(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q4_0:
            exec_matvec_q4_0(&inputs[0], &inputs[1], output, accum);
            return ZCC_STATUS_OK;
        case ZCC_OP_DEQUANT_Q4_K:
            exec_dequant_q4_k(&inputs[0], output);
            return ZCC_STATUS_OK;
        case ZCC_OP_MATVEC_Q4_K:
            exec_matvec_q4_k(&inputs[0], &inputs[1], output, accum);
            return ZCC_STATUS_OK;
        default:
            return ZCC_STATUS_ERR_INVALID_OP;
    }
}
