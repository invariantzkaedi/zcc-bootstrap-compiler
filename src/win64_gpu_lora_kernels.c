/*
 * ZKAEDI ZCC // Sovereign Zero-PyTorch GPU LoRA + Autodiff Blackwell Kernels (Track 19)
 * =====================================================================================
 * Compiled directly via `./zcc --target=ptx --ptx-arch=sm_120` (and zcc_sov_stage3.exe).
 * Zero CUDA SDK, zero nvcc, zero PyTorch, zero external runtime headers.
 *
 * Covers full causal Transformer + GQA + NeoX/Llama RoPE + SwiGLU + RMSNorm +
 * Masked Cross-Entropy forward & exact backward autodiff, LoRA adapters (A, B),
 * FP32 reference/fallback matmuls, global gradient norm reduction, and AdamW.
 */

unsigned int __ptx_tid_x(void);
unsigned int __ptx_ntid_x(void);
unsigned int __ptx_ctaid_x(void);
unsigned int __ptx_ctaid_y(void);
void __ptx_syncthreads(void);

float __ptx_sqrtf(float x);
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

static float lora_expf(float x) {
    return __ptx_ex2f(x * 1.4426950408889634f);
}

static float lora_logf(float x) {
    float xc = (x > 1e-20f) ? x : 1e-20f;
    return __ptx_lg2f(xc) * 0.6931471805599453f;
}

static float lwarp_sum_f32(float v) {
    v += __ptx_shfl_down_f32(v, 16U);
    v += __ptx_shfl_down_f32(v, 8U);
    v += __ptx_shfl_down_f32(v, 4U);
    v += __ptx_shfl_down_f32(v, 2U);
    v += __ptx_shfl_down_f32(v, 1U);
    return __ptx_shfl_idx_f32(v, 0U);
}

static float lwarp_max_f32(float v) {
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 16U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 8U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 4U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 2U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 1U));
    return __ptx_shfl_idx_f32(v, 0U);
}

/* 1. Elementwise / Layout / Slice Helpers */

void lora_zero_f32(float *dst, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) dst[i] = 0.0f;
}

void lora_add_f32_out(const float *a, const float *b, float *out, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) out[i] = a[i] + b[i];
}

void lora_add_inplace_f32(float *dst, const float *src, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) dst[i] += src[i];
}

void lora_amax_blocks(const float *x, float *blk_out, unsigned int n) {
    unsigned int cta = __ptx_ctaid_x();
    unsigned int gdim = 64U;
    unsigned int lane = __ptx_tid_x() & 31U;
    float mx = 0.0f;
    for (unsigned int i = cta * 32U + lane; i < n; i += gdim * 32U) {
        float v = x[i];
        float av = (v < 0.0f) ? -v : v;
        mx = __ptx_maxf(mx, av);
    }
    float r = lwarp_max_f32(mx);
    if (lane == 0U) blk_out[cta] = r;
}

void lora_amax_slice_blocks(
    const float *src,
    unsigned int src_stride,
    unsigned int T_real,
    unsigned int col_offset,
    unsigned int cols,
    float *blk_out
) {
    unsigned int cta = __ptx_ctaid_x();
    unsigned int gdim = 64U;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int total = T_real * cols;
    float mx = 0.0f;
    for (unsigned int i = cta * 32U + lane; i < total; i += gdim * 32U) {
        unsigned int t = i / cols;
        unsigned int c = i % cols;
        float v = src[(unsigned long long)t * (unsigned long long)src_stride + (unsigned long long)(col_offset + c)];
        float av = (v < 0.0f) ? -v : v;
        mx = __ptx_maxf(mx, av);
    }
    float r = lwarp_max_f32(mx);
    if (lane == 0U) blk_out[cta] = r;
}

void lora_amax_finalize(const float *blk_in, float *scale_pair) {
    unsigned int lane = __ptx_tid_x() & 31U;
    float mx = lwarp_max_f32(__ptx_maxf(blk_in[lane], blk_in[lane + 32U]));
    if (lane == 0U) {
        if (mx > 1e-25f) {
            scale_pair[0] = 8192.0f / mx;
            scale_pair[1] = mx / 8192.0f;
        } else {
            scale_pair[0] = 0.0f;
            scale_pair[1] = 0.0f;
        }
    }
}

void lora_copy_scaled_f32(
    float *dst,
    const float *src,
    const float *scale_pair,
    unsigned int n,
    unsigned int add_flag
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float v = src[i] * scale_pair[1];
    dst[i] = add_flag ? (dst[i] + v) : v;
}

void lora_copy_cols_scaled_f32(
    const float *src,
    unsigned int src_stride,
    float *dst,
    unsigned int dst_stride,
    unsigned int rows,
    unsigned int col_offset,
    unsigned int cols,
    const float *scale_pair
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = rows * cols;
    if (idx >= total) return;
    unsigned int r = idx / cols;
    unsigned int c = idx % cols;
    float sc = scale_pair[1];
    dst[(unsigned long long)r * (unsigned long long)dst_stride + (unsigned long long)(col_offset + c)] =
        src[(unsigned long long)r * (unsigned long long)src_stride + (unsigned long long)c] * sc;
}

void lora_f32_to_f16_pad(
    const float *src,
    unsigned short *dst,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int K,
    const float *scale_pair
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = T_pad * K;
    if (idx >= total) return;
    unsigned int t = idx / K;
    if (t < T_real) {
        float sc = scale_pair[0];
        dst[idx] = (unsigned short)__ptx_f32_to_f16(src[idx] * sc);
    } else {
        dst[idx] = 0U;
    }
}

void lora_transpose_f16(
    const unsigned short *src,
    unsigned short *dst,
    unsigned int rows,
    unsigned int cols
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = rows * cols;
    if (idx >= total) return;
    unsigned int r = idx / cols;
    unsigned int c = idx % cols;
    dst[(unsigned long long)c * (unsigned long long)rows + (unsigned long long)r] = src[idx];
}

void lora_copy_cols_f32(
    const float *src,
    unsigned int src_stride,
    float *dst,
    unsigned int dst_stride,
    unsigned int rows,
    unsigned int col_offset,
    unsigned int cols,
    unsigned int src_to_dst
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = rows * cols;
    if (idx >= total) return;
    unsigned int r = idx / cols;
    unsigned int c = idx % cols;
    if (src_to_dst) {
        dst[(unsigned long long)r * (unsigned long long)dst_stride + (unsigned long long)(col_offset + c)] =
            src[(unsigned long long)r * (unsigned long long)src_stride + (unsigned long long)c];
    } else {
        dst[(unsigned long long)r * (unsigned long long)dst_stride + (unsigned long long)c] =
            src[(unsigned long long)r * (unsigned long long)src_stride + (unsigned long long)(col_offset + c)];
    }
}

void lora_slice_f32_to_f16(
    const float *src,
    unsigned int src_stride,
    unsigned short *dst,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int col_offset,
    unsigned int cols,
    const float *scale_pair
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = T_pad * cols;
    if (idx >= total) return;
    unsigned int t = idx / cols;
    unsigned int c = idx % cols;
    if (t < T_real) {
        float sc = scale_pair[0];
        float v = src[(unsigned long long)t * (unsigned long long)src_stride + (unsigned long long)(col_offset + c)];
        dst[idx] = (unsigned short)__ptx_f32_to_f16(v * sc);
    } else {
        dst[idx] = 0U;
    }
}

/* 2. Exact FP32 Matrix Multiplies (Reference Grad-Check & Unaligned Fallback) */

/* C[M, N] = (add ? C : 0) + A[M, K] @ W_f32[N, K]^T */
void lora_matmul_f32w_nt(
    const float *A,
    const float *W,
    float *C,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    unsigned int n = __ptx_ctaid_x();
    unsigned int m = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (n >= N || m >= M) return;

    const float *ar = A + (unsigned long long)m * (unsigned long long)K;
    const float *wr = W + (unsigned long long)n * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(ar[k], wr[k], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        unsigned long long oi = (unsigned long long)m * (unsigned long long)N + (unsigned long long)n;
        C[oi] = add_flag ? (C[oi] + sum) : sum;
    }
}

