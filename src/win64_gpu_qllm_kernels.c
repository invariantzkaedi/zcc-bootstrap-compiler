/*
 * ZKAEDI ZCC // Hybrid Quantum-Classical Transformer Blackwell Kernels (Track 13)
 * Compiled directly via `./zcc --target=ptx --ptx-arch=sm_120`
 * Zero nvcc, zero CUDA SDK, zero PyTorch, zero external runtime dependencies.
 *
 * Implements:
 *   - Complex Quantum Embeddings + Positional Phase-RoPE (fwd / bwd)
 *   - Hilbert-Sphere L2 Complex State Normalization (fwd / bwd)
 *   - N-Qubit Parameterized Quantum Circuits U_Q, U_K, U_V, U_f1, U_f2:
 *       RY, RZ, CNOT-ring, CP-ring + Exact Adjoint Uncomputation (bwd)
 *   - Per-Head Quantum Phase-RoPE (fwd / bwd)
 *   - Causal Complex Born-Rule Interference Attention (fwd / bwd_q / bwd_kv)
 *   - Optical Kerr Self-Phase Modulation Nonlinearity (fwd / bwd)
 *   - Hybrid Quadrature Linear + SiLU FFN + POVM Readout + CE Loss + AdamW
 */

unsigned int __ptx_tid_x(void);
unsigned int __ptx_ntid_x(void);
unsigned int __ptx_ctaid_x(void);
unsigned int __ptx_ctaid_y(void);
void __ptx_syncthreads(void);

float __ptx_sqrtf(float x);
float __ptx_sinf(float x);
float __ptx_cosf(float x);
float __ptx_ex2f(float x);
float __ptx_lg2f(float x);
float __ptx_maxf(float a, float b);
float __ptx_minf(float a, float b);
float __ptx_fmaf(float a, float b, float c);
float __ptx_shfl_down_f32(float val, unsigned int delta);
float __ptx_shfl_idx_f32(float val, unsigned int src_lane);
unsigned int __ptx_f32_to_f16(float x);
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

static float qfast_expf(float x) {
    return __ptx_ex2f(x * 1.4426950408889634f);
}

static float qfast_logf(float x) {
    float xc = (x > 1e-20f) ? x : 1e-20f;
    return __ptx_lg2f(xc) * 0.6931471805599453f;
}

static float qphase_omega(unsigned int idx, unsigned int dim) {
    /* pow(10000.0, -idx / dim) = 2^((-idx / dim) * log2(10000)) */
    float exponent = -((float)idx) / (float)dim;
    return __ptx_ex2f(exponent * 13.287712379549449f);
}

static float qwarp_sum_f32(float v) {
    v += __ptx_shfl_down_f32(v, 16U);
    v += __ptx_shfl_down_f32(v, 8U);
    v += __ptx_shfl_down_f32(v, 4U);
    v += __ptx_shfl_down_f32(v, 2U);
    v += __ptx_shfl_down_f32(v, 1U);
    return __ptx_shfl_idx_f32(v, 0U);
}

static float qwarp_max_f32(float v) {
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 16U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 8U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 4U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 2U));
    v = __ptx_maxf(v, __ptx_shfl_down_f32(v, 1U));
    return __ptx_shfl_idx_f32(v, 0U);
}

/* 1. Complex Quantum Embedding + Positional Phase-RoPE (fwd / bwd) */

void qllm_embed_qrope_fwd(
    const float *E_re,
    const float *E_im,
    const unsigned int *tokens,
    float *out_re,
    float *out_im,
    unsigned int BT,
    unsigned int T,
    unsigned int D,
    unsigned int pos_offset
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = BT * D;
    if (idx >= total) return;

    unsigned int bt = idx / D;
    unsigned int k = idx % D;
    unsigned int t = pos_offset + (bt % T);
    unsigned int tok = tokens[bt];
    unsigned long long e_off = (unsigned long long)tok * (unsigned long long)D + (unsigned long long)k;
    float er = E_re[e_off];
    float ei = E_im[e_off];

    float theta = (float)t * qphase_omega(k, D);
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);
    out_re[idx] = er * c - ei * s;
    out_im[idx] = er * s + ei * c;
}

void qllm_embed_qrope_bwd(
    const float *d_re,
    const float *d_im,
    const unsigned int *tokens,
    float *dE_re,
    float *dE_im,
    unsigned int BT,
    unsigned int T,
    unsigned int D,
    unsigned int V
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = V * D;
    if (idx >= total) return;

    unsigned int v = idx / D;
    unsigned int k = idx % D;
    float omega = qphase_omega(k, D);
    float acc_re = 0.0f;
    float acc_im = 0.0f;

    for (unsigned int bt = 0U; bt < BT; bt++) {
        if (tokens[bt] == v) {
            unsigned int t = bt % T;
            float theta = (float)t * omega;
            float c = __ptx_cosf(theta);
            float s = __ptx_sinf(theta);
            unsigned long long d_off = (unsigned long long)bt * (unsigned long long)D + (unsigned long long)k;
            float gr = d_re[d_off];
            float gi = d_im[d_off];
            acc_re = __ptx_fmaf(gr, c, acc_re);
            acc_re = __ptx_fmaf(gi, s, acc_re);
            acc_im = __ptx_fmaf(gi, c, acc_im);
            acc_im = __ptx_fmaf(-gr, s, acc_im);
        }
    }
    dE_re[idx] += acc_re;
    dE_im[idx] += acc_im;
}

/* 2. Hilbert-Sphere Complex State Normalization (1 warp/token, block=32) */

void qllm_cunit_norm_fwd(
    const float *x_re,
    const float *x_im,
    float *y_re,
    float *y_im,
    float *inv_norm,
    unsigned int BT,
    unsigned int D
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;

    float n2 = 0.0f;
    for (unsigned int k = lane; k < D; k += 32U) {
        float r = x_re[base + k];
        float im = x_im[base + k];
        n2 = __ptx_fmaf(r, r, n2);
        n2 = __ptx_fmaf(im, im, n2);
    }
    n2 = qwarp_sum_f32(n2);
    float inv = 1.0f / __ptx_sqrtf(n2 + 1e-6f);
    if (lane == 0U) inv_norm[bt] = inv;

    for (unsigned int k = lane; k < D; k += 32U) {
        y_re[base + k] = x_re[base + k] * inv;
        y_im[base + k] = x_im[base + k] * inv;
    }
}

void qllm_cunit_norm_bwd(
    const float *y_re,
    const float *y_im,
    const float *dy_re,
    const float *dy_im,
    const float *inv_norm,
    float *dx_re,
    float *dx_im,
    unsigned int BT,
    unsigned int D
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;

    float dot = 0.0f;
    for (unsigned int k = lane; k < D; k += 32U) {
        dot = __ptx_fmaf(dy_re[base + k], y_re[base + k], dot);
        dot = __ptx_fmaf(dy_im[base + k], y_im[base + k], dot);
    }
    dot = qwarp_sum_f32(dot);
    float inv = inv_norm[bt];

    for (unsigned int k = lane; k < D; k += 32U) {
        dx_re[base + k] = (dy_re[base + k] - y_re[base + k] * dot) * inv;
        dx_im[base + k] = (dy_im[base + k] - y_im[base + k] * dot) * inv;
    }
}

