/*
 * ZKAEDI ZCC // Sovereign C99 Transformer & GGUF GPU Kernels
 * Compiled directly via `./zcc --target=ptx --ptx-arch=sm_120`
 * Zero CUDA SDK, zero nvcc, zero host runtime headers.
 */

unsigned int __ptx_tid_x(void);
unsigned int __ptx_ntid_x(void);
unsigned int __ptx_ctaid_x(void);
unsigned int __ptx_ctaid_y(void);
void __ptx_syncthreads(void);

float __ptx_rsqrtf(float x);
float __ptx_sinf(float x);
float __ptx_cosf(float x);
float __ptx_ex2f(float x);
float __ptx_lg2f(float x);
float __ptx_maxf(float a, float b);
float __ptx_fmaf(float a, float b, float c);

float __ptx_f16_to_f32(unsigned int h);
float __ptx_bf16_to_f32(unsigned int b);
unsigned int __ptx_f32_to_f16(float x);
float __ptx_shfl_down_f32(float val, unsigned int delta);
float __ptx_shfl_idx_f32(float val, unsigned int src_lane);
void __ptx_mma_gemm_tile_f16(
    const unsigned short *A,
    const unsigned short *B,
    float *D,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int row0,
    unsigned int col0
);

static float fast_expf(float x) {
    /* exp(x) = 2^(x * log2(e)) */
    return __ptx_ex2f(x * 1.4426950408889634f);
}

static float warp_sum_f32(float v) {
    v += __ptx_shfl_down_f32(v, 16U);
    v += __ptx_shfl_down_f32(v, 8U);
    v += __ptx_shfl_down_f32(v, 4U);
    v += __ptx_shfl_down_f32(v, 2U);
    v += __ptx_shfl_down_f32(v, 1U);
    return __ptx_shfl_idx_f32(v, 0U);
}

static float warp_max_f32(float v) {
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 16U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 8U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 4U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 2U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 1U));
    return __ptx_shfl_idx_f32(v, 0U);
}

void llm_embed_f32(const float *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        out[i] = table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i];
    }
}

void llm_embed_f16(const unsigned short *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int h = (unsigned int)table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i];
        out[i] = __ptx_f16_to_f32(h);
    }
}

void llm_embed_q8_0(const unsigned char *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int blocks_per_row = dim >> 5;
        unsigned int b = i >> 5;
        unsigned int j = i & 31U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(blocks_per_row * 34U);
        const unsigned char *blk = table + row_off + (unsigned long long)(b * 34U);
        unsigned int h = (unsigned int)blk[0] | ((unsigned int)blk[1] << 8);
        float d = __ptx_f16_to_f32(h);
        int q = (int)blk[2U + j];
        if (q >= 128) q -= 256;
        out[i] = d * (float)q;
    }
}

void llm_rmsnorm_f32(float *out, const float *x, const float *weight, unsigned int dim, float eps) {
    unsigned int lane = __ptx_tid_x() & 31U;
    float ss = 0.0f;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int i = base + lane;
        if (i < dim) {
            float v = x[i];
            ss = __ptx_fmaf(v, v, ss);
        }
    }
    ss = warp_sum_f32(ss);
    float inv_rms = __ptx_rsqrtf(ss / (float)dim + eps);
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int i = base + lane;
        if (i < dim) {
            out[i] = x[i] * inv_rms * weight[i];
        }
    }
}

void llm_f32_to_f16(const float *src, unsigned short *dst, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) {
        dst[i] = (unsigned short)__ptx_f32_to_f16(src[i]);
    }
}

void llm_gemm_tensor_f16(
    const unsigned short *A,
    const unsigned short *B,
    float *D,
    unsigned int M,
    unsigned int N,
    unsigned int K
) {
    unsigned int col0 = __ptx_ctaid_x() * 8U;
    unsigned int row0 = __ptx_ctaid_y() * 16U;
    if (row0 < M && col0 < N) {
        __ptx_mma_gemm_tile_f16(A, B, D, M, N, K, row0, col0);
    }
}