/* C[M, K] = (add ? C : 0) + A[M, N] @ W_f32[N, K] */
void lora_matmul_f32w_tx(
    const float *A,
    const float *W,
    float *C,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    unsigned int k = __ptx_ctaid_x();
    unsigned int m = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (k >= K || m >= M) return;

    const float *ar = A + (unsigned long long)m * (unsigned long long)N;
    float acc = 0.0f;
    for (unsigned int n = lane; n < N; n += 32U) {
        acc = __ptx_fmaf(ar[n], W[(unsigned long long)n * (unsigned long long)K + (unsigned long long)k], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        unsigned long long oi = (unsigned long long)m * (unsigned long long)K + (unsigned long long)k;
        C[oi] = add_flag ? (C[oi] + sum) : sum;
    }
}

/* C[M, N] = (add ? C : 0) + A[M, K] @ W_f16[N, K]^T */
void lora_matmul_f16w_nt(
    const float *A,
    const unsigned short *W,
    float *C,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    unsigned int n = __ptx_ctaid_x();
    unsigned int m = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (n >= N || m >= M) return;

    const float *ar = A + (unsigned long long)m * (unsigned long long)K;
    const unsigned short *wr = W + (unsigned long long)n * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(ar[k], __ptx_f16_to_f32((unsigned int)wr[k]), acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        unsigned long long oi = (unsigned long long)m * (unsigned long long)N + (unsigned long long)n;
        C[oi] = add_flag ? (C[oi] + sum) : sum;
    }
}

/* C[M, K] = (add ? C : 0) + A[M, N] @ W_f16[N, K] */
void lora_matmul_f16w_tx(
    const float *A,
    const unsigned short *W,
    float *C,
    unsigned int M,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    unsigned int k = __ptx_ctaid_x();
    unsigned int m = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (k >= K || m >= M) return;

    const float *ar = A + (unsigned long long)m * (unsigned long long)N;
    float acc = 0.0f;
    for (unsigned int n = lane; n < N; n += 32U) {
        float wv = __ptx_f16_to_f32((unsigned int)W[(unsigned long long)n * (unsigned long long)K + (unsigned long long)k]);
        acc = __ptx_fmaf(ar[n], wv, acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        unsigned long long oi = (unsigned long long)m * (unsigned long long)K + (unsigned long long)k;
        C[oi] = add_flag ? (C[oi] + sum) : sum;
    }
}

/* 3. LoRA Low-Rank Forward & Backward Kernels (A[r, K], B[N, r]) */

/* H[T, r] = X[T, K] @ A[r, K]^T; grid=(r, T), block=32 */
void lora_fwd_a(
    const float *X,
    const float *A,
    float *H,
    unsigned int T,
    unsigned int r,
    unsigned int K
) {
    unsigned int j = __ptx_ctaid_x();
    unsigned int t = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (j >= r || t >= T) return;

    const float *xt = X + (unsigned long long)t * (unsigned long long)K;
    const float *aj = A + (unsigned long long)j * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(xt[k], aj[k], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        H[(unsigned long long)t * (unsigned long long)r + (unsigned long long)j] = sum;
    }
}

/* Y[T, N] += scale * H[T, r] @ B[N, r]^T; grid=((N+255)/256, T), block=256 */
void lora_fwd_b_add(
    const float *H,
    const float *B,
    float *Y,
    unsigned int T,
    unsigned int N,
    unsigned int r,
    float scale
) {
    unsigned int n = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int t = __ptx_ctaid_y();
    if (n >= N || t >= T) return;

    const float *ht = H + (unsigned long long)t * (unsigned long long)r;
    const float *bn = B + (unsigned long long)n * (unsigned long long)r;
    float acc = 0.0f;
    for (unsigned int j = 0U; j < r; j++) {
        acc = __ptx_fmaf(ht[j], bn[j], acc);
    }
    Y[(unsigned long long)t * (unsigned long long)N + (unsigned long long)n] += scale * acc;
}

/* dH[T, r] = scale * dY[T, N] @ B[N, r]; grid=(r, T), block=32 */
void lora_bwd_dh(
    const float *dY,
    const float *B,
    float *dH,
    unsigned int T,
    unsigned int N,
    unsigned int r,
    float scale
) {
    unsigned int j = __ptx_ctaid_x();
    unsigned int t = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (j >= r || t >= T) return;

    const float *dyt = dY + (unsigned long long)t * (unsigned long long)N;
    float acc = 0.0f;
    for (unsigned int n = lane; n < N; n += 32U) {
        acc = __ptx_fmaf(dyt[n], B[(unsigned long long)n * (unsigned long long)r + (unsigned long long)j], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        dH[(unsigned long long)t * (unsigned long long)r + (unsigned long long)j] = scale * sum;
    }
}

/* dB[N, r] += scale * sum_t dY[t, n] * H[t, j]; grid=(N, r), block=32 */
void lora_bwd_db_accum(
    const float *dY,
    const float *H,
    float *dB,
    unsigned int T,
    unsigned int N,
    unsigned int r,
    float scale
) {
    unsigned int n = __ptx_ctaid_x();
    unsigned int j = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (n >= N || j >= r) return;

    float acc = 0.0f;
    for (unsigned int t = lane; t < T; t += 32U) {
        acc = __ptx_fmaf(
            dY[(unsigned long long)t * (unsigned long long)N + (unsigned long long)n],
            H[(unsigned long long)t * (unsigned long long)r + (unsigned long long)j],
            acc
        );
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) {
        dB[(unsigned long long)n * (unsigned long long)r + (unsigned long long)j] += scale * sum;
    }
}

/* dA[r, K] += sum_t dH[t, j] * X[t, k]; grid=((K+255)/256, r), block=256 */
void lora_bwd_da_accum(
    const float *dH,
    const float *X,
    float *dA,
    unsigned int T,
    unsigned int r,
    unsigned int K
) {
    unsigned int k = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int j = __ptx_ctaid_y();
    if (k >= K || j >= r) return;

    float acc = 0.0f;
    for (unsigned int t = 0U; t < T; t++) {
        acc = __ptx_fmaf(
            dH[(unsigned long long)t * (unsigned long long)r + (unsigned long long)j],
            X[(unsigned long long)t * (unsigned long long)K + (unsigned long long)k],
            acc
        );
    }
    dA[(unsigned long long)j * (unsigned long long)K + (unsigned long long)k] += acc;
}

/* dX[T, K] += dH[T, r] @ A[r, K]; grid=((K+255)/256, T), block=256 */
void lora_bwd_dx_add(
    const float *dH,
    const float *A,
    float *dX,
    unsigned int T,
    unsigned int r,
    unsigned int K
) {
    unsigned int k = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int t = __ptx_ctaid_y();
    if (k >= K || t >= T) return;

    const float *dht = dH + (unsigned long long)t * (unsigned long long)r;
    float acc = 0.0f;
    for (unsigned int j = 0U; j < r; j++) {
        acc = __ptx_fmaf(dht[j], A[(unsigned long long)j * (unsigned long long)K + (unsigned long long)k], acc);
    }
    dX[(unsigned long long)t * (unsigned long long)K + (unsigned long long)k] += acc;
}

/* 4. RMSNorm Backward & RoPE Backward */

/* dx[T, K]: grid=(T, 1), block=32 */
void lora_rmsnorm_bwd(
    const float *x,
    const float *dy,
    const float *w,
    float *dx,
    unsigned int T,
    unsigned int K,
    float eps,
    unsigned int add_flag
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (t >= T) return;

    unsigned long long base = (unsigned long long)t * (unsigned long long)K;
    const float *xt = x + base;
    const float *dyt = dy + base;
    float *dxt = dx + base;

    float ss = 0.0f;
    float dot = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        float xk = xt[k];
        ss = __ptx_fmaf(xk, xk, ss);
        dot = __ptx_fmaf(dyt[k] * w[k], xk, dot);
    }
    float inv_k = 1.0f / (float)K;
    float r = __ptx_rsqrtf(lwarp_sum_f32(ss) * inv_k + eps);
    float c_r2 = (lwarp_sum_f32(dot) * inv_k) * r * r;

    for (unsigned int k = lane; k < K; k += 32U) {
        float grad = r * (dyt[k] * w[k] - xt[k] * c_r2);
        dxt[k] = add_flag ? (dxt[k] + grad) : grad;
    }
}

/* Inverse (-theta) RoPE on dq and dk; grid=((total_pairs+255)/256, T), block=256 */
void lora_rope_bwd(
    float *dq,
    float *dk,
    unsigned int pos0,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta,
    unsigned int is_neox
) {
    unsigned int t = __ptx_ctaid_y();
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int pos = pos0 + t;
    float *dqt = dq + (unsigned long long)t * (unsigned long long)(n_heads * head_dim);
    float *dkt = dk + (unsigned long long)t * (unsigned long long)(n_kv_heads * head_dim);
    unsigned int h = idx / half;
    unsigned int p = idx % half;
    float *vec = (h < n_heads) ? (dqt + h * head_dim) : (dkt + (h - n_heads) * head_dim);

    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    unsigned int i0 = is_neox ? p : (2U * p);
    unsigned int i1 = is_neox ? (p + half) : (2U * p + 1U);
    float da = vec[i0];
    float db = vec[i1];
    vec[i0] = da * c + db * s;
    vec[i1] = -da * s + db * c;
}

/* 5. Causal GQA Attention Forward & Exact Deterministic Backward */

/* probs[n_heads, T, T], out[T, n_heads * head_dim]; grid=(T, n_heads), block=32 */
void lora_attn_gqa_fwd(
    const float *q,
    const float *k,
    const float *v,
    float *probs,
    float *out,
    unsigned int T,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int h = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (t >= T || h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int q_dim = n_heads * head_dim;
    unsigned int kv_dim = n_kv_heads * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    const float *qt = q + (unsigned long long)t * (unsigned long long)q_dim + (unsigned long long)(h * head_dim);
    float *prow = probs + ((unsigned long long)h * (unsigned long long)T + (unsigned long long)t) * (unsigned long long)T;
    float *oh = out + (unsigned long long)t * (unsigned long long)q_dim + (unsigned long long)(h * head_dim);

    float lmax = -1e30f;
    for (unsigned int s = 0U; s <= t; s++) {
        const float *ks = k + (unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        float dot = 0.0f;
        for (unsigned int d = lane; d < head_dim; d += 32U) {
            dot = __ptx_fmaf(qt[d], ks[d], dot);
        }
        float score = lwarp_sum_f32(dot) * inv_sqrt_hd;
        if (lane == 0U) prow[s] = score;
        lmax = __ptx_maxf(lmax, score);
    }
    for (unsigned int s = t + 1U + lane; s < T; s += 32U) {
        prow[s] = 0.0f;
    }
    __ptx_syncthreads();

    float lsum = 0.0f;
    for (unsigned int s = lane; s <= t; s += 32U) {
        float e = lora_expf(prow[s] - lmax);
        prow[s] = e;
        lsum += e;
    }
    float sumv = lwarp_sum_f32(lsum);
    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    __ptx_syncthreads();
    for (unsigned int s = lane; s <= t; s += 32U) {
        prow[s] *= inv_sum;
    }
    __ptx_syncthreads();

    for (unsigned int d = lane; d < head_dim; d += 32U) {
        float acc = 0.0f;
        for (unsigned int s = 0U; s <= t; s++) {
            float vs = v[(unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim + d)];
            acc = __ptx_fmaf(prow[s], vs, acc);
        }
        oh[d] = acc;
    }
}

/* Computes ds[n_heads, T, T] = P * (dP - dot(P, dP)) / sqrt(hd); grid=(T, n_heads), block=32 */
void lora_attn_gqa_bwd_ds(
    const float *dout,
    const float *v,
    const float *probs,
    float *ds,
    unsigned int T,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int h = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (t >= T || h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int q_dim = n_heads * head_dim;
    unsigned int kv_dim = n_kv_heads * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    const float *dout_t = dout + (unsigned long long)t * (unsigned long long)q_dim + (unsigned long long)(h * head_dim);
    const float *prow = probs + ((unsigned long long)h * (unsigned long long)T + (unsigned long long)t) * (unsigned long long)T;
    float *drow = ds + ((unsigned long long)h * (unsigned long long)T + (unsigned long long)t) * (unsigned long long)T;

    for (unsigned int s = 0U; s <= t; s++) {
        const float *vs = v + (unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
        float dot = 0.0f;
        for (unsigned int d = lane; d < head_dim; d += 32U) {
            dot = __ptx_fmaf(dout_t[d], vs[d], dot);
        }
        float dp = lwarp_sum_f32(dot);
        if (lane == 0U) drow[s] = dp;
    }
    __ptx_syncthreads();

    float di_local = 0.0f;
    for (unsigned int s = lane; s <= t; s += 32U) {
        di_local = __ptx_fmaf(prow[s], drow[s], di_local);
    }
    float di = lwarp_sum_f32(di_local);

    for (unsigned int s = lane; s <= t; s += 32U) {
        drow[s] = prow[s] * (drow[s] - di) * inv_sqrt_hd;
    }
    for (unsigned int s = t + 1U + lane; s < T; s += 32U) {
        drow[s] = 0.0f;
    }
}

/* dq[T, n_heads * head_dim] = ds @ K; grid=(T, n_heads), block=32 */
void lora_attn_gqa_bwd_q(
    const float *ds,
    const float *k,
    float *dq,
    unsigned int T,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int h = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (t >= T || h >= n_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int q_dim = n_heads * head_dim;
    unsigned int kv_dim = n_kv_heads * head_dim;

    const float *drow = ds + ((unsigned long long)h * (unsigned long long)T + (unsigned long long)t) * (unsigned long long)T;
    float *dqh = dq + (unsigned long long)t * (unsigned long long)q_dim + (unsigned long long)(h * head_dim);

    for (unsigned int d = lane; d < head_dim; d += 32U) {
        float acc = 0.0f;
        for (unsigned int s = 0U; s <= t; s++) {
            float ks = k[(unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim + d)];
            acc = __ptx_fmaf(drow[s], ks, acc);
        }
        dqh[d] = acc;
    }
}

/* dk[T, n_kv_heads * head_dim], dv[T, n_kv_heads * head_dim]; grid=(T, n_kv_heads), block=32 */
void lora_attn_gqa_bwd_kv(
    const float *ds,
    const float *probs,
    const float *q,
    const float *dout,
    float *dk,
    float *dv,
    unsigned int T,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int s = __ptx_ctaid_x();
    unsigned int kv_h = __ptx_ctaid_y();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (s >= T || kv_h >= n_kv_heads) return;

    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int h0 = kv_h * kv_group;
    unsigned int q_dim = n_heads * head_dim;
    unsigned int kv_dim = n_kv_heads * head_dim;

    float *dks = dk + (unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);
    float *dvs = dv + (unsigned long long)s * (unsigned long long)kv_dim + (unsigned long long)(kv_h * head_dim);

    for (unsigned int d = lane; d < head_dim; d += 32U) {
        float ak = 0.0f;
        float av = 0.0f;
        for (unsigned int g = 0U; g < kv_group; g++) {
            unsigned int h = h0 + g;
            unsigned long long h_off = (unsigned long long)h * (unsigned long long)T * (unsigned long long)T;
            for (unsigned int t = s; t < T; t++) {
                unsigned long long ts_idx = h_off + (unsigned long long)t * (unsigned long long)T + (unsigned long long)s;
                unsigned long long thd = (unsigned long long)t * (unsigned long long)q_dim + (unsigned long long)(h * head_dim + d);
                ak = __ptx_fmaf(ds[ts_idx], q[thd], ak);
                av = __ptx_fmaf(probs[ts_idx], dout[thd], av);
            }
        }
        dks[d] = ak;
        dvs[d] = av;
    }
}

/* 6. SwiGLU Forward & Exact Backward */

void lora_swiglu_fwd_batch(
    const float *gate,
    const float *up,
    float *out,
    unsigned int n
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float g = gate[i];
    float sig = 1.0f / (1.0f + lora_expf(-g));
    out[i] = (g * sig) * up[i];
}

void lora_swiglu_bwd_batch(
    const float *gate,
    const float *up,
    const float *dout,
    float *dgate,
    float *dup,
    unsigned int n
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float g = gate[i];
    float u = up[i];
    float dy = dout[i];
    float sig = 1.0f / (1.0f + lora_expf(-g));
    float silu = g * sig;
    dup[i] = dy * silu;
    dgate[i] = dy * u * (sig * (1.0f + g * (1.0f - sig)));
}

/* 7. Assistant-Only Masked Cross-Entropy, Norm Reduction & AdamW */

void lora_ce_loss_masked_bwd(
    const float *logits,
    const unsigned int *targets,
    float *dlogits,
    float *losses,
    unsigned int T,
    unsigned int V,
    float inv_active
) {
    unsigned int t = __ptx_ctaid_x();
    if (t >= T) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned long long base = (unsigned long long)t * (unsigned long long)V;
    unsigned int tgt = targets[t];
    if (tgt >= V) {
        if (lane == 0U) losses[t] = 0.0f;
        for (unsigned int v = lane; v < V; v += 32U) {
            dlogits[base + v] = 0.0f;
        }
        return;
    }

    float lmax = -1e30f;
    for (unsigned int v = lane; v < V; v += 32U) {
        lmax = __ptx_maxf(lmax, logits[base + v]);
    }
    float maxv = lwarp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int v = lane; v < V; v += 32U) {
        float e = lora_expf(logits[base + v] - maxv);
        dlogits[base + v] = e;
        lsum += e;
    }
    float sumv = lwarp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    if (lane == 0U) {
        float tlog = logits[base + tgt] - maxv;
        losses[t] = lora_logf(sumv) - tlog;
    }
    for (unsigned int v = lane; v < V; v += 32U) {
        float p = dlogits[base + v] * inv_sum;
        float sub = (v == tgt) ? 1.0f : 0.0f;
        dlogits[base + v] = (p - sub) * inv_active;
    }
}

void lora_sq_sum_blocks(const float *x, float *block_out, unsigned int n) {
    unsigned int cta = __ptx_ctaid_x();
    unsigned int gdim = 256U;
    unsigned int lane = __ptx_tid_x() & 31U;
    float acc = 0.0f;
    for (unsigned int i = cta * 32U + lane; i < n; i += gdim * 32U) {
        float v = x[i];
        acc = __ptx_fmaf(v, v, acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) block_out[cta] = sum;
}

void lora_scale_f32(float *x, float scale, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) x[i] *= scale;
}

void lora_adamw_step(
    float *param,
    const float *grad,
    float *m,
    float *v,
    unsigned int n,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float wd,
    float bc1_inv,
    float bc2_inv
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float g = grad[i];
    float mi = beta1 * m[i] + (1.0f - beta1) * g;
    float vi = beta2 * v[i] + (1.0f - beta2) * (g * g);
    m[i] = mi;
    v[i] = vi;
    float m_hat = mi * bc1_inv;
    float v_hat = vi * bc2_inv;
    float p = param[i];
    if (wd > 0.0f) p = p * (1.0f - lr * wd);
    p = p - lr * (m_hat / (__ptx_sqrtf(v_hat) + eps));
    param[i] = p;
}

/* ========================================================================= */
/* 6. Evolved Blackwell sm_120 Decoder Kernels (Track 23A)                   */
/* ========================================================================= */

static int dec_s8_from_u8(unsigned int b) {
    int v = (int)(b & 255U);
    if (v >= 128) v -= 256;
    return v;
}

static unsigned int dec_q4k_scale_at(const unsigned char *scales, unsigned int j) {
    if (j < 4U) return (unsigned int)(scales[j] & 63U);
    return (unsigned int)((scales[j + 4U] & 15U) | ((scales[j - 4U] >> 6) << 4));
}

static unsigned int dec_q4k_min_at(const unsigned char *scales, unsigned int j) {
    if (j < 4U) return (unsigned int)(scales[j + 4U] & 63U);
    return (unsigned int)((scales[j + 4U] >> 4) | ((scales[j] >> 6) << 4));
}

/* 8-warp (256 threads) RMSNorm using 32-byte scratch inside unused xb2 */
void dec_rmsnorm_f32(
    float *out,
    const float *x,
    const float *weight,
    float *scratch,
    unsigned int dim,
    float eps
) {
    unsigned int tid = __ptx_tid_x();
    unsigned int warp = tid >> 5;
    unsigned int lane = tid & 31U;
    float ss = 0.0f;
    for (unsigned int i = tid; i < dim; i += 256U) {
        float v = x[i];
        ss = __ptx_fmaf(v, v, ss);
    }
    ss = lwarp_sum_f32(ss);
    if (lane == 0U) scratch[warp] = ss;
    __ptx_syncthreads();
    float tot = (scratch[0] + scratch[1]) + (scratch[2] + scratch[3])
              + (scratch[4] + scratch[5]) + (scratch[6] + scratch[7]);
    float inv_rms = __ptx_rsqrtf(tot / (float)dim + eps);
    for (unsigned int i = tid; i < dim; i += 256U) {
        out[i] = x[i] * inv_rms * weight[i];
    }
}

/* Lean 4-warp (128 threads), 1 row/warp Q4_K matvec for small-N projections */
void dec_matvec_q4_k_4w(
    const unsigned int *W,
    const float *x,
    float *y,
    unsigned int N,
    unsigned int K,
    unsigned int add_res
) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;

    unsigned int sb_per_row = K >> 8;
    unsigned long long row_words = (unsigned long long)sb_per_row * 36ULL;
    const unsigned int *wrow = W + (unsigned long long)row * row_words;

    unsigned int c = lane >> 3;
    unsigned int q_in_chunk = (lane & 7U) << 2;
    unsigned int x_off = (c << 6) + q_in_chunk;
    unsigned int c_lo = (c < 2U) ? 1U : 0U;
    unsigned int k0 = c_lo ? (c << 1) : ((c - 2U) << 1);
    unsigned int k1 = k0 + 1U;
    unsigned int sh0 = k0 << 3;
    unsigned int sh1 = k1 << 3;

    float acc = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const float *xsb = x + (sb << 8) + x_off;
        float x0 = xsb[0];
        float x1 = xsb[1];
        float x2 = xsb[2];
        float x3 = xsb[3];
        float x4 = xsb[32];
        float x5 = xsb[33];
        float x6 = xsb[34];
        float x7 = xsb[35];
        float sx0 = (x0 + x1) + (x2 + x3);
        float sx1 = (x4 + x5) + (x6 + x7);

        const unsigned int *w0 = wrow + (unsigned long long)sb * 36ULL;
        unsigned int dm = w0[0];
        unsigned int s0 = w0[1];
        unsigned int s1 = w0[2];
        unsigned int s2 = w0[3];
        unsigned int q4 = w0[4U + lane];
        float d = __ptx_f16_to_f32(dm & 0xFFFFU);
        float dmin = __ptx_f16_to_f32(dm >> 16);
        unsigned int sc0, mn0, sc1, mn1;
        if (c_lo) {
            sc0 = (s0 >> sh0) & 63U;
            mn0 = (s1 >> sh0) & 63U;
            sc1 = (s0 >> sh1) & 63U;
            mn1 = (s1 >> sh1) & 63U;
        } else {
            unsigned int b2_0 = (s2 >> sh0) & 255U;
            unsigned int b0_0 = (s0 >> sh0) & 255U;
            unsigned int b1_0 = (s1 >> sh0) & 255U;
            sc0 = (b2_0 & 15U) | ((b0_0 >> 6) << 4);
            mn0 = (b2_0 >> 4) | ((b1_0 >> 6) << 4);
            unsigned int b2_1 = (s2 >> sh1) & 255U;
            unsigned int b0_1 = (s0 >> sh1) & 255U;
            unsigned int b1_1 = (s1 >> sh1) & 255U;
            sc1 = (b2_1 & 15U) | ((b0_1 >> 6) << 4);
            mn1 = (b2_1 >> 4) | ((b1_1 >> 6) << 4);
        }
        float sum0 = __ptx_fmaf((float)(q4 & 15U), x0,
                     __ptx_fmaf((float)((q4 >> 8) & 15U), x1,
                     __ptx_fmaf((float)((q4 >> 16) & 15U), x2, (float)((q4 >> 24) & 15U) * x3)));
        float sum1 = __ptx_fmaf((float)((q4 >> 4) & 15U), x4,
                     __ptx_fmaf((float)((q4 >> 12) & 15U), x5,
                     __ptx_fmaf((float)((q4 >> 20) & 15U), x6, (float)((q4 >> 28) & 15U) * x7)));
        acc = __ptx_fmaf(d * (float)sc0, sum0, __ptx_fmaf(-(dmin * (float)mn0), sx0, acc));
        acc = __ptx_fmaf(d * (float)sc1, sum1, __ptx_fmaf(-(dmin * (float)mn1), sx1, acc));
    }

    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

/* Coalesced 4-warp (128 threads), 2 rows/warp = 8 rows/CTA Q4_K matvec + fused add_res */
void dec_matvec_q4_k_8r(
    const unsigned int *W,
    const float *x,
    float *y,
    unsigned int N,
    unsigned int K,
    unsigned int add_res
) {
    unsigned int tid = __ptx_tid_x();
    unsigned int warp = tid >> 5;
    unsigned int lane = tid & 31U;
    unsigned int row0 = (__ptx_ctaid_x() << 3) + (warp << 1);
    if (row0 >= N) return;
    unsigned int has1 = (row0 + 1U < N) ? 1U : 0U;

    unsigned int sb_per_row = K >> 8;
    unsigned long long row_words = (unsigned long long)sb_per_row * 36ULL;
    const unsigned int *wrow0 = W + (unsigned long long)row0 * row_words;
    const unsigned int *wrow1 = has1 ? (wrow0 + row_words) : wrow0;

    unsigned int c = lane >> 3;
    unsigned int q_in_chunk = (lane & 7U) << 2;
    unsigned int x_off = (c << 6) + q_in_chunk;
    unsigned int c_lo = (c < 2U) ? 1U : 0U;
    unsigned int k0 = c_lo ? (c << 1) : ((c - 2U) << 1);
    unsigned int k1 = k0 + 1U;
    unsigned int sh0 = k0 << 3;
    unsigned int sh1 = k1 << 3;

    float acc0 = 0.0f;
    float acc1 = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const float *xsb = x + (sb << 8) + x_off;
        float x0 = xsb[0];
        float x1 = xsb[1];
        float x2 = xsb[2];
        float x3 = xsb[3];
        float x4 = xsb[32];
        float x5 = xsb[33];
        float x6 = xsb[34];
        float x7 = xsb[35];
        float sx0 = (x0 + x1) + (x2 + x3);
        float sx1 = (x4 + x5) + (x6 + x7);

        const unsigned int *w0 = wrow0 + (unsigned long long)sb * 36ULL;
        unsigned int dm = w0[0];
        unsigned int s0 = w0[1];
        unsigned int s1 = w0[2];
        unsigned int s2 = w0[3];
        unsigned int q4 = w0[4U + lane];
        float d = __ptx_f16_to_f32(dm & 0xFFFFU);
        float dmin = __ptx_f16_to_f32(dm >> 16);
        unsigned int sc0, mn0, sc1, mn1;
        if (c_lo) {
            sc0 = (s0 >> sh0) & 63U;
            mn0 = (s1 >> sh0) & 63U;
            sc1 = (s0 >> sh1) & 63U;
            mn1 = (s1 >> sh1) & 63U;
        } else {
            unsigned int b2_0 = (s2 >> sh0) & 255U;
            unsigned int b0_0 = (s0 >> sh0) & 255U;
            unsigned int b1_0 = (s1 >> sh0) & 255U;
            sc0 = (b2_0 & 15U) | ((b0_0 >> 6) << 4);
            mn0 = (b2_0 >> 4) | ((b1_0 >> 6) << 4);
            unsigned int b2_1 = (s2 >> sh1) & 255U;
            unsigned int b0_1 = (s0 >> sh1) & 255U;
            unsigned int b1_1 = (s1 >> sh1) & 255U;
            sc1 = (b2_1 & 15U) | ((b0_1 >> 6) << 4);
            mn1 = (b2_1 >> 4) | ((b1_1 >> 6) << 4);
        }
        float sum0 = __ptx_fmaf((float)(q4 & 15U), x0,
                     __ptx_fmaf((float)((q4 >> 8) & 15U), x1,
                     __ptx_fmaf((float)((q4 >> 16) & 15U), x2, (float)((q4 >> 24) & 15U) * x3)));
        float sum1 = __ptx_fmaf((float)((q4 >> 4) & 15U), x4,
                     __ptx_fmaf((float)((q4 >> 12) & 15U), x5,
                     __ptx_fmaf((float)((q4 >> 20) & 15U), x6, (float)((q4 >> 28) & 15U) * x7)));
        acc0 = __ptx_fmaf(d * (float)sc0, sum0, __ptx_fmaf(-(dmin * (float)mn0), sx0, acc0));
        acc0 = __ptx_fmaf(d * (float)sc1, sum1, __ptx_fmaf(-(dmin * (float)mn1), sx1, acc0));

        if (has1) {
            const unsigned int *w1 = wrow1 + (unsigned long long)sb * 36ULL;
            dm = w1[0];
            s0 = w1[1];
            s1 = w1[2];
            s2 = w1[3];
            q4 = w1[4U + lane];
            d = __ptx_f16_to_f32(dm & 0xFFFFU);
            dmin = __ptx_f16_to_f32(dm >> 16);
            if (c_lo) {
                sc0 = (s0 >> sh0) & 63U;
                mn0 = (s1 >> sh0) & 63U;
                sc1 = (s0 >> sh1) & 63U;
                mn1 = (s1 >> sh1) & 63U;
            } else {
                unsigned int b2_0 = (s2 >> sh0) & 255U;
                unsigned int b0_0 = (s0 >> sh0) & 255U;
                unsigned int b1_0 = (s1 >> sh0) & 255U;
                sc0 = (b2_0 & 15U) | ((b0_0 >> 6) << 4);
                mn0 = (b2_0 >> 4) | ((b1_0 >> 6) << 4);
                unsigned int b2_1 = (s2 >> sh1) & 255U;
                unsigned int b0_1 = (s0 >> sh1) & 255U;
                unsigned int b1_1 = (s1 >> sh1) & 255U;
                sc1 = (b2_1 & 15U) | ((b0_1 >> 6) << 4);
                mn1 = (b2_1 >> 4) | ((b1_1 >> 6) << 4);
            }
            sum0 = __ptx_fmaf((float)(q4 & 15U), x0,
                   __ptx_fmaf((float)((q4 >> 8) & 15U), x1,
                   __ptx_fmaf((float)((q4 >> 16) & 15U), x2, (float)((q4 >> 24) & 15U) * x3)));
            sum1 = __ptx_fmaf((float)((q4 >> 4) & 15U), x4,
                   __ptx_fmaf((float)((q4 >> 12) & 15U), x5,
                   __ptx_fmaf((float)((q4 >> 20) & 15U), x6, (float)((q4 >> 28) & 15U) * x7)));
            acc1 = __ptx_fmaf(d * (float)sc0, sum0, __ptx_fmaf(-(dmin * (float)mn0), sx0, acc1));
            acc1 = __ptx_fmaf(d * (float)sc1, sum1, __ptx_fmaf(-(dmin * (float)mn1), sx1, acc1));
        }
    }

    acc0 = lwarp_sum_f32(acc0);
    if (lane == 0U) y[row0] = add_res ? (y[row0] + acc0) : acc0;
    if (has1) {
        acc1 = lwarp_sum_f32(acc1);
        if (lane == 0U) y[row0 + 1U] = add_res ? (y[row0 + 1U] + acc1) : acc1;
    }
}

/* Single-pass 4-warp (128 threads), 4 rows/CTA Q6_K matvec using aligned u16 loads + fused add_res */
void dec_matvec_q6_k_4w(
    const unsigned short *W,
    const float *x,
    float *y,
    unsigned int N,
    unsigned int K,
    unsigned int add_res
) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;

    unsigned int sb_per_row = K >> 8;
    unsigned long long row_u16 = (unsigned long long)sb_per_row * 105ULL;
    const unsigned short *wrow = W + (unsigned long long)row * row_u16;

    unsigned int h = lane >> 4;
    unsigned int j = lane & 15U;
    unsigned int sh_sc = (j >> 3) << 3;
    unsigned int ql_off = (h << 5) + j;
    unsigned int qh_off = 64U + (h << 4) + j;
    unsigned int sc_off = 96U + (h << 2);
    unsigned int x_base_off = (h << 7) + (j << 1);

    float acc = 0.0f;
    for (unsigned int sb = 0U; sb < sb_per_row; sb++) {
        const unsigned short *w16 = wrow + (unsigned long long)sb * 105ULL;
        float d = __ptx_f16_to_f32((unsigned int)w16[104]);
        unsigned int ql_w0 = (unsigned int)w16[ql_off];
        unsigned int ql_w1 = (unsigned int)w16[ql_off + 16U];
        unsigned int qh_w = (unsigned int)w16[qh_off];
        float s0 = (float)dec_s8_from_u8(((unsigned int)w16[sc_off] >> sh_sc) & 255U);
        float s2 = (float)dec_s8_from_u8(((unsigned int)w16[sc_off + 1U] >> sh_sc) & 255U);
        float s4 = (float)dec_s8_from_u8(((unsigned int)w16[sc_off + 2U] >> sh_sc) & 255U);
        float s6 = (float)dec_s8_from_u8(((unsigned int)w16[sc_off + 3U] >> sh_sc) & 255U);

        unsigned int ql0a = ql_w0 & 255U, ql0b = ql_w0 >> 8;
        unsigned int ql1a = ql_w1 & 255U, ql1b = ql_w1 >> 8;
        unsigned int qh0a = qh_w & 255U, qh0b = qh_w >> 8;

        int q1a = (int)((ql0a & 15U) | ((qh0a & 3U) << 4)) - 32;
        int q2a = (int)((ql1a & 15U) | (((qh0a >> 2) & 3U) << 4)) - 32;
        int q3a = (int)((ql0a >> 4) | (((qh0a >> 4) & 3U) << 4)) - 32;
        int q4a = (int)((ql1a >> 4) | (((qh0a >> 6) & 3U) << 4)) - 32;

        int q1b = (int)((ql0b & 15U) | ((qh0b & 3U) << 4)) - 32;
        int q2b = (int)((ql1b & 15U) | (((qh0b >> 2) & 3U) << 4)) - 32;
        int q3b = (int)((ql0b >> 4) | (((qh0b >> 4) & 3U) << 4)) - 32;
        int q4b = (int)((ql1b >> 4) | (((qh0b >> 6) & 3U) << 4)) - 32;

        const float *xh = x + (sb << 8) + x_base_off;
        float sub = __ptx_fmaf(s0, __ptx_fmaf((float)q1a, xh[0], (float)q1b * xh[1]),
                    __ptx_fmaf(s2, __ptx_fmaf((float)q2a, xh[32], (float)q2b * xh[33]),
                    __ptx_fmaf(s4, __ptx_fmaf((float)q3a, xh[64], (float)q3b * xh[65]),
                               s6 * __ptx_fmaf((float)q4a, xh[96], (float)q4b * xh[97]))));
        acc = __ptx_fmaf(d, sub, acc);
    }

    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

void dec_matvec_f32_4w(const float *W, const float *x, float *y, unsigned int N, unsigned int K, unsigned int add_res) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;
    const float *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(wrow[k], x[k], acc);
    }
    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

void dec_matvec_f16_4w(const unsigned short *W, const float *x, float *y, unsigned int N, unsigned int K, unsigned int add_res) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;
    const unsigned short *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(__ptx_f16_to_f32((unsigned int)wrow[k]), x[k], acc);
    }
    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

void dec_matvec_bf16_4w(const unsigned short *W, const float *x, float *y, unsigned int N, unsigned int K, unsigned int add_res) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;
    const unsigned short *wrow = W + (unsigned long long)row * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(__ptx_bf16_to_f32((unsigned int)wrow[k]), x[k], acc);
    }
    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

void dec_matvec_q8_0_4w(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K, unsigned int add_res) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
    if (row >= N) return;
    unsigned int blocks_per_row = K >> 5;
    const unsigned char *wrow = W + (unsigned long long)row * (unsigned long long)(blocks_per_row * 34U);
    float acc = 0.0f;
    for (unsigned int b = 0U; b < blocks_per_row; b++) {
        const unsigned char *blk = wrow + (unsigned long long)(b * 34U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        int q = dec_s8_from_u8((unsigned int)blk[2U + lane]);
        acc = __ptx_fmaf(d * (float)q, x[(b << 5) + lane], acc);
    }
    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

void dec_matvec_q4_0_4w(const unsigned char *W, const float *x, float *y, unsigned int N, unsigned int K, unsigned int add_res) {
    unsigned int tid = __ptx_tid_x();
    unsigned int row = (__ptx_ctaid_x() << 2) + (tid >> 5);
    unsigned int lane = tid & 31U;
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
        acc = __ptx_fmaf(d * (float)q, x[(b << 5) + lane], acc);
    }
    acc = lwarp_sum_f32(acc);
    if (lane == 0U) y[row] = add_res ? (y[row] + acc) : acc;
}

/* Device-indirect single-token embeddings reading tok = step_args[0] */
void dec_embed_ind_f32(const float *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
        out[i] = table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i];
    }
}