/* 3. N-Qubit Unitary Variational PQC (RY, RZ, CNOT-ring, CP-ring) + Exact Adjoint */

void qllm_pqc_fwd(
    const float *in_re,
    const float *in_im,
    float *out_re,
    float *out_im,
    const float *theta,
    unsigned int BT,
    unsigned int nq,
    unsigned int depth
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int D = 1U << nq;
    unsigned int halfD = D >> 1;
    unsigned int n_q_params = 3U * nq;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;

    const float *ir = in_re + base;
    const float *ii = in_im + base;
    float *or = out_re + base;
    float *oi = out_im + base;

    for (unsigned int i = lane; i < D; i += 32U) {
        or[i] = ir[i];
        oi[i] = ii[i];
    }
    __ptx_syncthreads();

    for (unsigned int p = 0U; p < depth; p++) {
        /* Layer step 1: RY(th) on all qubits 0..nq-1 */
        for (unsigned int q = 0U; q < nq; q++) {
            float ang = theta[p * n_q_params + q];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << q;
            unsigned int lmask = bit - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & lmask;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                unsigned int i1 = i0 | bit;
                float ar = or[i0];
                float br = or[i1];
                float ai = oi[i0];
                float bi = oi[i1];
                or[i0] = c * ar - s * br;
                or[i1] = s * ar + c * br;
                oi[i0] = c * ai - s * bi;
                oi[i1] = s * ai + c * bi;
            }
            __ptx_syncthreads();
        }

        /* Layer step 2: RZ(th) on all qubits 0..nq-1 */
        for (unsigned int q = 0U; q < nq; q++) {
            float ang = theta[p * n_q_params + nq + q];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << q;
            for (unsigned int i = lane; i < D; i += 32U) {
                float ar = or[i];
                float ai = oi[i];
                if ((i & bit) == 0U) {
                    or[i] = c * ar + s * ai;
                    oi[i] = c * ai - s * ar;
                } else {
                    or[i] = c * ar - s * ai;
                    oi[i] = s * ar + c * ai;
                }
            }
            __ptx_syncthreads();
        }

        /* Layer step 3: CNOT ring ctrl=q, tgt=(q+1)%nq */
        for (unsigned int q = 0U; q < nq; q++) {
            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
            unsigned int cmask = 1U << q;
            unsigned int tmask = 1U << tgt;
            unsigned int tlo = tmask - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & tlo;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                if ((i0 & cmask) != 0U) {
                    unsigned int i1 = i0 | tmask;
                    float tr = or[i0];
                    float ti = oi[i0];
                    or[i0] = or[i1];
                    oi[i0] = oi[i1];
                    or[i1] = tr;
                    oi[i1] = ti;
                }
            }
            __ptx_syncthreads();
        }

        /* Layer step 4: Controlled-Phase ring CP(alpha) */
        for (unsigned int q = 0U; q < nq; q++) {
            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
            float alpha = theta[p * n_q_params + 2U * nq + q];
            float ca = __ptx_cosf(alpha);
            float sa = __ptx_sinf(alpha);
            unsigned int cmask = 1U << q;
            unsigned int tmask = 1U << tgt;
            for (unsigned int i = lane; i < D; i += 32U) {
                if ((i & cmask) != 0U && (i & tmask) != 0U) {
                    float ar = or[i];
                    float ai = oi[i];
                    or[i] = ca * ar - sa * ai;
                    oi[i] = sa * ar + ca * ai;
                }
            }
            __ptx_syncthreads();
        }
    }
}

void qllm_pqc_bwd(
    const float *psi_out_re,
    const float *psi_out_im,
    const float *dout_re,
    const float *dout_im,
    float *psi_work_re,
    float *psi_work_im,
    float *din_re,
    float *din_im,
    const float *theta,
    float *dtheta_tok,
    unsigned int BT,
    unsigned int nq,
    unsigned int depth
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int D = 1U << nq;
    unsigned int halfD = D >> 1;
    unsigned int n_q_params = 3U * nq;
    unsigned int n_total_params = depth * n_q_params;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;

    const float *por = psi_out_re + base;
    const float *poi = psi_out_im + base;
    const float *dor = dout_re + base;
    const float *doi = dout_im + base;
    float *wr = psi_work_re + base;
    float *wi = psi_work_im + base;
    float *dr = din_re + base;
    float *di = din_im + base;
    float *dt = dtheta_tok + (unsigned long long)bt * (unsigned long long)n_total_params;

    for (unsigned int i = lane; i < D; i += 32U) {
        wr[i] = por[i];
        wi[i] = poi[i];
        dr[i] = dor[i];
        di[i] = doi[i];
    }
    __ptx_syncthreads();

    for (int p = (int)depth - 1; p >= 0; p--) {
        unsigned int pu = (unsigned int)p;

        /* Reverse step 4: Adjoint CP(alpha) */
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int tgt = (qu + 1U == nq) ? 0U : (qu + 1U);
            unsigned int pidx = pu * n_q_params + 2U * nq + qu;
            float alpha = theta[pidx];
            float ca = __ptx_cosf(alpha);
            float sa = __ptx_sinf(alpha);
            unsigned int cmask = 1U << qu;
            unsigned int tmask = 1U << tgt;
            float ga = 0.0f;

            for (unsigned int i = lane; i < D; i += 32U) {
                if ((i & cmask) != 0U && (i & tmask) != 0U) {
                    float pr = wr[i];
                    float pi = wi[i];
                    float cr = dr[i];
                    float ci = di[i];
                    ga = __ptx_fmaf(ci, pr, ga);
                    ga = __ptx_fmaf(-cr, pi, ga);
                    wr[i] = ca * pr + sa * pi;
                    wi[i] = ca * pi - sa * pr;
                    dr[i] = ca * cr + sa * ci;
                    di[i] = ca * ci - sa * cr;
                }
            }
            ga = qwarp_sum_f32(ga);
            if (lane == 0U) dt[pidx] = ga;
            __ptx_syncthreads();
        }

        /* Reverse step 3: Adjoint CNOT ring */
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int tgt = (qu + 1U == nq) ? 0U : (qu + 1U);
            unsigned int cmask = 1U << qu;
            unsigned int tmask = 1U << tgt;
            unsigned int tlo = tmask - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & tlo;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                if ((i0 & cmask) != 0U) {
                    unsigned int i1 = i0 | tmask;
                    float tr = wr[i0];
                    float ti = wi[i0];
                    wr[i0] = wr[i1];
                    wi[i0] = wi[i1];
                    wr[i1] = tr;
                    wi[i1] = ti;

                    tr = dr[i0];
                    ti = di[i0];
                    dr[i0] = dr[i1];
                    di[i0] = di[i1];
                    dr[i1] = tr;
                    di[i1] = ti;
                }
            }
            __ptx_syncthreads();
        }

        /* Reverse step 2: Adjoint RZ */
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int pidx = pu * n_q_params + nq + qu;
            float ang = theta[pidx];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << qu;
            float gz = 0.0f;

            for (unsigned int i = lane; i < D; i += 32U) {
                float pr = wr[i];
                float pi = wi[i];
                float cr = dr[i];
                float ci = di[i];
                if ((i & bit) == 0U) {
                    gz = __ptx_fmaf(cr, pi, gz);
                    gz = __ptx_fmaf(-ci, pr, gz);
                    wr[i] = c * pr - s * pi;
                    wi[i] = s * pr + c * pi;
                    dr[i] = c * cr - s * ci;
                    di[i] = s * cr + c * ci;
                } else {
                    gz = __ptx_fmaf(ci, pr, gz);
                    gz = __ptx_fmaf(-cr, pi, gz);
                    wr[i] = c * pr + s * pi;
                    wi[i] = c * pi - s * pr;
                    dr[i] = c * cr + s * ci;
                    di[i] = c * ci - s * cr;
                }
            }
            gz = qwarp_sum_f32(gz) * 0.5f;
            if (lane == 0U) dt[pidx] = gz;
            __ptx_syncthreads();
        }

        /* Reverse step 1: Adjoint RY */
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int pidx = pu * n_q_params + qu;
            float ang = theta[pidx];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << qu;
            unsigned int lmask = bit - 1U;
            float gy = 0.0f;

            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & lmask;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                unsigned int i1 = i0 | bit;
                float p0r = wr[i0];
                float p1r = wr[i1];
                float p0i = wi[i0];
                float p1i = wi[i1];
                float c0r = dr[i0];
                float c1r = dr[i1];
                float c0i = di[i0];
                float c1i = di[i1];

                gy += (c1r * p0r - c0r * p1r) + (c1i * p0i - c0i * p1i);

                wr[i0] = c * p0r + s * p1r;
                wr[i1] = c * p1r - s * p0r;
                wi[i0] = c * p0i + s * p1i;
                wi[i1] = c * p1i - s * p0i;

                dr[i0] = c * c0r + s * c1r;
                dr[i1] = c * c1r - s * c0r;
                di[i0] = c * c0i + s * c1i;
                di[i1] = c * c1i - s * c0i;
            }
            gy = qwarp_sum_f32(gy) * 0.5f;
            if (lane == 0U) dt[pidx] = gy;
            __ptx_syncthreads();
        }
    }
}