void llm_matvec_f32(const float *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    const float *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int base = 0U; base < K; base += 32U) {
        unsigned int k = base + lane;
        if (k < K) {
            acc = __ptx_fmaf(wrow[k], x[k], acc);
        }
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_matvec_f16(const unsigned short *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    const unsigned short *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int base = 0U; base < K; base += 32U) {
        unsigned int k = base + lane;
        if (k < K) {
            float w = __ptx_f16_to_f32((unsigned int)wrow[k]);
            acc = __ptx_fmaf(w, x[k], acc);
        }
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_matvec_q8_0(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int blocks_per_row = K >> 5;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(blocks_per_row * 34U);
    float acc = 0.0f;
    for (unsigned int b = 0U; b < blocks_per_row; b++) {
        const unsigned char *blk = wrow + (unsigned long long)(b * 34U);
        unsigned int h = (unsigned int)blk[0] | ((unsigned int)blk[1] << 8);
        float d = __ptx_f16_to_f32(h);
        int q = (int)blk[2U + lane];
        if (q >= 128) q -= 256;
        float w = d * (float)q;
        acc = __ptx_fmaf(w, x[b * 32U + lane], acc);
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_rope_f32(
    float *q,
    float *k,
    unsigned int pos,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int h = idx / half;
    unsigned int p = idx % half;
    float *vec = (h < n_heads) ? (q + h * head_dim) : (k + (h - n_heads) * head_dim);
    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    float x0 = vec[2U * p];
    float x1 = vec[2U * p + 1U];
    vec[2U * p] = x0 * c - x1 * s;
    vec[2U * p + 1U] = x0 * s + x1 * c;
}

void llm_kv_store_f32(
    const float *k_cur,
    const float *v_cur,
    float *k_cache,
    float *v_cache,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < kv_dim) {
        unsigned long long dst_off = ((unsigned long long)(layer * max_seq + pos)) * (unsigned long long)kv_dim + (unsigned long long)i;
        k_cache[dst_off] = k_cur[i];
        v_cache[dst_off] = v_cur[i];
    }
}

void llm_attn_gqa_f32(
    const float *q,
    const float *k_cache,
    const float *v_cache,
    float *att_scratch,
    float *out,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    const float *qh = q + h * head_dim;
    float *att = att_scratch + h * max_seq;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    /* Pass 1: scaled dot-product Q_h . K_{kv_h, t} for t = 0..pos */
    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long k_off = ((unsigned long long)(layer * max_seq + t)) * (unsigned long long)kv_dim
                                 + (unsigned long long)(kv_h * head_dim);
        const float *kt = k_cache + k_off;
        float dot = 0.0f;
        for (unsigned int base = 0U; base < head_dim; base += 32U) {
            unsigned int d = base + lane;
            if (d < head_dim) {
                dot = __ptx_fmaf(qh[d], kt[d], dot);
            }
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;
        if (lane == 0U) att[t] = score;
    }
    __ptx_syncthreads();

    /* Pass 2: numerically stable softmax over t = 0..pos */
    float lmax = -1e30f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int t = base + lane;
        if (t <= pos) {
            lmax = __ptx_maxf(lmax, att[t]);
        }
    }
    float maxv = warp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int t = base + lane;
        if (t <= pos) {
            float e = fast_expf(att[t] - maxv);
            att[t] = e;
            lsum += e;
        }
    }
    float sumv = warp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int t = base + lane;
        if (t <= pos) {
            att[t] *= inv_sum;
        }
    }
    __ptx_syncthreads();

    /* Pass 3: weighted sum over V_{kv_h, t} */
    for (unsigned int base = 0U; base < head_dim; base += 32U) {
        unsigned int d = base + lane;
        if (d < head_dim) {
            float acc = 0.0f;
            for (unsigned int t = 0U; t <= pos; t++) {
                unsigned long long v_off = ((unsigned long long)(layer * max_seq + t)) * (unsigned long long)kv_dim
                                         + (unsigned long long)(kv_h * head_dim);
                acc = __ptx_fmaf(att[t], v_cache[v_off + d], acc);
            }
            oh[d] = acc;
        }
    }
}

void llm_swiglu_f32(float *gate, const float *up, unsigned int hidden_dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < hidden_dim) {
        float g = gate[i];
        float silu = g / (1.0f + fast_expf(-g));
        gate[i] = silu * up[i];
    }
}

void llm_add_f32(float *dst, const float *src, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) {
        dst[i] += src[i];
    }
}

void llm_softmax_f32(float *logits, float *probs, unsigned int vocab_size) {
    unsigned int lane = __ptx_tid_x() & 31U;
    float lmax = -1e30f;
    for (unsigned int base = 0U; base < vocab_size; base += 32U) {
        unsigned int i = base + lane;
        if (i < vocab_size) {
            lmax = __ptx_maxf(lmax, logits[i]);
        }
    }
    float maxv = warp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int base = 0U; base < vocab_size; base += 32U) {
        unsigned int i = base + lane;
        if (i < vocab_size) {
            float e = fast_expf(logits[i] - maxv);
            probs[i] = e;
            lsum += e;
        }
    }
    float sumv = warp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    for (unsigned int base = 0U; base < vocab_size; base += 32U) {
        unsigned int i = base + lane;
        if (i < vocab_size) {
            probs[i] *= inv_sum;
        }
    }
}

