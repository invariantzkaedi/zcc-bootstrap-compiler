/* SPDX-License-Identifier: MIT */
/* Track 21A Blackwell sm_120 compute kernels compiled via ZCC part6_ptx.c (--target=ptx). */

unsigned int __ptx_tid_x(void);
unsigned int __ptx_ntid_x(void);
unsigned int __ptx_ctaid_x(void);
unsigned int __ptx_ctaid_y(void);
float __ptx_sqrtf(float x);
float __ptx_ex2f(float x);
float __ptx_maxf(float a, float b);
float __ptx_fmaf(float a, float b, float c);
float __ptx_f16_to_f32(unsigned int h);
float __ptx_shfl_down_f32(float val, unsigned int delta);
float __ptx_shfl_idx_f32(float val, unsigned int src_lane);

static float t21_fast_expf(float x) {
    return __ptx_ex2f(x * 1.4426950408889634f);
}

static float t21_warp_sum(float v) {
    v += __ptx_shfl_down_f32(v, 16U);
    v += __ptx_shfl_down_f32(v, 8U);
    v += __ptx_shfl_down_f32(v, 4U);
    v += __ptx_shfl_down_f32(v, 2U);
    v += __ptx_shfl_down_f32(v, 1U);
    return __ptx_shfl_idx_f32(v, 0U);
}

static float t21_warp_max(float v) {
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 16U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 8U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 4U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 2U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 1U));
    return __ptx_shfl_idx_f32(v, 0U);
}

static int t21_s8(unsigned int u) {
    u &= 255U;
    return (u >= 128U) ? ((int)u - 256) : (int)u;
}

static unsigned int t21_q4k_scale(const unsigned char *sc, unsigned int j) {
    if (j < 4U) return (unsigned int)(sc[j] & 63U);
    return (unsigned int)((sc[j + 4U] & 15U) | ((sc[j - 4U] >> 6) << 4));
}

static unsigned int t21_q4k_min(const unsigned char *sc, unsigned int j) {
    if (j < 4U) return (unsigned int)(sc[j + 4U] & 63U);
    return (unsigned int)((sc[j + 4U] >> 4) | ((sc[j] >> 6) << 4));
}

void t21_vec_add_f32(const float *a, const float *b, float *out, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) out[i] = a[i] + b[i];
}

void t21_vec_mul_f32(const float *a, const float *b, float *out, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) out[i] = a[i] * b[i];
}

void t21_vec_fma_f32(const float *a, const float *b, const float *c, float *out, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) out[i] = __ptx_fmaf(a[i], b[i], c[i]);
}

void t21_dot_f32(const float *a, const float *b, float *out, unsigned int n) {
    unsigned int lane = __ptx_tid_x() & 31U;
    float acc = 0.0f;
    for (unsigned int base = 0U; base < n; base += 32U) {
        unsigned int i = base + lane;
        if (i < n) {
            acc = __ptx_fmaf(a[i], b[i], acc);
        }
    }
    acc = t21_warp_sum(acc);
    if (lane == 0U) out[0] = acc;
}

void t21_gemm_f32(
    const float *A,
    const float *B,
    float *C,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int lda_elems,
    unsigned int ldb_elems,
    unsigned int ldc_elems
) {
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int row = __ptx_ctaid_y();
    if (row < M && col < N) {
        const float *arow = A + (unsigned long long)row * (unsigned long long)lda_elems;
        float acc = 0.0f;
        for (unsigned int k = 0U; k < K; k++) {
            float av = arow[k];
            float bv = B[(unsigned long long)k * (unsigned long long)ldb_elems + (unsigned long long)col];
            acc = __ptx_fmaf(av, bv, acc);
        }
        C[(unsigned long long)row * (unsigned long long)ldc_elems + (unsigned long long)col] = acc;
    }
}

void t21_rmsnorm_f32(
    const float *X,
    const float *W,
    float *Y,
    unsigned int rows,
    unsigned int dim,
    unsigned int ldx_elems,
    unsigned int ldy_elems,
    float eps
) {
    unsigned int r = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (r >= rows) return;
    const float *xr = X + (unsigned long long)r * (unsigned long long)ldx_elems;
    float *yr = Y + (unsigned long long)r * (unsigned long long)ldy_elems;
    float ss = 0.0f;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int j = base + lane;
        if (j < dim) {
            float v = xr[j];
            ss = __ptx_fmaf(v, v, ss);
        }
    }
    ss = t21_warp_sum(ss);
    float inv_rms = 1.0f / __ptx_sqrtf(ss / (float)dim + eps);
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int j = base + lane;
        if (j < dim) {
            yr[j] = (xr[j] * inv_rms) * W[j];
        }
    }
}

void t21_softmax_f32(
    const float *X,
    float *Y,
    unsigned int rows,
    unsigned int dim,
    unsigned int ldx_elems,
    unsigned int ldy_elems
) {
    unsigned int r = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (r >= rows) return;
    const float *xr = X + (unsigned long long)r * (unsigned long long)ldx_elems;
    float *yr = Y + (unsigned long long)r * (unsigned long long)ldy_elems;

    float local_m = -3.402823466e+38f;
    float nan_flag = 0.0f;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int j = base + lane;
        if (j < dim) {
            float v = xr[j];
            if (v != v) nan_flag = 1.0f;
            else if (v > local_m) local_m = v;
        }
    }
    float nan_sum = t21_warp_sum(nan_flag);
    float m_max = t21_warp_max(local_m);
    if (nan_sum > 0.0f) {
        m_max = 0.0f / 0.0f;
    }

    float local_s = 0.0f;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int j = base + lane;
        if (j < dim) {
            float ev = t21_fast_expf(xr[j] - m_max);
            yr[j] = ev;
            local_s += ev;
        }
    }
    float sum = t21_warp_sum(local_s);
    float inv_s = 1.0f / sum;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int j = base + lane;
        if (j < dim) {
            yr[j] = yr[j] * inv_s;
        }
    }
}