void dec_embed_ind_f16(const unsigned short *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
        out[i] = __ptx_f16_to_f32((unsigned int)table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i]);
    }
}

void dec_embed_ind_bf16(const unsigned short *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
        out[i] = __ptx_bf16_to_f32((unsigned int)table[(unsigned long long)tok * (unsigned long long)dim + (unsigned long long)i]);
    }
}

void dec_embed_ind_q8_0(const unsigned char *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
        unsigned int blocks_per_row = dim >> 5;
        unsigned int b = i >> 5;
        unsigned int j = i & 31U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(blocks_per_row * 34U);
        const unsigned char *blk = table + row_off + (unsigned long long)(b * 34U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        out[i] = d * (float)dec_s8_from_u8((unsigned int)blk[2U + j]);
    }
}

void dec_embed_ind_q4_0(const unsigned char *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
        unsigned int blocks_per_row = dim >> 5;
        unsigned int b = i >> 5;
        unsigned int j = i & 31U;
        unsigned long long row_off = (unsigned long long)tok * (unsigned long long)(blocks_per_row * 18U);
        const unsigned char *blk = table + row_off + (unsigned long long)(b * 18U);
        float d = __ptx_f16_to_f32((unsigned int)blk[0] | ((unsigned int)blk[1] << 8));
        unsigned int qbyte = (unsigned int)blk[2U + (j & 15U)];
        int q = (int)((j < 16U) ? (qbyte & 15U) : (qbyte >> 4)) - 8;
        out[i] = d * (float)q;
    }
}