/* ========================================================================= */
/* Track 9 Extensions: Q4_0 / BF16 / Q4_K / Q6_K + Bias + FlashAttention-2   */
/* ========================================================================= */

static int s8_from_u8(unsigned int b) {
    int v = (int)(b & 255U);
    if (v >= 128) v -= 256;
    return v;
}

static unsigned int q4k_scale_at(const unsigned char *scales, unsigned int j) {
    if (j < 4U) return (unsigned int)(scales[j] & 63U);
    return (unsigned int)((scales[j + 4U] & 15U) | ((scales[j - 4U] >> 6) << 4));
}

static unsigned int q4k_min_at(const unsigned char *scales, unsigned int j) {
    if (j < 4U) return (unsigned int)(scales[j + 4U] & 63U);
    return (unsigned int)((scales[j + 4U] >> 4) | ((scales[j] >> 6) << 4));
}

void llm_embed_q4_0(const unsigned char *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int blocks_per_row = dim >> 5;
        unsigned int b = i >> 5;
        unsigned int j = i & 31U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(blocks_per_row * 18U);
        const unsigned char *blk = table + row_off + (unsigned long long)(b * 18U);
        unsigned int h = (unsigned int)blk[0] | ((unsigned int)blk[1] << 8);
        float d = __ptx_f16_to_f32(h);
        unsigned int qbyte = (unsigned int)blk[2U + (j & 15U)];
        int q = (int)((j < 16U) ? (qbyte & 15U) : (qbyte >> 4)) - 8;
        out[i] = d * (float)q;
    }
}

void llm_embed_bf16(const unsigned short *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int b = (unsigned int)table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i];
        out[i] = __ptx_bf16_to_f32(b);
    }
}

void llm_embed_q4_k(const unsigned char *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int sb_per_row = dim >> 8;
        unsigned int sb = i >> 8;
        unsigned int rem = i & 255U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(sb_per_row * 144U);
        const unsigned char *blk = table + row_off + (unsigned long long)(sb * 144U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float dmin = __ptx_f16_to_f32((unsigned int)blk[2] | ((unsigned int)blk[3] << 8));
        unsigned int c = rem >> 6;
        unsigned int sub = (rem >> 5) & 1U;
        unsigned int l = rem & 31U;
        unsigned int j = (c << 1) | sub;
        unsigned int sc = q4k_scale_at(blk + 4U, j);
        unsigned int mn = q4k_min_at(blk + 4U, j);
        unsigned int qbyte = (unsigned int)blk[16U + (c << 5) + l];
        unsigned int q = sub ? (qbyte >> 4) : (qbyte & 15U);
        out[i] = (d * (float)sc) * (float)q - (dmin * (float)mn);
    }
}

void llm_embed_q6_k(const unsigned char *table, float *out, unsigned int tok, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int sb_per_row = dim >> 8;
        unsigned int sb = i >> 8;
        unsigned int rem = i & 255U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(sb_per_row * 210U);
        const unsigned char *blk = table + row_off + (unsigned long long)(sb * 210U);
        float d = __ptx_f16_to_f32((unsigned int)blk[208] | ((unsigned int)blk[209] << 8));
        unsigned int h = rem >> 7;
        unsigned int rem2 = rem & 127U;
        unsigned int qtr = rem2 >> 5;
        unsigned int l = rem2 & 31U;
        unsigned int is = l >> 4;
        unsigned int ql_off = (h << 6) + (((qtr & 1U) != 0U) ? (l + 32U) : l);
        unsigned int qh_off = 128U + (h << 5) + l;
        unsigned int sc_off = 192U + (h << 3) + is + (qtr << 1);
        unsigned int ql_b = (unsigned int)blk[ql_off];
        unsigned int qh_b = (unsigned int)blk[qh_off];
        unsigned int ql_n = (qtr >= 2U) ? (ql_b >> 4) : (ql_b & 15U);
        unsigned int qh_n = (qh_b >> (qtr << 1)) & 3U;
        int q = (int)(ql_n | (qh_n << 4)) - 32;
        int sc = s8_from_u8((unsigned int)blk[sc_off]);
        out[i] = d * (float)sc * (float)q;
    }
}