/* 4. Per-Head Quantum Phase-RoPE (fwd / bwd) */

void qllm_qrope_heads_fwd(
    const float *qi_re,
    const float *qi_im,
    const float *ki_re,
    const float *ki_im,
    float *qo_re,
    float *qo_im,
    float *ko_re,
    float *ko_im,
    unsigned int BT,
    unsigned int T,
    unsigned int D,
    unsigned int Dh,
    unsigned int pos_offset
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = BT * D;
    if (idx >= total) return;

    unsigned int bt = idx / D;
    unsigned int t = pos_offset + (bt % T);
    unsigned int j = (idx % D) % Dh;
    float theta = (float)t * qphase_omega(j, Dh);
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    float qr = qi_re[idx];
    float qi = qi_im[idx];
    qo_re[idx] = qr * c - qi * s;
    qo_im[idx] = qr * s + qi * c;

    float kr = ki_re[idx];
    float ki = ki_im[idx];
    ko_re[idx] = kr * c - ki * s;
    ko_im[idx] = kr * s + ki * c;
}

void qllm_qrope_heads_bwd(
    float *dq_re,
    float *dq_im,
    float *dk_re,
    float *dk_im,
    unsigned int BT,
    unsigned int T,
    unsigned int D,
    unsigned int Dh
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = BT * D;
    if (idx >= total) return;

    unsigned int bt = idx / D;
    unsigned int t = bt % T;
    unsigned int j = (idx % D) % Dh;
    float theta = (float)t * qphase_omega(j, Dh);
    float c = __ptx_cosf(theta);
    float s = __ptx_sinf(theta);

    float qr = dq_re[idx];
    float qi = dq_im[idx];
    dq_re[idx] = qr * c + qi * s;
    dq_im[idx] = qi * c - qr * s;

    float kr = dk_re[idx];
    float ki = dk_im[idx];
    dk_re[idx] = kr * c + ki * s;
    dk_im[idx] = ki * c - kr * s;
}

/* 5. Causal Complex Born-Rule Interference Attention (fwd / bwd_q / bwd_kv) */

void qllm_born_attn_fwd(
    const float *Q_re,
    const float *Q_im,
    const float *K_re,
    const float *K_im,
    const float *V_re,
    const float *V_im,
    const float *beta,
    const float *gamma,
    float *att_re,
    float *att_im,
    float *att_w,
    float *out_re,
    float *out_im,
    unsigned int B,
    unsigned int T,
    unsigned int H,
    unsigned int Dh
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int bh = __ptx_ctaid_y();
    if (t >= T || bh >= B * H) return;
    unsigned int lane = __ptx_tid_x() & 31U;

    unsigned int b = bh / H;
    unsigned int h = bh % H;
    unsigned int D = H * Dh;
    float b_val = beta[h];
    float g_val = gamma[h];
    float inv_sqrt_dh = 1.0f / __ptx_sqrtf((float)Dh);

    unsigned long long q_off = ((unsigned long long)(b * T + t)) * (unsigned long long)D + (unsigned long long)(h * Dh);
    unsigned long long att_base = ((unsigned long long)(bh * T + t)) * (unsigned long long)T;

    for (unsigned int s = 0U; s <= t; s++) {
        unsigned long long k_off = ((unsigned long long)(b * T + s)) * (unsigned long long)D + (unsigned long long)(h * Dh);
        float rp = 0.0f;
        float ip = 0.0f;
        for (unsigned int j = lane; j < Dh; j += 32U) {
            float qr = Q_re[q_off + j];
            float qi = Q_im[q_off + j];
            float kr = K_re[k_off + j];
            float ki = K_im[k_off + j];
            rp += kr * qr + ki * qi;
            ip += kr * qi - ki * qr;
        }
        float R = qwarp_sum_f32(rp);
        float I = qwarp_sum_f32(ip);
        if (lane == 0U) {
            att_re[att_base + s] = R;
            att_im[att_base + s] = I;
            att_w[att_base + s] = (b_val * (R * R + I * I) + g_val * R) * inv_sqrt_dh;
        }
    }
    __ptx_syncthreads();

    float lmax = -1e30f;
    for (unsigned int base = 0U; base <= t; base += 32U) {
        unsigned int s = base + lane;
        if (s <= t) {
            lmax = __ptx_maxf(lmax, att_w[att_base + s]);
        }
    }
    float maxv = qwarp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int base = 0U; base <= t; base += 32U) {
        unsigned int s = base + lane;
        if (s <= t) {
            float e = qfast_expf(att_w[att_base + s] - maxv);
            att_w[att_base + s] = e;
            lsum += e;
        }
    }
    float sumv = qwarp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    for (unsigned int base = 0U; base <= t; base += 32U) {
        unsigned int s = base + lane;
        if (s <= t) {
            att_w[att_base + s] *= inv_sum;
        }
    }
    __ptx_syncthreads();

    for (unsigned int j = lane; j < Dh; j += 32U) {
        float ar = 0.0f;
        float ai = 0.0f;
        for (unsigned int s = 0U; s <= t; s++) {
            unsigned long long v_off = ((unsigned long long)(b * T + s)) * (unsigned long long)D + (unsigned long long)(h * Dh);
            float w = att_w[att_base + s];
            ar = __ptx_fmaf(w, V_re[v_off + j], ar);
            ai = __ptx_fmaf(w, V_im[v_off + j], ai);
        }
        out_re[q_off + j] = ar;
        out_im[q_off + j] = ai;
    }
}