void dec_embed_ind_q4_k(const unsigned char *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
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
        unsigned int sc = dec_q4k_scale_at(blk + 4U, j);
        unsigned int mn = dec_q4k_min_at(blk + 4U, j);
        unsigned int qbyte = (unsigned int)blk[16U + (c << 5) + l];
        unsigned int q = sub ? (qbyte >> 4) : (qbyte & 15U);
        out[i] = (d * (float)sc) * (float)q - (dmin * (float)mn);
    }
}

void dec_embed_ind_q6_k(const unsigned char *table, float *out, const unsigned int *step_args, unsigned int dim) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < dim) {
        unsigned int tok = step_args[0];
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
        int sc = dec_s8_from_u8((unsigned int)blk[sc_off]);
        out[i] = d * (float)sc * (float)q;
    }
}

/* Fused optional Q/K/V bias + device-indirect RoPE + KV cache store (step_args[1]=pos, step_args[2]=max_seq) */
void dec_rope_kv_ind_f16(
    float *q,
    const float *k,
    const float *v,
    const float *bq,
    const float *bk,
    const float *bv,
    unsigned short *k_cache,
    unsigned short *v_cache,
    const unsigned int *step_args,
    unsigned int layer,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta,
    unsigned int is_neox,
    unsigned int bias_mask
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int pos = step_args[1];
    unsigned int max_seq = step_args[2];
    unsigned int h = idx / half;
    unsigned int p = idx % half;

    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    unsigned int i0 = is_neox ? p : (2U * p);
    unsigned int i1 = is_neox ? (p + half) : (2U * p + 1U);

    if (h < n_heads) {
        unsigned int off0 = h * head_dim + i0;
        unsigned int off1 = h * head_dim + i1;
        float x0 = q[off0];
        float x1 = q[off1];
        if (bias_mask & 1U) {
            x0 += bq[off0];
            x1 += bq[off1];
        }
        q[off0] = x0 * c - x1 * s;
        q[off1] = x0 * s + x1 * c;
    } else {
        unsigned int kh = h - n_heads;
        unsigned int kvoff0 = kh * head_dim + i0;
        unsigned int kvoff1 = kh * head_dim + i1;
        float x0 = k[kvoff0];
        float x1 = k[kvoff1];
        if (bias_mask & 2U) {
            x0 += bk[kvoff0];
            x1 += bk[kvoff1];
        }
        float rk0 = x0 * c - x1 * s;
        float rk1 = x0 * s + x1 * c;
        float v0 = v[kvoff0];
        float v1 = v[kvoff1];
        if (bias_mask & 4U) {
            v0 += bv[kvoff0];
            v1 += bv[kvoff1];
        }
        unsigned int kv_dim = n_kv_heads * head_dim;
        unsigned long long slot = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)pos)
                                * (unsigned long long)kv_dim;
        k_cache[slot + (unsigned long long)kvoff0] = (unsigned short)__ptx_f32_to_f16(rk0);
        k_cache[slot + (unsigned long long)kvoff1] = (unsigned short)__ptx_f32_to_f16(rk1);
        v_cache[slot + (unsigned long long)kvoff0] = (unsigned short)__ptx_f32_to_f16(v0);
        v_cache[slot + (unsigned long long)kvoff1] = (unsigned short)__ptx_f32_to_f16(v1);
    }
}