void llm_matvec_q4_0(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int blocks_per_row = K >> 5;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(blocks_per_row * 18U);
    unsigned int qs_off = 2U + (lane & 15U);
    float acc = 0.0f;
    for (unsigned int b = 0U; b < blocks_per_row; b++) {
        const unsigned char *blk = wrow + (unsigned long long)(b * 18U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        unsigned int qb = (unsigned int)blk[qs_off];
        int q = (int)((lane < 16U) ? (qb & 15U) : (qb >> 4)) - 8;
        acc = __ptx_fmaf(d * (float)q, x[b * 32U + lane], acc);
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_matvec_bf16(const unsigned short *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    const unsigned short *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int base = 0U; base < K; base += 32U) {
        unsigned int k = base + lane;
        if (k < K) {
            float w = __ptx_bf16_to_f32((unsigned int)wrow[k]);
            acc = __ptx_fmaf(w, x[k], acc);
        }
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_matvec_q4_k(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int sb_per_row = K >> 8;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(sb_per_row * 144U);
    float acc = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const unsigned char *blk = wrow + (unsigned long long)(sb * 144U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float dmin = __ptx_f16_to_f32((unsigned int)blk[2] | ((unsigned int)blk[3] << 8));
        const unsigned char *scales = blk + 4U;
        const float *xsb = x + (sb << 8);
        for (unsigned int c = 0U; c < 4U; c++) {
            unsigned int sc0 = q4k_scale_at(scales, c << 1);
            unsigned int mn0 = q4k_min_at(scales, c << 1);
            unsigned int sc1 = q4k_scale_at(scales, (c << 1) + 1U);
            unsigned int mn1 = q4k_min_at(scales, (c << 1) + 1U);
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
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_matvec_q6_k(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (row >= N) return;
    unsigned int sb_per_row = K >> 8;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(sb_per_row * 210U);
    unsigned int is = lane >> 4;
    float acc = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const unsigned char *blk = wrow + (unsigned long long)(sb * 210U);
        float d = __ptx_f16_to_f32((unsigned int)blk[208] | ((unsigned int)blk[209] << 8));
        for (unsigned int h = 0U; h < 2U; h++) {
            const unsigned char *ql = blk + (h << 6);
            const unsigned char *qh = blk + 128U + (h << 5);
            const unsigned char *sc = blk + 192U + (h << 3);
            unsigned int ql0 = (unsigned int)ql[lane];
            unsigned int ql1 = (unsigned int)ql[lane + 32U];
            unsigned int qh0 = (unsigned int)qh[lane];
            int q1 = (int)((ql0 & 15U) | ((qh0 & 3U) << 4)) - 32;
            int q2 = (int)((ql1 & 15U) | (((qh0 >> 2) & 3U) << 4)) - 32;
            int q3 = (int)((ql0 >> 4) | (((qh0 >> 4) & 3U) << 4)) - 32;
            int q4 = (int)((ql1 >> 4) | (((qh0 >> 6) & 3U) << 4)) - 32;
            float w1 = d * (float)s8_from_u8((unsigned int)sc[is]) * (float)q1;
            float w2 = d * (float)s8_from_u8((unsigned int)sc[is + 2U]) * (float)q2;
            float w3 = d * (float)s8_from_u8((unsigned int)sc[is + 4U]) * (float)q3;
            float w4 = d * (float)s8_from_u8((unsigned int)sc[is + 6U]) * (float)q4;
            const float *xh = x + (sb << 8) + (h << 7) + lane;
            acc = __ptx_fmaf(w1, xh[0], acc);
            acc = __ptx_fmaf(w2, xh[32], acc);
            acc = __ptx_fmaf(w3, xh[64], acc);
            acc = __ptx_fmaf(w4, xh[96], acc);
        }
    }
    acc = warp_sum_f32(acc);
    if (lane == 0U) y[row] = acc;
}

void llm_add_bias_f32(float *dst, const float *bias, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) {
        dst[i] += bias[i];
    }
}

void llm_attn_flash_gqa_f32(
    const float *q,
    const float *k_cache,
    const float *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    const float *qh = q + h * head_dim;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;

    unsigned int chunks = (head_dim + 31U) >> 5;
    if (chunks > 8U) chunks = 8U;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        const float *kt = k_cache + kv_off;
        const float *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                dot = __ptx_fmaf(qh[d], kt[d], dot);
            }
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                acc[c] = __ptx_fmaf(p_tv, vt[d], acc[c] * alpha);
            }
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        unsigned int d = (c << 5) + lane;
        if (d < head_dim) {
            oh[d] = acc[c] * inv_l;
        }
    }
}

/* ========================================================================= */
/* Track 10: FP16/Q8_0 KV Cache, Dequant-to-F16 & Batched Tensor Core Prefill*/
/* ========================================================================= */

void llm_kv_store_f16(
    const float *k_cur,
    const float *v_cur,
    unsigned short *k_cache,
    unsigned short *v_cache,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < kv_dim) {
        unsigned long long dst_off = ((unsigned long long)(layer * max_seq + pos)) * (unsigned long long)kv_dim + (unsigned long long)i;
        k_cache[dst_off] = (unsigned short)__ptx_f32_to_f16(k_cur[i]);
        v_cache[dst_off] = (unsigned short)__ptx_f32_to_f16(v_cur[i]);
    }
}

void llm_kv_store_q8_0(
    const float *k_cur,
    const float *v_cur,
    unsigned char *k_cache,
    unsigned char *v_cache,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int nb_kv = kv_dim >> 5;
    unsigned int bidx = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (bidx >= (nb_kv << 1)) return;
    int is_v = (bidx >= nb_kv) ? 1 : 0;
    unsigned int blk = is_v ? (bidx - nb_kv) : bidx;
    const float *src = is_v ? v_cur : k_cur;
    unsigned char *dst_base = is_v ? v_cache : k_cache;

    float v = src[(blk << 5) + lane];
    float av = (v < 0.0f) ? -v : v;
    float amax = warp_max_f32(av);
    float d = amax / 127.0f;
    unsigned int dh = __ptx_f32_to_f16(d);
    float df = __ptx_f16_to_f32(dh);

    unsigned long long slot_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)pos)
                                * (unsigned long long)(nb_kv * 34U) + (unsigned long long)(blk * 34U);
    unsigned char *dblk = dst_base + slot_off;
    if (lane == 0U) {
        dblk[0] = (unsigned char)(dh & 255U);
        dblk[1] = (unsigned char)((dh >> 8) & 255U);
    }
    int q = 0;
    if (df > 0.0f) {
        float s = v / df;
        q = (s >= 0.0f) ? (int)(s + 0.5f) : (int)(s - 0.5f);
        if (q > 127) q = 127;
        if (q < -127) q = -127;
    }
    dblk[2U + lane] = (unsigned char)(q & 255);
}