void qllm_born_attn_bwd_q(
    const float *K_re,
    const float *K_im,
    const float *V_re,
    const float *V_im,
    const float *beta,
    const float *gamma,
    const float *att_re,
    const float *att_im,
    const float *att_w,
    const float *dout_re,
    const float *dout_im,
    float *att_dS,
    float *dQ_re,
    float *dQ_im,
    float *dbeta_tok,
    float *dgamma_tok,
    unsigned int B,
    unsigned int T,
    unsigned int H,
    unsigned int Dh
) {
    unsigned int t = __ptx_ctaid_x();
    unsigned int bh = __ptx_ctaid_y();
    if (t >= T || bh >= B * H) return;
    unsigned int lane = __ptx_tid_x() & 31U;

    unsigned int b = bh / H;
    unsigned int h = bh % H;
    unsigned int D = H * Dh;
    float b_val = beta[h];
    float g_val = gamma[h];
    float inv_sqrt_dh = 1.0f / __ptx_sqrtf((float)Dh);

    unsigned long long q_off = ((unsigned long long)(b * T + t)) * (unsigned long long)D + (unsigned long long)(h * Dh);
    unsigned long long att_base = ((unsigned long long)(bh * T + t)) * (unsigned long long)T;

    /* Pass 1: dw[s] stored temporarily in att_dS[att_base + s] */
    for (unsigned int s = 0U; s <= t; s++) {
        unsigned long long v_off = ((unsigned long long)(b * T + s)) * (unsigned long long)D + (unsigned long long)(h * Dh);
        float dwp = 0.0f;
        for (unsigned int j = lane; j < Dh; j += 32U) {
            dwp = __ptx_fmaf(dout_re[q_off + j], V_re[v_off + j], dwp);
            dwp = __ptx_fmaf(dout_im[q_off + j], V_im[v_off + j], dwp);
        }
        float dw = qwarp_sum_f32(dwp);
        if (lane == 0U) att_dS[att_base + s] = dw;
    }
    __ptx_syncthreads();

    /* Pass 2: sum_wdw = sum_{s<=t} w[s] * dw[s] */
    float sw_part = 0.0f;
    for (unsigned int base = 0U; base <= t; base += 32U) {
        unsigned int s = base + lane;
        if (s <= t) {
            sw_part = __ptx_fmaf(att_w[att_base + s], att_dS[att_base + s], sw_part);
        }
    }
    float sum_wdw = qwarp_sum_f32(sw_part);

    /* Pass 3: compute final dS[s] and accumulate per-(b,h,t) dbeta & dgamma */
    float db_part = 0.0f;
    float dg_part = 0.0f;
    for (unsigned int base = 0U; base <= t; base += 32U) {
        unsigned int s = base + lane;
        if (s <= t) {
            float w = att_w[att_base + s];
            float dw = att_dS[att_base + s];
            float dS = w * (dw - sum_wdw);
            att_dS[att_base + s] = dS;
            float R = att_re[att_base + s];
            float I = att_im[att_base + s];
            db_part = __ptx_fmaf(dS, (R * R + I * I) * inv_sqrt_dh, db_part);
            dg_part = __ptx_fmaf(dS, R * inv_sqrt_dh, dg_part);
        }
    }
    float db_tot = qwarp_sum_f32(db_part);
    float dg_tot = qwarp_sum_f32(dg_part);
    if (lane == 0U) {
        dbeta_tok[bh * T + t] = db_tot;
        dgamma_tok[bh * T + t] = dg_tot;
    }
    __ptx_syncthreads();

    /* Pass 4: dQ */
    for (unsigned int j = lane; j < Dh; j += 32U) {
        float dqr = 0.0f;
        float dqi = 0.0f;
        for (unsigned int s = 0U; s <= t; s++) {
            float dS = att_dS[att_base + s];
            float R = att_re[att_base + s];
            float I = att_im[att_base + s];
            float gR = dS * (2.0f * b_val * R + g_val) * inv_sqrt_dh;
            float gI = dS * (2.0f * b_val * I) * inv_sqrt_dh;
            unsigned long long k_off = ((unsigned long long)(b * T + s)) * (unsigned long long)D + (unsigned long long)(h * Dh);
            float kr = K_re[k_off + j];
            float ki = K_im[k_off + j];
            dqr += gR * kr - gI * ki;
            dqi += gR * ki + gI * kr;
        }
        dQ_re[q_off + j] = dqr;
        dQ_im[q_off + j] = dqi;
    }
}

void qllm_born_attn_bwd_kv(
    const float *Q_re,
    const float *Q_im,
    const float *beta,
    const float *gamma,
    const float *att_re,
    const float *att_im,
    const float *att_w,
    const float *att_dS,
    const float *dout_re,
    const float *dout_im,
    float *dK_re,
    float *dK_im,
    float *dV_re,
    float *dV_im,
    unsigned int B,
    unsigned int T,
    unsigned int H,
    unsigned int Dh
) {
    unsigned int s = __ptx_ctaid_x();
    unsigned int bh = __ptx_ctaid_y();
    if (s >= T || bh >= B * H) return;
    unsigned int lane = __ptx_tid_x() & 31U;

    unsigned int b = bh / H;
    unsigned int h = bh % H;
    unsigned int D = H * Dh;
    float b_val = beta[h];
    float g_val = gamma[h];
    float inv_sqrt_dh = 1.0f / __ptx_sqrtf((float)Dh);

    unsigned long long kv_off = ((unsigned long long)(b * T + s)) * (unsigned long long)D + (unsigned long long)(h * Dh);

    for (unsigned int j = lane; j < Dh; j += 32U) {
        float dkr = 0.0f;
        float dki = 0.0f;
        float dvr = 0.0f;
        float dvi = 0.0f;
        for (unsigned int t = s; t < T; t++) {
            unsigned long long att_idx = ((unsigned long long)(bh * T + t)) * (unsigned long long)T + (unsigned long long)s;
            unsigned long long q_off = ((unsigned long long)(b * T + t)) * (unsigned long long)D + (unsigned long long)(h * Dh);
            float w = att_w[att_idx];
            float dor = dout_re[q_off + j];
            float doi = dout_im[q_off + j];
            dvr = __ptx_fmaf(w, dor, dvr);
            dvi = __ptx_fmaf(w, doi, dvi);

            float dS = att_dS[att_idx];
            float R = att_re[att_idx];
            float I = att_im[att_idx];
            float gR = dS * (2.0f * b_val * R + g_val) * inv_sqrt_dh;
            float gI = dS * (2.0f * b_val * I) * inv_sqrt_dh;
            float qr = Q_re[q_off + j];
            float qi = Q_im[q_off + j];
            dkr += gR * qr + gI * qi;
            dki += gR * qi - gI * qr;
        }
        dK_re[kv_off + j] = dkr;
        dK_im[kv_off + j] = dki;
        dV_re[kv_off + j] = dvr;
        dV_im[kv_off + j] = dvi;
    }
}

/* 6. Optical Kerr Self-Phase Modulation (fwd / bwd) */