void dec_rope_kv_ind_f32(
    float *q,
    const float *k,
    const float *v,
    const float *bq,
    const float *bk,
    const float *bv,
    float *k_cache,
    float *v_cache,
    const unsigned int *step_args,
    unsigned int layer,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim,
    float rope_theta,
    unsigned int is_neox,
    unsigned int bias_mask
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int half = head_dim >> 1;
    unsigned int total_pairs = (n_heads + n_kv_heads) * half;
    if (idx >= total_pairs) return;

    unsigned int pos = step_args[1];
    unsigned int max_seq = step_args[2];
    unsigned int h = idx / half;
    unsigned int p = idx % half;

    float exponent = -((float)(2U * p)) / (float)head_dim;
    float inv_freq = __ptx_ex2f(exponent * __ptx_lg2f(rope_theta));
    float theta = (float)pos * inv_freq;
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    unsigned int i0 = is_neox ? p : (2U * p);
    unsigned int i1 = is_neox ? (p + half) : (2U * p + 1U);

    if (h < n_heads) {
        unsigned int off0 = h * head_dim + i0;
        unsigned int off1 = h * head_dim + i1;
        float x0 = q[off0];
        float x1 = q[off1];
        if (bias_mask & 1U) {
            x0 += bq[off0];
            x1 += bq[off1];
        }
        q[off0] = x0 * c - x1 * s;
        q[off1] = x0 * s + x1 * c;
    } else {
        unsigned int kh = h - n_heads;
        unsigned int kvoff0 = kh * head_dim + i0;
        unsigned int kvoff1 = kh * head_dim + i1;
        float x0 = k[kvoff0];
        float x1 = k[kvoff1];
        if (bias_mask & 2U) {
            x0 += bk[kvoff0];
            x1 += bk[kvoff1];
        }
        float rk0 = x0 * c - x1 * s;
        float rk1 = x0 * s + x1 * c;
        float v0 = v[kvoff0];
        float v1 = v[kvoff1];
        if (bias_mask & 4U) {
            v0 += bv[kvoff0];
            v1 += bv[kvoff1];
        }
        unsigned int kv_dim = n_kv_heads * head_dim;
        unsigned long long slot = ((unsigned long long)layer * (unsigned long long)max_seq + (unsigned long long)pos)
                                * (unsigned long long)kv_dim;
        k_cache[slot + (unsigned long long)kvoff0] = rk0;
        k_cache[slot + (unsigned long long)kvoff1] = rk1;
        v_cache[slot + (unsigned long long)kvoff0] = v0;
        v_cache[slot + (unsigned long long)kvoff1] = v1;
    }
}