void llm_attn_flash_gqa_f16(
    const float *q,
    const unsigned short *k_cache,
    const unsigned short *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    const float *qh = q + h * head_dim;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;

    unsigned int chunks = (head_dim + 31U) >> 5;
    if (chunks > 8U) chunks = 8U;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        const unsigned short *kt = k_cache + kv_off;
        const unsigned short *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                float kf = __ptx_f16_to_f32((unsigned int)kt[d]);
                dot = __ptx_fmaf(qh[d], kf, dot);
            }
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                float vf = __ptx_f16_to_f32((unsigned int)vt[d]);
                acc[c] = __ptx_fmaf(p_tv, vf, acc[c] * alpha);
            }
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        unsigned int d = (c << 5) + lane;
        if (d < head_dim) {
            oh[d] = acc[c] * inv_l;
        }
    }
}

void llm_attn_flash_gqa_q8_0(
    const float *q,
    const unsigned char *k_cache,
    const unsigned char *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int nb_kv = (n_kv_heads * head_dim) >> 5;
    unsigned int row_bytes = nb_kv * 34U;
    unsigned int chunks = head_dim >> 5;
    if (chunks > 8U) chunks = 8U;
    const float *qh = q + h * head_dim;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)row_bytes + (unsigned long long)(kv_h * chunks * 34U);
        const unsigned char *kt = k_cache + kv_off;
        const unsigned char *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            const unsigned char *blk = kt + (c * 34U);
            float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
            float kf = d * (float)s8_from_u8((unsigned int)blk[2U + lane]);
            dot = __ptx_fmaf(qh[(c << 5) + lane], kf, dot);
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            const unsigned char *blk = vt + (c * 34U);
            float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
            float vf = d * (float)s8_from_u8((unsigned int)blk[2U + lane]);
            acc[c] = __ptx_fmaf(p_tv, vf, acc[c] * alpha);
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        oh[(c << 5) + lane] = acc[c] * inv_l;
    }
}

void llm_dequant_f32_to_f16(const float *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned long long idx = (unsigned long long)row * (unsigned long long)K + (unsigned long long)col;
        dst[idx] = (unsigned short)__ptx_f32_to_f16(W[idx]);
    }
}

void llm_dequant_bf16_to_f16(const unsigned short *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned long long idx = (unsigned long long)row * (unsigned long long)K + (unsigned long long)col;
        float v = __ptx_bf16_to_f32((unsigned int)W[idx]);
        dst[idx] = (unsigned short)__ptx_f32_to_f16(v);
    }
}

void llm_dequant_q8_0_to_f16(const unsigned char *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int blocks_per_row = K >> 5;
        unsigned int b = col >> 5;
        unsigned int j = col & 31U;
        unsigned long long row_off = (unsigned long long)row * (unsigned long long)(blocks_per_row * 34U);
        const unsigned char *blk = W + row_off + (unsigned long long)(b * 34U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float v = d * (float)s8_from_u8((unsigned int)blk[2U + j]);
        dst[(unsigned long long)row * (unsigned long long)K + (unsigned long long)col] = (unsigned short)__ptx_f32_to_f16(v);
    }
}