void qllm_kerr_fwd(
    const float *u_re,
    const float *u_im,
    const float *kerr_k,
    const float *kerr_b,
    float *v_re,
    float *v_im,
    unsigned int BT,
    unsigned int D
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * D) return;
    unsigned int k = idx % D;
    float ur = u_re[idx];
    float ui = u_im[idx];
    float ik = ur * ur + ui * ui;
    float phi = kerr_k[k] * ((float)D * ik) + kerr_b[k];
    float c = __ptx_cosf(phi);
    float s = __ptx_sinf(phi);
    v_re[idx] = ur * c - ui * s;
    v_im[idx] = ur * s + ui * c;
}

void qllm_kerr_bwd(
    const float *u_re,
    const float *u_im,
    const float *v_re,
    const float *v_im,
    const float *dv_re,
    const float *dv_im,
    const float *kerr_k,
    const float *kerr_b,
    float *du_re,
    float *du_im,
    float *dk_tok,
    float *db_tok,
    unsigned int BT,
    unsigned int D
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * D) return;
    unsigned int k = idx % D;
    float ur = u_re[idx];
    float ui = u_im[idx];
    float vr = v_re[idx];
    float vi = v_im[idx];
    float dvr = dv_re[idx];
    float dvi = dv_im[idx];
    float kk = kerr_k[k];
    float kb = kerr_b[k];

    float ik = ur * ur + ui * ui;
    float phi = kk * ((float)D * ik) + kb;
    float c = __ptx_cosf(phi);
    float s = __ptx_sinf(phi);

    float gphi = dvi * vr - dvr * vi;
    dk_tok[idx] = gphi * ((float)D * ik);
    db_tok[idx] = gphi;

    float scale_phi = gphi * (2.0f * (float)D * kk);
    du_re[idx] = (dvr * c + dvi * s) + scale_phi * ur;
    du_im[idx] = (dvi * c - dvr * s) + scale_phi * ui;
}

/* 7. Quadrature Pack / Unpack ([re, im] <-> [2*D]) */

void qllm_pack_quad(
    const float *re,
    const float *im,
    float *quad,
    unsigned int BT,
    unsigned int D,
    float scale
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * D) return;
    unsigned int bt = idx / D;
    unsigned int k = idx % D;
    unsigned long long qbase = (unsigned long long)bt * (unsigned long long)(2U * D);
    quad[qbase + (unsigned long long)k] = re[idx] * scale;
    quad[qbase + (unsigned long long)D + (unsigned long long)k] = im[idx] * scale;
}

void qllm_unpack_quad(
    const float *quad,
    float *re,
    float *im,
    unsigned int BT,
    unsigned int D,
    float scale
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * D) return;
    unsigned int bt = idx / D;
    unsigned int k = idx % D;
    unsigned long long qbase = (unsigned long long)bt * (unsigned long long)(2U * D);
    re[idx] = quad[qbase + (unsigned long long)k] * scale;
    im[idx] = quad[qbase + (unsigned long long)D + (unsigned long long)k] * scale;
}

/* 8. Linear Forward & Race-Free Backward (W: [N, K], X: [BT, K], Y: [BT, N]) */

void qllm_linear_fwd(
    const float *X,
    const float *W,
    const float *b,
    float *Y,
    unsigned int BT,
    unsigned int N,
    unsigned int K
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * N) return;
    unsigned int bt = idx / N;
    unsigned int n = idx % N;
    const float *xr = X + (unsigned long long)bt * (unsigned long long)K;
    const float *wr = W + (unsigned long long)n * (unsigned long long)K;
    float acc = b[n];
    for (unsigned int k = 0U; k < K; k++) {
        acc = __ptx_fmaf(xr[k], wr[k], acc);
    }
    Y[idx] = acc;
}

void qllm_linear_bwd_x(
    const float *dY,
    const float *W,
    float *dX,
    unsigned int BT,
    unsigned int N,
    unsigned int K
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= BT * K) return;
    unsigned int bt = idx / K;
    unsigned int k = idx % K;
    const float *dyr = dY + (unsigned long long)bt * (unsigned long long)N;
    float acc = 0.0f;
    for (unsigned int n = 0U; n < N; n++) {
        acc = __ptx_fmaf(dyr[n], W[(unsigned long long)n * (unsigned long long)K + (unsigned long long)k], acc);
    }
    dX[idx] = acc;
}

void qllm_linear_bwd_w(
    const float *dY,
    const float *X,
    float *dW,
    unsigned int BT,
    unsigned int N,
    unsigned int K
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= N * K) return;
    unsigned int n = idx / K;
    unsigned int k = idx % K;
    float acc = 0.0f;
    for (unsigned int bt = 0U; bt < BT; bt++) {
        float dy = dY[(unsigned long long)bt * (unsigned long long)N + (unsigned long long)n];
        float xv = X[(unsigned long long)bt * (unsigned long long)K + (unsigned long long)k];
        acc = __ptx_fmaf(dy, xv, acc);
    }
    dW[idx] += acc;
}

void qllm_linear_bwd_b(
    const float *dY,
    float *db,
    unsigned int BT,
    unsigned int N
) {
    unsigned int n = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (n >= N) return;
    float acc = 0.0f;
    for (unsigned int bt = 0U; bt < BT; bt++) {
        acc += dY[(unsigned long long)bt * (unsigned long long)N + (unsigned long long)n];
    }
    db[n] += acc;
}

/* 9. SiLU Activation (fwd / bwd) & Vector Residual Adds */

void qllm_silu_fwd(const float *x, float *y, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float xv = x[i];
    float sig = 1.0f / (1.0f + qfast_expf(-xv));
    y[i] = xv * sig;
}

void qllm_silu_bwd(const float *x, const float *dy, float *dx, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float xv = x[i];
    float sig = 1.0f / (1.0f + qfast_expf(-xv));
    float dsilu = sig * (1.0f + xv * (1.0f - sig));
    dx[i] = dy[i] * dsilu;
}

void qllm_add_f32(const float *a, const float *b, float *out, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) out[i] = a[i] + b[i];
}

void qllm_add_inplace_f32(float *dst, const float *src, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) dst[i] += src[i];
}

void qllm_zero_f32(float *dst, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) dst[i] = 0.0f;
}

/* 10. Deterministic Gradient Reductions (Zero Atomics) */

void qllm_reduce_rows_add(const float *src, float *dst, unsigned int rows, unsigned int cols) {
    unsigned int c = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (c >= cols) return;
    float acc = 0.0f;
    for (unsigned int r = 0U; r < rows; r++) {
        acc += src[(unsigned long long)r * (unsigned long long)cols + (unsigned long long)c];
    }
    dst[c] += acc;
}

void qllm_reduce_head_add(const float *src, float *dst, unsigned int B, unsigned int T, unsigned int H) {
    unsigned int h = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (h >= H) return;
    float acc = 0.0f;
    for (unsigned int b = 0U; b < B; b++) {
        unsigned long long row = (unsigned long long)(b * H + h) * (unsigned long long)T;
        for (unsigned int t = 0U; t < T; t++) {
            acc += src[row + (unsigned long long)t];
        }
    }
    dst[h] += acc;
}

/* 11. Mean Cross-Entropy Loss + dLogits & AdamW Step */