/* Zero-spill register-cached FlashAttention-2 GQA (step_args[1]=pos, step_args[2]=max_seq) */
void dec_attn_flash_ind_f16(
    const float *q,
    const unsigned short *k_cache,
    const unsigned short *v_cache,
    float *out,
    const unsigned int *step_args,
    unsigned int layer,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int pos = step_args[1];
    unsigned int max_seq = step_args[2];
    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    const float *qh = q + h * head_dim;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    unsigned long long kv_base = (unsigned long long)layer * (unsigned long long)max_seq * (unsigned long long)kv_dim
                               + (unsigned long long)(kv_h * head_dim) + (unsigned long long)lane;
    const unsigned short *kt = k_cache + kv_base;
    const unsigned short *vt = v_cache + kv_base;

    float m_max = -1e30f;
    float l_sum = 0.0f;

    if (head_dim == 128U) {
        float q0 = qh[lane];
        float q1 = qh[lane + 32U];
        float q2 = qh[lane + 64U];
        float q3 = qh[lane + 96U];
        float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        for (unsigned int t = 0U; t <= pos; t++) {
            float k0 = __ptx_f16_to_f32((unsigned int)kt[0]);
            float k1 = __ptx_f16_to_f32((unsigned int)kt[32]);
            float k2 = __ptx_f16_to_f32((unsigned int)kt[64]);
            float k3 = __ptx_f16_to_f32((unsigned int)kt[96]);
            float dot = __ptx_fmaf(q0, k0, __ptx_fmaf(q1, k1, __ptx_fmaf(q2, k2, q3 * k3)));
            float score = lwarp_sum_f32(dot) * inv_sqrt_hd;

            float alpha = 0.0f;
            float p_tv = 1.0f;
            if (t == 0U) {
                m_max = score;
                l_sum = 1.0f;
            } else {
                float m_new = __ptx_maxf(m_max, score);
                alpha = lora_expf(m_max - m_new);
                p_tv = lora_expf(score - m_new);
                l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
                m_max = m_new;
            }

            float v0 = __ptx_f16_to_f32((unsigned int)vt[0]);
            float v1 = __ptx_f16_to_f32((unsigned int)vt[32]);
            float v2 = __ptx_f16_to_f32((unsigned int)vt[64]);
            float v3 = __ptx_f16_to_f32((unsigned int)vt[96]);
            a0 = __ptx_fmaf(p_tv, v0, a0 * alpha);
            a1 = __ptx_fmaf(p_tv, v1, a1 * alpha);
            a2 = __ptx_fmaf(p_tv, v2, a2 * alpha);
            a3 = __ptx_fmaf(p_tv, v3, a3 * alpha);
            kt += kv_dim;
            vt += kv_dim;
        }
        float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
        oh[lane] = a0 * inv_l;
        oh[lane + 32U] = a1 * inv_l;
        oh[lane + 64U] = a2 * inv_l;
        oh[lane + 96U] = a3 * inv_l;
        return;
    }

    float q0 = (lane < head_dim) ? qh[lane] : 0.0f;
    float q1 = (lane + 32U < head_dim) ? qh[lane + 32U] : 0.0f;
    float q2 = (lane + 64U < head_dim) ? qh[lane + 64U] : 0.0f;
    float q3 = (lane + 96U < head_dim) ? qh[lane + 96U] : 0.0f;
    float q4 = (lane + 128U < head_dim) ? qh[lane + 128U] : 0.0f;
    float q5 = (lane + 160U < head_dim) ? qh[lane + 160U] : 0.0f;
    float q6 = (lane + 192U < head_dim) ? qh[lane + 192U] : 0.0f;
    float q7 = (lane + 224U < head_dim) ? qh[lane + 224U] : 0.0f;
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float a4 = 0.0f, a5 = 0.0f, a6 = 0.0f, a7 = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        float dot = 0.0f;
        if (lane < head_dim) dot = __ptx_fmaf(q0, __ptx_f16_to_f32((unsigned int)kt[0]), dot);
        if (lane + 32U < head_dim) dot = __ptx_fmaf(q1, __ptx_f16_to_f32((unsigned int)kt[32]), dot);
        if (lane + 64U < head_dim) dot = __ptx_fmaf(q2, __ptx_f16_to_f32((unsigned int)kt[64]), dot);
        if (lane + 96U < head_dim) dot = __ptx_fmaf(q3, __ptx_f16_to_f32((unsigned int)kt[96]), dot);
        if (lane + 128U < head_dim) dot = __ptx_fmaf(q4, __ptx_f16_to_f32((unsigned int)kt[128]), dot);
        if (lane + 160U < head_dim) dot = __ptx_fmaf(q5, __ptx_f16_to_f32((unsigned int)kt[160]), dot);
        if (lane + 192U < head_dim) dot = __ptx_fmaf(q6, __ptx_f16_to_f32((unsigned int)kt[192]), dot);
        if (lane + 224U < head_dim) dot = __ptx_fmaf(q7, __ptx_f16_to_f32((unsigned int)kt[224]), dot);
        float score = lwarp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = lora_expf(m_max - m_new);
            p_tv = lora_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        if (lane < head_dim) a0 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[0]), a0 * alpha);
        if (lane + 32U < head_dim) a1 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[32]), a1 * alpha);
        if (lane + 64U < head_dim) a2 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[64]), a2 * alpha);
        if (lane + 96U < head_dim) a3 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[96]), a3 * alpha);
        if (lane + 128U < head_dim) a4 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[128]), a4 * alpha);
        if (lane + 160U < head_dim) a5 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[160]), a5 * alpha);
        if (lane + 192U < head_dim) a6 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[192]), a6 * alpha);
        if (lane + 224U < head_dim) a7 = __ptx_fmaf(p_tv, __ptx_f16_to_f32((unsigned int)vt[224]), a7 * alpha);
        kt += kv_dim;
        vt += kv_dim;
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    if (lane < head_dim) oh[lane] = a0 * inv_l;
    if (lane + 32U < head_dim) oh[lane + 32U] = a1 * inv_l;
    if (lane + 64U < head_dim) oh[lane + 64U] = a2 * inv_l;
    if (lane + 96U < head_dim) oh[lane + 96U] = a3 * inv_l;
    if (lane + 128U < head_dim) oh[lane + 128U] = a4 * inv_l;
    if (lane + 160U < head_dim) oh[lane + 160U] = a5 * inv_l;
    if (lane + 192U < head_dim) oh[lane + 192U] = a6 * inv_l;
    if (lane + 224U < head_dim) oh[lane + 224U] = a7 * inv_l;
}