void llm_dequant_q4_0_to_f16(const unsigned char *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int blocks_per_row = K >> 5;
        unsigned int b = col >> 5;
        unsigned int j = col & 31U;
        unsigned long long row_off = (unsigned long long)row * (unsigned long long)(blocks_per_row * 18U);
        const unsigned char *blk = W + row_off + (unsigned long long)(b * 18U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        unsigned int qb = (unsigned int)blk[2U + (j & 15U)];
        int q = (int)((j < 16U) ? (qb & 15U) : (qb >> 4)) - 8;
        float v = d * (float)q;
        dst[(unsigned long long)row * (unsigned long long)K + (unsigned long long)col] = (unsigned short)__ptx_f32_to_f16(v);
    }
}

void llm_dequant_q4_k_to_f16(const unsigned char *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int sb_per_row = K >> 8;
        unsigned int sb = col >> 8;
        unsigned int rem = col & 255U;
        unsigned long long row_off = (unsigned long long)row * (unsigned long long)(sb_per_row * 144U);
        const unsigned char *blk = W + row_off + (unsigned long long)(sb * 144U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        float dmin = __ptx_f16_to_f32((unsigned int)blk[2] | ((unsigned int)blk[3] << 8));
        unsigned int c = rem >> 6;
        unsigned int sub = (rem >> 5) & 1U;
        unsigned int l = rem & 31U;
        unsigned int j = (c << 1) | sub;
        unsigned int sc = q4k_scale_at(blk + 4U, j);
        unsigned int mn = q4k_min_at(blk + 4U, j);
        unsigned int qbyte = (unsigned int)blk[16U + (c << 5) + l];
        unsigned int q = sub ? (qbyte >> 4) : (qbyte & 15U);
        float v = (d * (float)sc) * (float)q - (dmin * (float)mn);
        dst[(unsigned long long)row * (unsigned long long)K + (unsigned long long)col] = (unsigned short)__ptx_f32_to_f16(v);
    }
}

void llm_dequant_q6_k_to_f16(const unsigned char *W, unsigned short *dst, unsigned int N, unsigned int K) {
    unsigned int row = __ptx_ctaid_y();
    unsigned int col = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (row < N && col < K) {
        unsigned int sb_per_row = K >> 8;
        unsigned int sb = col >> 8;
        unsigned int rem = col & 255U;
        unsigned long long row_off = (unsigned long long)row * (unsigned long long)(sb_per_row * 210U);
        const unsigned char *blk = W + row_off + (unsigned long long)(sb * 210U);
        float d = __ptx_f16_to_f32((unsigned int)blk[208] | ((unsigned int)blk[209] << 8));
        unsigned int h = rem >> 7;
        unsigned int rem2 = rem & 127U;
        unsigned int qtr = rem2 >> 5;
        unsigned int l = rem2 & 31U;
        unsigned int is = l >> 4;
        unsigned int ql_off = (h << 6) + (((qtr & 1U) != 0U) ? (l + 32U) : l);
        unsigned int qh_off = 128U + (h << 5) + l;
        unsigned int sc_off = 192U + (h << 3) + is + (qtr << 1);
        unsigned int ql_b = (unsigned int)blk[ql_off];
        unsigned int qh_b = (unsigned int)blk[qh_off];
        unsigned int ql_n = (qtr >= 2U) ? (ql_b >> 4) : (ql_b & 15U);
        unsigned int qh_n = (qh_b >> (qtr << 1)) & 3U;
        int q = (int)(ql_n | (qh_n << 4)) - 32;
        int sc = s8_from_u8((unsigned int)blk[sc_off]);
        float v = d * (float)sc * (float)q;
        dst[(unsigned long long)row * (unsigned long long)K + (unsigned long long)col] = (unsigned short)__ptx_f32_to_f16(v);
    }
}

void llm_scatter_tile_f32(float *dst, const float *src, unsigned int n, unsigned int add_flag) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) {
        dst[i] = add_flag ? (dst[i] + src[i]) : src[i];
    }
}

void llm_rmsnorm_batch_f32(float *out, const float *x, const float *weight, unsigned int dim, float eps) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    const float *xt = x + (unsigned long long)t * (unsigned long long)dim;
    float *ot = out + (unsigned long long)t * (unsigned long long)dim;
    float ss = 0.0f;
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int i = base + lane;
        if (i < dim) {
            float v = xt[i];
            ss = __ptx_fmaf(v, v, ss);
        }
    }
    ss = warp_sum_f32(ss);
    float inv_rms = __ptx_rsqrtf(ss / (float)dim + eps);
    for (unsigned int base = 0U; base < dim; base += 32U) {
        unsigned int i = base + lane;
        if (i < dim) {
            ot[i] = xt[i] * inv_rms * weight[i];
        }
    }
}