void qllm_ce_loss_bwd(
    const float *logits,
    const unsigned int *targets,
    float *dlogits,
    float *losses,
    unsigned int BT,
    unsigned int V
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)V;

    float lmax = -1e30f;
    for (unsigned int v = lane; v < V; v += 32U) {
        lmax = __ptx_maxf(lmax, logits[base + v]);
    }
    float maxv = qwarp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int v = lane; v < V; v += 32U) {
        float e = qfast_expf(logits[base + v] - maxv);
        dlogits[base + v] = e;
        lsum += e;
    }
    float sumv = qwarp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    float inv_bt = 1.0f / (float)BT;
    unsigned int tgt = targets[bt];
    if (lane == 0U) {
        float tlog = logits[base + tgt] - maxv;
        losses[bt] = qfast_logf(sumv) - tlog;
    }
    for (unsigned int v = lane; v < V; v += 32U) {
        float p = dlogits[base + v] * inv_sum;
        float sub = (v == tgt) ? 1.0f : 0.0f;
        dlogits[base + v] = (p - sub) * inv_bt;
    }
}

void qllm_adamw_step(
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
    float bc2_inv,
    float grad_clip
) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i >= n) return;
    float g = grad[i];
    if (grad_clip > 0.0f) {
        if (g > grad_clip) g = grad_clip;
        if (g < -grad_clip) g = -grad_clip;
    }
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

/* ============================================================================
 * Track 15: SM 12.0 Tensor Core MMA, Multi-Register PQCs & Bridge, KV Cache
 * ============================================================================ */

/* 12. FP32 -> FP16 Conversion, Transpose, SM 12.0 Tensor Core GEMM & Bias */

void qllm_f32_to_f16(const float *src, unsigned short *dst, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) {
        dst[i] = (unsigned short)__ptx_f32_to_f16(src[i]);
    }
}

void qllm_transpose_f32_to_f16(
    const float *src,
    unsigned short *dst,
    unsigned int rows,
    unsigned int cols
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    unsigned int total = rows * cols;
    if (idx >= total) return;
    unsigned int r = idx / cols;
    unsigned int c = idx % cols;
    dst[(unsigned long long)c * (unsigned long long)rows + (unsigned long long)r] = (unsigned short)__ptx_f32_to_f16(src[idx]);
}

void qllm_gemm_tc_f16(
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

void qllm_add_bias_rows(
    float *Y,
    const float *b,
    unsigned int M,
    unsigned int N
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx < M * N) {
        Y[idx] += b[idx % N];
    }
}

/* 13. Multi-Register Variational PQCs & Cyclic Givens Entangling Bridge */

void qllm_pqc_mr_fwd(
    const float *in_re,
    const float *in_im,
    float *out_re,
    float *out_im,
    const float *theta,
    unsigned int BT,
    unsigned int n_reg,
    unsigned int nq,
    unsigned int depth
) {
    unsigned int btr = __ptx_ctaid_x();
    if (btr >= BT * n_reg) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int r = btr % n_reg;
    unsigned int D = 1U << nq;
    unsigned int halfD = D >> 1;
    unsigned int n_q_params = 3U * nq;
    unsigned int n_reg_params = depth * n_q_params;
    unsigned long long base = (unsigned long long)btr * (unsigned long long)D;

    const float *ir = in_re + base;
    const float *ii = in_im + base;
    float *or = out_re + base;
    float *oi = out_im + base;
    const float *th = theta + (unsigned long long)r * (unsigned long long)n_reg_params;

    for (unsigned int i = lane; i < D; i += 32U) {
        or[i] = ir[i];
        oi[i] = ii[i];
    }
    __ptx_syncthreads();

    for (unsigned int p = 0U; p < depth; p++) {
        for (unsigned int q = 0U; q < nq; q++) {
            float ang = th[p * n_q_params + q];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << q;
            unsigned int lmask = bit - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & lmask;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                unsigned int i1 = i0 | bit;
                float ar = or[i0], br = or[i1];
                float ai = oi[i0], bi = oi[i1];
                or[i0] = c * ar - s * br;
                or[i1] = s * ar + c * br;
                oi[i0] = c * ai - s * bi;
                oi[i1] = s * ai + c * bi;
            }
            __ptx_syncthreads();
        }
        for (unsigned int q = 0U; q < nq; q++) {
            float ang = th[p * n_q_params + nq + q];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << q;
            for (unsigned int i = lane; i < D; i += 32U) {
                float ar = or[i], ai = oi[i];
                if ((i & bit) == 0U) {
                    or[i] = c * ar + s * ai;
                    oi[i] = c * ai - s * ar;
                } else {
                    or[i] = c * ar - s * ai;
                    oi[i] = s * ar + c * ai;
                }
            }
            __ptx_syncthreads();
        }
        for (unsigned int q = 0U; q < nq; q++) {
            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
            unsigned int cmask = 1U << q;
            unsigned int tmask = 1U << tgt;
            unsigned int tlo = tmask - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & tlo;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                if ((i0 & cmask) != 0U) {
                    unsigned int i1 = i0 | tmask;
                    float tr = or[i0], ti = oi[i0];
                    or[i0] = or[i1]; oi[i0] = oi[i1];
                    or[i1] = tr;     oi[i1] = ti;
                }
            }
            __ptx_syncthreads();
        }
        for (unsigned int q = 0U; q < nq; q++) {
            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
            float alpha = th[p * n_q_params + 2U * nq + q];
            float ca = __ptx_cosf(alpha);
            float sa = __ptx_sinf(alpha);
            unsigned int cmask = 1U << q;
            unsigned int tmask = 1U << tgt;
            for (unsigned int i = lane; i < D; i += 32U) {
                if ((i & cmask) != 0U && (i & tmask) != 0U) {
                    float ar = or[i], ai = oi[i];
                    or[i] = ca * ar - sa * ai;
                    oi[i] = sa * ar + ca * ai;
                }
            }
            __ptx_syncthreads();
        }
    }
}