void dec_attn_flash_ind_f32(
    const float *q,
    const float *k_cache,
    const float *v_cache,
    float *out,
    const unsigned int *step_args,
    unsigned int layer,
    unsigned int n_heads,
    unsigned int n_kv_heads,
    unsigned int head_dim
) {
    unsigned int h = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (h >= n_heads) return;

    unsigned int pos = step_args[1];
    unsigned int max_seq = step_args[2];
    unsigned int kv_group = n_heads / n_kv_heads;
    unsigned int kv_h = h / kv_group;
    unsigned int kv_dim = n_kv_heads * head_dim;
    const float *qh = q + h * head_dim;
    float *oh = out + h * head_dim;
    float inv_sqrt_hd = __ptx_rsqrtf((float)head_dim);

    unsigned long long kv_base = (unsigned long long)layer * (unsigned long long)max_seq * (unsigned long long)kv_dim
                               + (unsigned long long)(kv_h * head_dim) + (unsigned long long)lane;
    const float *kt = k_cache + kv_base;
    const float *vt = v_cache + kv_base;

    float m_max = -1e30f;
    float l_sum = 0.0f;

    if (head_dim == 128U) {
        float q0 = qh[lane];
        float q1 = qh[lane + 32U];
        float q2 = qh[lane + 64U];
        float q3 = qh[lane + 96U];
        float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        for (unsigned int t = 0U; t <= pos; t++) {
            float dot = __ptx_fmaf(q0, kt[0], __ptx_fmaf(q1, kt[32], __ptx_fmaf(q2, kt[64], q3 * kt[96])));
            float score = lwarp_sum_f32(dot) * inv_sqrt_hd;

            float alpha = 0.0f;
            float p_tv = 1.0f;
            if (t == 0U) {
                m_max = score;
                l_sum = 1.0f;
            } else {
                float m_new = __ptx_maxf(m_max, score);
                alpha = lora_expf(m_max - m_new);
                p_tv = lora_expf(score - m_new);
                l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
                m_max = m_new;
            }

            a0 = __ptx_fmaf(p_tv, vt[0], a0 * alpha);
            a1 = __ptx_fmaf(p_tv, vt[32], a1 * alpha);
            a2 = __ptx_fmaf(p_tv, vt[64], a2 * alpha);
            a3 = __ptx_fmaf(p_tv, vt[96], a3 * alpha);
            kt += kv_dim;
            vt += kv_dim;
        }
        float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
        oh[lane] = a0 * inv_l;
        oh[lane + 32U] = a1 * inv_l;
        oh[lane + 64U] = a2 * inv_l;
        oh[lane + 96U] = a3 * inv_l;
        return;
    }

    float q0 = (lane < head_dim) ? qh[lane] : 0.0f;
    float q1 = (lane + 32U < head_dim) ? qh[lane + 32U] : 0.0f;
    float q2 = (lane + 64U < head_dim) ? qh[lane + 64U] : 0.0f;
    float q3 = (lane + 96U < head_dim) ? qh[lane + 96U] : 0.0f;
    float q4 = (lane + 128U < head_dim) ? qh[lane + 128U] : 0.0f;
    float q5 = (lane + 160U < head_dim) ? qh[lane + 160U] : 0.0f;
    float q6 = (lane + 192U < head_dim) ? qh[lane + 192U] : 0.0f;
    float q7 = (lane + 224U < head_dim) ? qh[lane + 224U] : 0.0f;
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float a4 = 0.0f, a5 = 0.0f, a6 = 0.0f, a7 = 0.0f;

    for (unsigned int t = 0U; t <= pos; t++) {
        float dot = 0.0f;
        if (lane < head_dim) dot = __ptx_fmaf(q0, kt[0], dot);
        if (lane + 32U < head_dim) dot = __ptx_fmaf(q1, kt[32], dot);
        if (lane + 64U < head_dim) dot = __ptx_fmaf(q2, kt[64], dot);
        if (lane + 96U < head_dim) dot = __ptx_fmaf(q3, kt[96], dot);
        if (lane + 128U < head_dim) dot = __ptx_fmaf(q4, kt[128], dot);
        if (lane + 160U < head_dim) dot = __ptx_fmaf(q5, kt[160], dot);
        if (lane + 192U < head_dim) dot = __ptx_fmaf(q6, kt[192], dot);
        if (lane + 224U < head_dim) dot = __ptx_fmaf(q7, kt[224], dot);
        float score = lwarp_sum_f32(dot) * inv_sqrt_hd;

        float alpha = 0.0f;
        float p_tv = 1.0f;
        if (t == 0U) {
            m_max = score;
            l_sum = 1.0f;
        } else {
            float m_new = __ptx_maxf(m_max, score);
            alpha = lora_expf(m_max - m_new);
            p_tv = lora_expf(score - m_new);
            l_sum = __ptx_fmaf(l_sum, alpha, p_tv);
            m_max = m_new;
        }

        if (lane < head_dim) a0 = __ptx_fmaf(p_tv, vt[0], a0 * alpha);
        if (lane + 32U < head_dim) a1 = __ptx_fmaf(p_tv, vt[32], a1 * alpha);
        if (lane + 64U < head_dim) a2 = __ptx_fmaf(p_tv, vt[64], a2 * alpha);
        if (lane + 96U < head_dim) a3 = __ptx_fmaf(p_tv, vt[96], a3 * alpha);
        if (lane + 128U < head_dim) a4 = __ptx_fmaf(p_tv, vt[128], a4 * alpha);
        if (lane + 160U < head_dim) a5 = __ptx_fmaf(p_tv, vt[160], a5 * alpha);
        if (lane + 192U < head_dim) a6 = __ptx_fmaf(p_tv, vt[192], a6 * alpha);
        if (lane + 224U < head_dim) a7 = __ptx_fmaf(p_tv, vt[224], a7 * alpha);
        kt += kv_dim;
        vt += kv_dim;
    }

    float inv_l = (l_sum > 0.0f) ? (1.0f / l_sum) : 0.0f;
    if (lane < head_dim) oh[lane] = a0 * inv_l;
    if (lane + 32U < head_dim) oh[lane + 32U] = a1 * inv_l;
    if (lane + 64U < head_dim) oh[lane + 64U] = a2 * inv_l;
    if (lane + 96U < head_dim) oh[lane + 96U] = a3 * inv_l;
    if (lane + 128U < head_dim) oh[lane + 128U] = a4 * inv_l;
    if (lane + 160U < head_dim) oh[lane + 160U] = a5 * inv_l;
    if (lane + 192U < head_dim) oh[lane + 192U] = a6 * inv_l;
    if (lane + 224U < head_dim) oh[lane + 224U] = a7 * inv_l;
}