void llm_add_bias_batch_f32(float *dst, const float *bias, unsigned int N) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < N) {
        unsigned long long idx = (unsigned long long)t * (unsigned long long)N + (unsigned long long)i;
        dst[idx] += bias[i];
    }
}

void llm_rope_batch_f32(
    float *q,
    float *k,
    unsigned int pos0,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int pos = pos0 + t;
    float *qt = q + (unsigned long long)t * (unsigned long long)(n_heads * head_dim);
    float *kt = k + (unsigned long long)t * (unsigned long long)(n_kv_heads * head_dim);
    unsigned int h = idx / half;
    unsigned int p = idx % half;
    float *vec = (h < n_heads) ? (qt + h * head_dim) : (kt + (h - n_heads) * head_dim);
    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);
    float x0 = vec[2U * p];
    float x1 = vec[2U * p + 1U];
    vec[2U * p] = x0 * c - x1 * s;
    vec[2U * p + 1U] = x0 * s + x1 * c;
}

void llm_kv_store_batch_f32(
    const float *k_cur,
    const float *v_cur,
    float *k_cache,
    float *v_cache,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < kv_dim) {
        unsigned long long src_off = (unsigned long long)t * (unsigned long long)kv_dim + (unsigned long long)i;
        unsigned long long dst_off = ((unsigned long long)(layer * max_seq + pos0 + t)) * (unsigned long long)kv_dim + (unsigned long long)i;
        k_cache[dst_off] = k_cur[src_off];
        v_cache[dst_off] = v_cur[src_off];
    }
}

void llm_kv_store_batch_f16(
    const float *k_cur,
    const float *v_cur,
    unsigned short *k_cache,
    unsigned short *v_cache,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < kv_dim) {
        unsigned long long src_off = (unsigned long long)t * (unsigned long long)kv_dim + (unsigned long long)i;
        unsigned long long dst_off = ((unsigned long long)(layer * max_seq + pos0 + t)) * (unsigned long long)kv_dim + (unsigned long long)i;
        k_cache[dst_off] = (unsigned short)__ptx_f32_to_f16(k_cur[src_off]);
        v_cache[dst_off] = (unsigned short)__ptx_f32_to_f16(v_cur[src_off]);
    }
}

void llm_kv_store_batch_q8_0(
    const float *k_cur,
    const float *v_cur,
    unsigned char *k_cache,
    unsigned char *v_cache,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int kv_dim
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int nb_kv = kv_dim >> 5;
    unsigned int bidx = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (bidx >= (nb_kv << 1)) return;
    int is_v = (bidx >= nb_kv) ? 1 : 0;
    unsigned int blk = is_v ? (bidx - nb_kv) : bidx;
    const float *src = (is_v ? v_cur : k_cur) + (unsigned long long)t * (unsigned long long)kv_dim;
    unsigned char *dst_base = is_v ? v_cache : k_cache;

    float v = src[(blk << 5) + lane];
    float av = (v < 0.0f) ? -v : v;
    float amax = warp_max_f32(av);
    float d = amax / 127.0f;
    unsigned int dh = __ptx_f32_to_f16(d);
    float df = __ptx_f16_to_f32(dh);

    unsigned long long slot_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)(pos0 + t))
                                * (unsigned long long)(nb_kv * 34U) + (unsigned long long)(blk * 34U);
    unsigned char *dblk = dst_base + slot_off;
    if (lane == 0U) {
        dblk[0] = (unsigned char)(dh & 255U);
        dblk[1] = (unsigned char)((dh >> 8) & 255U);
    }
    int q = 0;
    if (df > 0.0f) {
        float s = v / df;
        q = (s >= 0.0f) ? (int)(s + 0.5f) : (int)(s - 0.5f);
        if (q > 127) q = 127;
        if (q < -127) q = -127;
    }
    dblk[2U + lane] = (unsigned char)(q & 255);
}

void llm_attn_flash_gqa_batch_f32(
    const float *q,
    const float *k_cache,
    const float *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int ti = __ptx_ctaid_y();
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int pos = pos0 + ti;
    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    unsigned long long q_off = ((unsigned long long)ti * (unsigned long long)n_heads + (unsigned long long)h) * (unsigned long long)head_dim;
    const float *qh = q + q_off;
    float *oh = out + q_off;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;

    unsigned int chunks = (head_dim + 31U) >> 5;
    if (chunks > 8U) chunks = 8U;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        const float *kt = k_cache + kv_off;
        const float *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                dot = __ptx_fmaf(qh[d], kt[d], dot);
            }
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                acc[c] = __ptx_fmaf(p_tv, vt[d], acc[c] * alpha);
            }
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        unsigned int d = (c << 5) + lane;
        if (d < head_dim) {
            oh[d] = acc[c] * inv_l;
        }
    }
}