void qllm_pqc_mr_bwd(
    const float *psi_out_re,
    const float *psi_out_im,
    const float *dout_re,
    const float *dout_im,
    float *psi_work_re,
    float *psi_work_im,
    float *din_re,
    float *din_im,
    const float *theta,
    float *dtheta_tok,
    unsigned int BT,
    unsigned int n_reg,
    unsigned int nq,
    unsigned int depth
) {
    unsigned int btr = __ptx_ctaid_x();
    if (btr >= BT * n_reg) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int bt = btr / n_reg;
    unsigned int r = btr % n_reg;
    unsigned int D = 1U << nq;
    unsigned int halfD = D >> 1;
    unsigned int n_q_params = 3U * nq;
    unsigned int n_reg_params = depth * n_q_params;
    unsigned int total_pqc_stride = n_reg * n_reg_params + ((n_reg > 1U) ? n_reg : 0U);
    unsigned long long base = (unsigned long long)btr * (unsigned long long)D;

    const float *por = psi_out_re + base;
    const float *poi = psi_out_im + base;
    const float *dor = dout_re + base;
    const float *doi = dout_im + base;
    float *wr = psi_work_re + base;
    float *wi = psi_work_im + base;
    float *dr = din_re + base;
    float *di = din_im + base;
    const float *th = theta + (unsigned long long)r * (unsigned long long)n_reg_params;
    float *dt = dtheta_tok + (unsigned long long)bt * (unsigned long long)total_pqc_stride
              + (unsigned long long)r * (unsigned long long)n_reg_params;

    for (unsigned int i = lane; i < D; i += 32U) {
        wr[i] = por[i]; wi[i] = poi[i];
        dr[i] = dor[i]; di[i] = doi[i];
    }
    __ptx_syncthreads();

    for (int p = (int)depth - 1; p >= 0; p--) {
        unsigned int pu = (unsigned int)p;
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int tgt = (qu + 1U == nq) ? 0U : (qu + 1U);
            unsigned int pidx = pu * n_q_params + 2U * nq + qu;
            float alpha = th[pidx];
            float ca = __ptx_cosf(alpha);
            float sa = __ptx_sinf(alpha);
            unsigned int cmask = 1U << qu;
            unsigned int tmask = 1U << tgt;
            float ga = 0.0f;
            for (unsigned int i = lane; i < D; i += 32U) {
                if ((i & cmask) != 0U && (i & tmask) != 0U) {
                    float pr = wr[i], pi = wi[i];
                    float cr = dr[i], ci = di[i];
                    ga = __ptx_fmaf(ci, pr, ga);
                    ga = __ptx_fmaf(-cr, pi, ga);
                    wr[i] = ca * pr + sa * pi;
                    wi[i] = ca * pi - sa * pr;
                    dr[i] = ca * cr + sa * ci;
                    di[i] = ca * ci - sa * cr;
                }
            }
            ga = qwarp_sum_f32(ga);
            if (lane == 0U) dt[pidx] = ga;
            __ptx_syncthreads();
        }
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int tgt = (qu + 1U == nq) ? 0U : (qu + 1U);
            unsigned int cmask = 1U << qu;
            unsigned int tmask = 1U << tgt;
            unsigned int tlo = tmask - 1U;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & tlo;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                if ((i0 & cmask) != 0U) {
                    unsigned int i1 = i0 | tmask;
                    float tr = wr[i0], ti = wi[i0];
                    wr[i0] = wr[i1]; wi[i0] = wi[i1];
                    wr[i1] = tr;     wi[i1] = ti;
                    tr = dr[i0]; ti = di[i0];
                    dr[i0] = dr[i1]; di[i0] = di[i1];
                    dr[i1] = tr;     di[i1] = ti;
                }
            }
            __ptx_syncthreads();
        }
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int pidx = pu * n_q_params + nq + qu;
            float ang = th[pidx];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << qu;
            float gz = 0.0f;
            for (unsigned int i = lane; i < D; i += 32U) {
                float pr = wr[i], pi = wi[i];
                float cr = dr[i], ci = di[i];
                if ((i & bit) == 0U) {
                    gz = __ptx_fmaf(cr, pi, gz);
                    gz = __ptx_fmaf(-ci, pr, gz);
                    wr[i] = c * pr - s * pi; wi[i] = s * pr + c * pi;
                    dr[i] = c * cr - s * ci; di[i] = s * cr + c * ci;
                } else {
                    gz = __ptx_fmaf(ci, pr, gz);
                    gz = __ptx_fmaf(-cr, pi, gz);
                    wr[i] = c * pr + s * pi; wi[i] = c * pi - s * pr;
                    dr[i] = c * cr + s * ci; di[i] = c * ci - s * cr;
                }
            }
            gz = qwarp_sum_f32(gz) * 0.5f;
            if (lane == 0U) dt[pidx] = gz;
            __ptx_syncthreads();
        }
        for (int q = (int)nq - 1; q >= 0; q--) {
            unsigned int qu = (unsigned int)q;
            unsigned int pidx = pu * n_q_params + qu;
            float ang = th[pidx];
            float h = 0.5f * ang;
            float c = __ptx_cosf(h);
            float s = __ptx_sinf(h);
            unsigned int bit = 1U << qu;
            unsigned int lmask = bit - 1U;
            float gy = 0.0f;
            for (unsigned int pair = lane; pair < halfD; pair += 32U) {
                unsigned int lo = pair & lmask;
                unsigned int hi = (pair - lo) << 1;
                unsigned int i0 = hi | lo;
                unsigned int i1 = i0 | bit;
                float p0r = wr[i0], p1r = wr[i1];
                float p0i = wi[i0], p1i = wi[i1];
                float c0r = dr[i0], c1r = dr[i1];
                float c0i = di[i0], c1i = di[i1];
                gy += (c1r * p0r - c0r * p1r) + (c1i * p0i - c0i * p1i);
                wr[i0] = c * p0r + s * p1r; wr[i1] = c * p1r - s * p0r;
                wi[i0] = c * p0i + s * p1i; wi[i1] = c * p1i - s * p0i;
                dr[i0] = c * c0r + s * c1r; dr[i1] = c * c1r - s * c0r;
                di[i0] = c * c0i + s * c1i; di[i1] = c * c1i - s * c0i;
            }
            gy = qwarp_sum_f32(gy) * 0.5f;
            if (lane == 0U) dt[pidx] = gy;
            __ptx_syncthreads();
        }
    }
}

void qllm_reg_bridge_fwd(
    float *re,
    float *im,
    const float *phi,
    unsigned int BT,
    unsigned int n_reg,
    unsigned int d_r
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT || n_reg <= 1U) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int D = n_reg * d_r;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;
    float *rr = re + base;
    float *ri = im + base;

    for (unsigned int r = 0U; r < n_reg; r++) {
        unsigned int rn = (r + 1U == n_reg) ? 0U : (r + 1U);
        float h = 0.5f * phi[r];
        float c = __ptx_cosf(h);
        float s = __ptx_sinf(h);
        unsigned int off0 = r * d_r;
        unsigned int off1 = rn * d_r;
        for (unsigned int k = lane; k < d_r; k += 32U) {
            unsigned int i0 = off0 + k;
            unsigned int i1 = off1 + k;
            float ar = rr[i0], br = rr[i1];
            float ai = ri[i0], bi = ri[i1];
            rr[i0] = c * ar - s * br;
            rr[i1] = s * ar + c * br;
            ri[i0] = c * ai - s * bi;
            ri[i1] = s * ai + c * bi;
        }
        __ptx_syncthreads();
    }
}

void qllm_reg_bridge_bwd(
    const float *out_re,
    const float *out_im,
    float *work_re,
    float *work_im,
    float *d_re,
    float *d_im,
    const float *phi,
    float *dtheta_tok,
    unsigned int BT,
    unsigned int n_reg,
    unsigned int d_r,
    unsigned int dtheta_stride,
    unsigned int phi_off
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT || n_reg <= 1U) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int D = n_reg * d_r;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)D;
    const float *or = out_re + base;
    const float *oi = out_im + base;
    float *wr = work_re + base;
    float *wi = work_im + base;
    float *dr = d_re + base;
    float *di = d_im + base;
    float *dt = dtheta_tok + (unsigned long long)bt * (unsigned long long)dtheta_stride + (unsigned long long)phi_off;

    for (unsigned int i = lane; i < D; i += 32U) {
        wr[i] = or[i];
        wi[i] = oi[i];
    }
    __ptx_syncthreads();

    for (int ri = (int)n_reg - 1; ri >= 0; ri--) {
        unsigned int r = (unsigned int)ri;
        unsigned int rn = (r + 1U == n_reg) ? 0U : (r + 1U);
        float h = 0.5f * phi[r];
        float c = __ptx_cosf(h);
        float s = __ptx_sinf(h);
        unsigned int off0 = r * d_r;
        unsigned int off1 = rn * d_r;
        float g = 0.0f;
        for (unsigned int k = lane; k < d_r; k += 32U) {
            unsigned int i0 = off0 + k;
            unsigned int i1 = off1 + k;
            float p0r = wr[i0], p1r = wr[i1];
            float p0i = wi[i0], p1i = wi[i1];
            float c0r = dr[i0], c1r = dr[i1];
            float c0i = di[i0], c1i = di[i1];
            g += (c1r * p0r - c0r * p1r) + (c1i * p0i - c0i * p1i);
            wr[i0] = c * p0r + s * p1r; wr[i1] = c * p1r - s * p0r;
            wi[i0] = c * p0i + s * p1i; wi[i1] = c * p1i - s * p0i;
            dr[i0] = c * c0r + s * c1r; dr[i1] = c * c1r - s * c0r;
            di[i0] = c * c0i + s * c1i; di[i1] = c * c1i - s * c0i;
        }
        g = qwarp_sum_f32(g) * 0.5f;
        if (lane == 0U) dt[r] = g;
        __ptx_syncthreads();
    }
}