void t21_dequant_q8_0(
    const unsigned char *W,
    float *Y,
    unsigned int N,
    unsigned int K,
    unsigned int ldy_elems
) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int nb = K >> 5;
        unsigned int b = col >> 5;
        unsigned int l = col & 31U;
        unsigned long long off = ((unsigned long long)row * (unsigned long long)nb + (unsigned long long)b) * 34ULL;
        const unsigned char *blk = W + off;
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        int q = t21_s8((unsigned int)blk[2U + l]);
        Y[(unsigned long long)row * (unsigned long long)ldy_elems + (unsigned long long)col] = d * (float)q;
    }
}

void t21_matvec_q8_0(const unsigned char *W, const float *X, float *Y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int nb = K >> 5;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(nb * 34U);
    float acc = 0.0f;
    for (unsigned int b = 0U; b < nb; b++) {
        const unsigned char *blk = wrow + (unsigned long long)(b * 34U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        int q = t21_s8((unsigned int)blk[2U + lane]);
        acc = __ptx_fmaf(d * (float)q, X[(b << 5) + lane], acc);
    }
    acc = t21_warp_sum(acc);
    if (lane == 0U) Y[row] = acc;
}

void t21_dequant_q4_0(
    const unsigned char *W,
    float *Y,
    unsigned int N,
    unsigned int K,
    unsigned int ldy_elems
) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int nb = K >> 5;
        unsigned int b = col >> 5;
        unsigned int l = col & 31U;
        unsigned long long off = ((unsigned long long)row * (unsigned long long)nb + (unsigned long long)b) * 18ULL;
        const unsigned char *blk = W + off;
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        unsigned int qb = (unsigned int)blk[2U + (l & 15U)];
        int q = (int)((l < 16U) ? (qb & 15U) : (qb >> 4)) - 8;
        Y[(unsigned long long)row * (unsigned long long)ldy_elems + (unsigned long long)col] = d * (float)q;
    }
}

void t21_matvec_q4_0(const unsigned char *W, const float *X, float *Y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int nb = K >> 5;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(nb * 18U);
    unsigned int q_off = 2U + (lane & 15U);
    float acc = 0.0f;
    for (unsigned int b = 0U; b < nb; b++) {
        const unsigned char *blk = wrow + (unsigned long long)(b * 18U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        unsigned int qb = (unsigned int)blk[q_off];
        int q = (int)((lane < 16U) ? (qb & 15U) : (qb >> 4)) - 8;
        acc = __ptx_fmaf(d * (float)q, X[(b << 5) + lane], acc);
    }
    acc = t21_warp_sum(acc);
    if (lane == 0U) Y[row] = acc;
}

void t21_dequant_q4_k(
    const unsigned char *W,
    float *Y,
    unsigned int N,
    unsigned int K,
    unsigned int ldy_elems
) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int sb_per_row = K >> 8;
        unsigned int sb = col >> 8;
        unsigned int rem = col & 255U;
        unsigned long long off = ((unsigned long long)row * (unsigned long long)sb_per_row + (unsigned long long)sb) * 144ULL;
        const unsigned char *blk = W + off;
        float d    = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float dmin = __ptx_f16_to_f32((unsigned int)blk[2] | ((unsigned int)blk[3] << 8));
        unsigned int c = rem >> 6;
        unsigned int sub = (rem >> 5) & 1U;
        unsigned int l = rem & 31U;
        unsigned int j = (c << 1) | sub;
        unsigned int sc = t21_q4k_scale(blk + 4U, j);
        unsigned int mn = t21_q4k_min(blk + 4U, j);
        unsigned int qb = (unsigned int)blk[16U + (c << 5) + l];
        unsigned int q = sub ? (qb >> 4) : (qb & 15U);
        Y[(unsigned long long)row * (unsigned long long)ldy_elems + (unsigned long long)col] = (d * (float)sc) * (float)q - (dmin * (float)mn);
    }
}

void t21_matvec_q4_k(const unsigned char *W, const float *X, float *Y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int sb_per_row = K >> 8;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(sb_per_row * 144U);
    float acc = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const unsigned char *blk = wrow + (unsigned long long)(sb * 144U);
        float d    = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float dmin = __ptx_f16_to_f32((unsigned int)blk[2] | ((unsigned int)blk[3] << 8));
        const unsigned char *scales = blk + 4U;
        const float *xsb = X + (sb << 8);
        for (unsigned int c = 0U; c < 4U; c++) {
            unsigned int sc0 = t21_q4k_scale(scales, c << 1);
            unsigned int mn0 = t21_q4k_min(scales, c << 1);
            unsigned int sc1 = t21_q4k_scale(scales, (c << 1) + 1U);
            unsigned int mn1 = t21_q4k_min(scales, (c << 1) + 1U);
            float ds0 = d * (float)sc0;
            float dm0 = -(dmin * (float)mn0);
            float ds1 = d * (float)sc1;
            float dm1 = -(dmin * (float)mn1);
            unsigned int qb = (unsigned int)blk[16U + (c << 5) + lane];
            float w0 = __ptx_fmaf(ds0, (float)(qb & 15U), dm0);
            float w1 = __ptx_fmaf(ds1, (float)(qb >> 4), dm1);
            unsigned int xoff = (c << 6) + lane;
            acc = __ptx_fmaf(w0, xsb[xoff], acc);
            acc = __ptx_fmaf(w1, xsb[xoff + 32U], acc);
        }
    }
    acc = t21_warp_sum(acc);
    if (lane == 0U) Y[row] = acc;
}