/* Dynamic-scale device-indirect LoRA forward inference (step_args_f[3] = scale) */
void dec_lora_fwd_a_ind(
    const float *X,
    const float *A,
    float *H,
    const float *step_args_f,
    unsigned int r,
    unsigned int K
) {
    if (step_args_f[3] == 0.0f) return;
    unsigned int j = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    if (j >= r) return;
    const float *aj = A + (unsigned long long)j * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(X[k], aj[k], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) H[j] = sum;
}

void dec_lora_fwd_b_add_ind(
    const float *H,
    const float *B,
    float *Y,
    const float *step_args_f,
    unsigned int N,
    unsigned int r
) {
    float scale = step_args_f[3];
    if (scale == 0.0f) return;
    unsigned int n = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (n >= N) return;
    const float *bn = B + (unsigned long long)n * (unsigned long long)r;
    float acc = 0.0f;
    for (unsigned int j = 0U; j < r; j++) {
        acc = __ptx_fmaf(H[j], bn[j], acc);
    }
    Y[n] += scale * acc;
}

/* Fused Q+K+V LoRA A projection (grid = 3*r, block = 32); writes H_qkv[proj*64 + j] */
void dec_lora_qkv_a_ind(
    const float *X,
    const float *Aq,
    const float *Ak,
    const float *Av,
    float *H_qkv,
    const float *step_args_f,
    unsigned int r,
    unsigned int K
) {
    if (step_args_f[3] == 0.0f) return;
    unsigned int bid = __ptx_ctaid_x();
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int proj = bid / r;
    unsigned int j = bid % r;
    if (proj >= 3U) return;
    const float *A = (proj == 0U) ? Aq : ((proj == 1U) ? Ak : Av);
    const float *aj = A + (unsigned long long)j * (unsigned long long)K;
    float acc = 0.0f;
    for (unsigned int k = lane; k < K; k += 32U) {
        acc = __ptx_fmaf(X[k], aj[k], acc);
    }
    float sum = lwarp_sum_f32(acc);
    if (lane == 0U) H_qkv[(proj << 6) + j] = sum;
}

/* Fused Q+K+V LoRA B+add projection (grid = (q_dim + 2*kv_dim + 255)/256, block = 256) */
void dec_lora_qkv_b_add_ind(
    const float *H_qkv,
    const float *Bq,
    const float *Bk,
    const float *Bv,
    float *Q,
    float *K_out,
    float *V_out,
    const float *step_args_f,
    unsigned int q_dim,
    unsigned int kv_dim,
    unsigned int r
) {
    float scale = step_args_f[3];
    if (scale == 0.0f) return;
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int qk_dim = q_dim + kv_dim;
    unsigned int total = qk_dim + kv_dim;
    if (idx >= total) return;

    const float *H;
    const float *B;
    float *Y;
    unsigned int n;
    if (idx < q_dim) {
        H = H_qkv;
        B = Bq;
        Y = Q;
        n = idx;
    } else if (idx < qk_dim) {
        H = H_qkv + 64U;
        B = Bk;
        Y = K_out;
        n = idx - q_dim;
    } else {
        H = H_qkv + 128U;
        B = Bv;
        Y = V_out;
        n = idx - qk_dim;
    }
    const float *bn = B + (unsigned long long)n * (unsigned long long)r;
    float acc = 0.0f;
    for (unsigned int j = 0U; j < r; j++) {
        acc = __ptx_fmaf(H[j], bn[j], acc);
    }
    Y[n] += scale * acc;
}

/* 2-Stage Exact GPU ArgMax + Finite Check */
void dec_argmax_blocks(
    const float *logits,
    unsigned int *part_ids,
    float *part_vals,
    unsigned int *part_bad,
    unsigned int V
) {
    unsigned int gid = __ptx_ctaid_x() * 32U + (__ptx_tid_x() & 31U);
    if (gid >= V) {
        part_ids[gid] = 0xFFFFFFFFU;
        part_vals[gid] = -3.4e38f;
        part_bad[gid] = 0U;
        return;
    }
    unsigned int best_id = gid;
    float best_val = logits[gid];
    unsigned int bad = (!(best_val >= -1e38f && best_val <= 1e38f)) ? 1U : 0U;
    for (unsigned int i = gid + 2048U; i < V; i += 2048U) {
        float v = logits[i];
        if (!(v >= -1e38f && v <= 1e38f)) bad = 1U;
        if (v > best_val || (v == best_val && i < best_id)) {
            best_val = v;
            best_id = i;
        }
    }
    part_ids[gid] = best_id;
    part_vals[gid] = best_val;
    part_bad[gid] = bad;
}

void dec_argmax_finalize(
    unsigned int *part_ids,
    float *part_vals,
    unsigned int *part_bad,
    unsigned int *out_ids,
    float *out_vals,
    unsigned int V
) {
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int best_id = 0xFFFFFFFFU;
    float best_val = -3.4e38f;
    unsigned int bad = 0U;
    for (unsigned int k = lane; k < 2048U; k += 32U) {
        unsigned int id = part_ids[k];
        bad |= part_bad[k];
        if (id < V) {
            float v = part_vals[k];
            if (best_id == 0xFFFFFFFFU || v > best_val || (v == best_val && id < best_id)) {
                best_val = v;
                best_id = id;
            }
        }
    }
    __ptx_syncthreads();
    part_ids[lane] = best_id;
    part_vals[lane] = best_val;
    part_bad[lane] = bad;
    __ptx_syncthreads();
    if (lane == 0U) {
        unsigned int win_id = 0U;
        float win_val = -3.4e38f;
        unsigned int any_bad = 0U;
        int have = 0;
        for (unsigned int j = 0U; j < 32U; j++) {
            any_bad |= part_bad[j];
            unsigned int id = part_ids[j];
            if (id < V) {
                float v = part_vals[j];
                if (!have || v > win_val || (v == win_val && id < win_id)) {
                    win_val = v;
                    win_id = id;
                    have = 1;
                }
            }
        }
        out_ids[0] = win_id;
        out_ids[1] = any_bad;
        out_vals[0] = win_val;
    }
}