/* 14. Causal KV-Cache Store & O(1) Born-Attention Single-Step Decode */

void qllm_kv_store_slice(
    const float *k_re,
    const float *k_im,
    const float *v_re,
    const float *v_im,
    float *ck_re,
    float *ck_im,
    float *cv_re,
    float *cv_im,
    unsigned int T,
    unsigned int D,
    unsigned int dst_pos
) {
    unsigned int idx = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (idx >= T * D) return;
    unsigned long long dst = (unsigned long long)dst_pos * (unsigned long long)D + (unsigned long long)idx;
    ck_re[dst] = k_re[idx];
    ck_im[dst] = k_im[idx];
    cv_re[dst] = v_re[idx];
    cv_im[dst] = v_im[idx];
}

void qllm_born_attn_decode_step(
    const float *q_re,
    const float *q_im,
    const float *ck_re,
    const float *ck_im,
    const float *cv_re,
    const float *cv_im,
    const float *beta,
    const float *gamma,
    float *scores_scratch,
    float *out_re,
    float *out_im,
    unsigned int pos,
    unsigned int H,
    unsigned int Dh
) {
    unsigned int h = __ptx_ctaid_x();
    if (h >= H) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned int D = H * Dh;
    unsigned int T_len = pos + 1U;
    float b_val = beta[h];
    float g_val = gamma[h];
    float inv_sqrt_dh = 1.0f / __ptx_sqrtf((float)Dh);

    unsigned long long q_off = (unsigned long long)(h * Dh);
    unsigned long long s_base = (unsigned long long)h * (unsigned long long)T_len;

    for (unsigned int s = 0U; s <= pos; s++) {
        unsigned long long k_off = (unsigned long long)s * (unsigned long long)D + q_off;
        float rp = 0.0f, ip = 0.0f;
        for (unsigned int j = lane; j < Dh; j += 32U) {
            float qr = q_re[q_off + j];
            float qi = q_im[q_off + j];
            float kr = ck_re[k_off + j];
            float ki = ck_im[k_off + j];
            rp += kr * qr + ki * qi;
            ip += kr * qi - ki * qr;
        }
        float R = qwarp_sum_f32(rp);
        float I = qwarp_sum_f32(ip);
        if (lane == 0U) {
            scores_scratch[s_base + s] = (b_val * (R * R + I * I) + g_val * R) * inv_sqrt_dh;
        }
    }
    __ptx_syncthreads();

    float lmax = -1e30f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int s = base + lane;
        if (s <= pos) lmax = __ptx_maxf(lmax, scores_scratch[s_base + s]);
    }
    float maxv = qwarp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int s = base + lane;
        if (s <= pos) {
            float e = qfast_expf(scores_scratch[s_base + s] - maxv);
            scores_scratch[s_base + s] = e;
            lsum += e;
        }
    }
    float sumv = qwarp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    for (unsigned int base = 0U; base <= pos; base += 32U) {
        unsigned int s = base + lane;
        if (s <= pos) scores_scratch[s_base + s] *= inv_sum;
    }
    __ptx_syncthreads();

    for (unsigned int j = lane; j < Dh; j += 32U) {
        float ar = 0.0f, ai = 0.0f;
        for (unsigned int s = 0U; s <= pos; s++) {
            unsigned long long v_off = (unsigned long long)s * (unsigned long long)D + q_off;
            float w = scores_scratch[s_base + s];
            ar = __ptx_fmaf(w, cv_re[v_off + j], ar);
            ai = __ptx_fmaf(w, cv_im[v_off + j], ai);
        }
        out_re[q_off + j] = ar;
        out_im[q_off + j] = ai;
    }
}

/* 15. Global Gradient L2 Norm Reduction & Vector Scaling */

void qllm_sq_sum_blocks(const float *x, float *block_out, unsigned int n) {
    unsigned int cta = __ptx_ctaid_x();
    unsigned int gdim = 256U;
    unsigned int lane = __ptx_tid_x() & 31U;
    float acc = 0.0f;
    for (unsigned int i = cta * 32U + lane; i < n; i += gdim * 32U) {
        float v = x[i];
        acc = __ptx_fmaf(v, v, acc);
    }
    float sum = qwarp_sum_f32(acc);
    if (lane == 0U) block_out[cta] = sum;
}

void qllm_scale_f32(float *x, float scale, unsigned int n) {
    unsigned int i = __ptx_ctaid_x() * __ptx_ntid_x() + __ptx_tid_x();
    if (i < n) x[i] *= scale;
}

void qllm_ce_loss_masked_bwd(
    const float *logits,
    const unsigned int *targets,
    float *dlogits,
    float *losses,
    unsigned int BT,
    unsigned int V,
    float inv_active
) {
    unsigned int bt = __ptx_ctaid_x();
    if (bt >= BT) return;
    unsigned int lane = __ptx_tid_x() & 31U;
    unsigned long long base = (unsigned long long)bt * (unsigned long long)V;
    unsigned int tgt = targets[bt];
    if (tgt >= V) {
        if (lane == 0U) losses[bt] = 0.0f;
        for (unsigned int v = lane; v < V; v += 32U) {
            dlogits[base + v] = 0.0f;
        }
        return;
    }

    float lmax = -1e30f;
    for (unsigned int v = lane; v < V; v += 32U) {
        lmax = __ptx_maxf(lmax, logits[base + v]);
    }
    float maxv = qwarp_max_f32(lmax);

    float lsum = 0.0f;
    for (unsigned int v = lane; v < V; v += 32U) {
        float e = qfast_expf(logits[base + v] - maxv);
        dlogits[base + v] = e;
        lsum += e;
    }
    float sumv = qwarp_sum_f32(lsum);
    __ptx_syncthreads();

    float inv_sum = (sumv > 0.0f) ? (1.0f / sumv) : 0.0f;
    if (lane == 0U) {
        float tlog = logits[base + tgt] - maxv;
        losses[bt] = qfast_logf(sumv) - tlog;
    }
    for (unsigned int v = lane; v < V; v += 32U) {
        float p = dlogits[base + v] * inv_sum;
        float sub = (v == tgt) ? 1.0f : 0.0f;
        dlogits[base + v] = (p - sub) * inv_active;
    }
}