void llm_attn_flash_gqa_batch_f16(
    const float *q,
    const unsigned short *k_cache,
    const unsigned short *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int ti = __ptx_ctaid_y();
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int pos = pos0 + ti;
    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    unsigned long long q_off = ((unsigned long long)ti * (unsigned long long)n_heads + (unsigned long long)h) * (unsigned long long)head_dim;
    const float *qh = q + q_off;
    float *oh = out + q_off;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;

    unsigned int chunks = (head_dim + 31U) >> 5;
    if (chunks > 8U) chunks = 8U;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        const unsigned short *kt = k_cache + kv_off;
        const unsigned short *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                float kf = __ptx_f16_to_f32((unsigned int)kt[d]);
                dot = __ptx_fmaf(qh[d], kf, dot);
            }
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            unsigned int d = (c << 5) + lane;
            if (d < head_dim) {
                float vf = __ptx_f16_to_f32((unsigned int)vt[d]);
                acc[c] = __ptx_fmaf(p_tv, vf, acc[c] * alpha);
            }
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        unsigned int d = (c << 5) + lane;
        if (d < head_dim) {
            oh[d] = acc[c] * inv_l;
        }
    }
}

void llm_attn_flash_gqa_batch_q8_0(
    const float *q,
    const unsigned char *k_cache,
    const unsigned char *v_cache,
    float *out,
    unsigned int layer,
    unsigned int pos0,
    unsigned int max_seq,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int ti = __ptx_ctaid_y();
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int pos = pos0 + ti;
    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int nb_kv = (n_kv_heads * head_dim) >> 5;
    unsigned int row_bytes = nb_kv * 34U;
    unsigned int chunks = head_dim >> 5;
    if (chunks > 8U) chunks = 8U;
    unsigned long long q_off = ((unsigned long long)ti * (unsigned long long)n_heads + (unsigned long long)h) * (unsigned long long)head_dim;
    const float *qh = q + q_off;
    float *oh = out + q_off;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    float acc[8];
    for (unsigned int c = 0U; c < 8U; c++) acc[c] = 0.0f;
    float m_max = -1e30f;
    float l_sum = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        unsigned long long kv_off = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)t)
                                  * (unsigned long long)row_bytes + (unsigned long long)(kv_h * chunks * 34U);
        const unsigned char *kt = k_cache + kv_off;
        const unsigned char *vt = v_cache + kv_off;

        float dot = 0.0f;
        for (unsigned int c = 0U; c < chunks; c++) {
            const unsigned char *blk = kt + (c * 34U);
            float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
            float kf = d * (float)s8_from_u8((unsigned int)blk[2U + lane]);
            dot = __ptx_fmaf(qh[(c << 5) + lane], kf, dot);
        }
        float score = warp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = fast_expf(m_max - m_new);
            p_tv = fast_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        for (unsigned int c = 0U; c < chunks; c++) {
            const unsigned char *blk = vt + (c * 34U);
            float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
            float vf = d * (float)s8_from_u8((unsigned int)blk[2U + lane]);
            acc[c] = __ptx_fmaf(p_tv, vf, acc[c] * alpha);
        }
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    for (unsigned int c = 0U; c < chunks; c++) {
        oh[(c << 5) + lane] = acc[c] * inv_l;
    }
}

/* ========================================================================= */
/* Track 11: Qwen2 / Qwen2.5 NeoX Split-Half RoPE Kernels (Generic Mode)     */
/* ========================================================================= */

void llm_rope_neox_f32(
    float *q,
    float *k,
    unsigned int pos,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int h = idx / half;
    unsigned int p = idx % half;
    float *vec = (h < n_heads) ? (q + h * head_dim) : (k + (h - n_heads) * head_dim);
    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    float x0 = vec[p];
    float x1 = vec[p + half];
    vec[p] = x0 * c - x1 * s;
    vec[p + half] = x0 * s + x1 * c;
}

void llm_rope_neox_batch_f32(
    float *q,
    float *k,
    unsigned int pos0,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int pos = pos0 + t;
    float *qt = q + (unsigned long long)t * (unsigned long long)(n_heads * head_dim);
    float *kt = k + (unsigned long long)t * (unsigned long long)(n_kv_heads * head_dim);
    unsigned int h = idx / half;
    unsigned int p = idx % half;
    float *vec = (h < n_heads) ? (qt + h * head_dim) : (kt + (h - n_heads) * head_dim);
    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    float x0 = vec[p];
    float x1 = vec[p + half];
    vec[p] = x0 * c - x1 * s;
    vec[p + half] = x0 * s + x1 * c;
}
