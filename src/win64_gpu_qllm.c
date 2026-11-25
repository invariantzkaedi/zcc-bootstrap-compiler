/*
 * ZKAEDI ZCC // Sovereign Blackwell Hybrid Quantum-Classical Transformer (Track 13)
 * ====================================================================================
 * Compiled natively to Win64 PE32+ by ZCC with ZERO PyTorch, ZERO CUDA Toolkit SDK,
 * ZERO nvcc, ZERO MSVC, ZERO MinGW, ZERO GCC on Windows.
 * Direct PE32+ imports: C:\Windows\System32\{msvcrt,nvcuda}.dll ONLY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "gpu_qllm_kernels_ptx.h"

/* CUDA Driver API -> C:\Windows\System32\nvcuda.dll */
extern int cuInit(unsigned int Flags);
extern int cuDriverGetVersion(int *driverVersion);
extern int cuDeviceGetCount(int *count);
extern int cuDeviceGet(int *device, int ordinal);
extern int cuDeviceGetName(char *name, int len, int dev);
extern int cuDeviceTotalMem_v2(size_t *bytes, int dev);
extern int cuCtxCreate_v2(void **pctx, unsigned int flags, int dev);
extern int cuCtxDestroy_v2(void *ctx);
extern int cuCtxSynchronize(void);
extern int cuModuleLoadDataEx(void **module, const void *image, unsigned int numOptions, int *options, void **optionValues);
extern int cuModuleUnload(void *hmod);
extern int cuModuleGetFunction(void **hfunc, void *hmod, const char *name);
extern int cuMemAlloc_v2(uint64_t *dptr, size_t bytesize);
extern int cuMemFree_v2(uint64_t dptr);
extern int cuMemsetD8_v2(uint64_t dstDevice, unsigned char uc, size_t N);
extern int cuMemcpyHtoD_v2(uint64_t dstDevice, const void *srcHost, size_t ByteCount);
extern int cuMemcpyDtoH_v2(void *dstHost, uint64_t srcDevice, size_t ByteCount);
extern int cuLaunchKernel(
    void *f,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    void *hStream,
    void **kernelParams,
    void **extra
);
extern int cuEventCreate(void **phEvent, unsigned int Flags);
extern int cuEventRecord(void *hEvent, void *hStream);
extern int cuEventSynchronize(void *hEvent);
extern int cuEventElapsedTime(float *pMilliseconds, void *hStart, void *hEnd);
extern int cuEventDestroy_v2(void *hEvent);
extern int cuGetErrorName(int error, const char **pStr);
extern int cuGetErrorString(int error, const char **pStr);

static void check_cu(int rc, const char *where) {
    if (rc != 0) {
        const char *ename = "UNKNOWN";
        const char *edesc = "";
        cuGetErrorName(rc, &ename);
        cuGetErrorString(rc, &edesc);
        fprintf(stderr, "CUDA_ERROR in %s: rc=%d (%s: %s)\n",
                where, rc, ename ? ename : "?", edesc ? edesc : "");
        exit(1);
    }
}

#define QLLM_MAX_LAYERS 8U
#define QLLM_VOCAB      128U
#define QLLM_MAX_VOCAB  2048U
#define QLLM_MAX_MERGES (QLLM_MAX_VOCAB - 256U)
#define QLLM_MAGIC0     0x4E4E515AU /* "ZQNN" */
#define QLLM_MAGIC1     0x00000001U

static uint64_t fnv1a_update64(uint64_t h, const void *data, size_t nbytes) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0U; i < nbytes; i++) {
        h ^= (uint64_t)p[i];
        h *= 0x00000100000001B3ULL;
    }
    return h;
}

static uint64_t fnv1a_bytes64(const void *data, size_t nbytes) {
    return fnv1a_update64(0xCBF29CE484222325ULL, data, nbytes);
}

typedef struct {
    uint64_t s;
} Qrng;

static uint64_t qrng_next_u64(Qrng *r) {
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static double qrng_u01(Qrng *r) {
    uint64_t v = qrng_next_u64(r) >> 11;
    double res = ((double)v + 0.5) * (1.0 / 9007199254740992.0);
    if (res <= 1e-12) res = 1e-12;
    if (res >= 1.0 - 1e-12) res = 1.0 - 1e-12;
    return res;
}

static float qrng_randn(Qrng *r, float stdv) {
    double u1 = qrng_u01(r);
    double u2 = qrng_u01(r);
    double mag = sqrt(-2.0 * log(u1));
    double z = mag * cos(6.28318530717958647692 * u2);
    return (float)(z * (double)stdv);
}

typedef struct {
    void *embed_fwd;
    void *embed_bwd;
    void *cunit_fwd;
    void *cunit_bwd;
    void *pqc_fwd;
    void *pqc_bwd;
    void *qrope_fwd;
    void *qrope_bwd;
    void *attn_fwd;
    void *attn_bwd_q;
    void *attn_bwd_kv;
    void *kerr_fwd;
    void *kerr_bwd;
    void *pack_quad;
    void *unpack_quad;
    void *linear_fwd;
    void *linear_bwd_x;
    void *linear_bwd_w;
    void *linear_bwd_b;
    void *silu_fwd;
    void *silu_bwd;
    void *add_f32;
    void *add_inplace;
    void *zero_f32;
    void *reduce_rows;
    void *reduce_head;
    void *ce_loss_bwd;
    void *adamw_step;
    void *f32_to_f16;
    void *transpose_f32_to_f16;
    void *gemm_tc_f16;
    void *add_bias_rows;
    void *pqc_mr_fwd;
    void *pqc_mr_bwd;
    void *reg_bridge_fwd;
    void *reg_bridge_bwd;
    void *kv_store_slice;
    void *attn_decode_step;
    void *sq_sum_blocks;
    void *scale_f32;
    void *ce_loss_masked_bwd;
} QKernels;

static void load_qkernels(void *mod, QKernels *k) {
    check_cu(cuModuleGetFunction(&k->embed_fwd, mod, "qllm_embed_qrope_fwd"), "qllm_embed_qrope_fwd");
    check_cu(cuModuleGetFunction(&k->embed_bwd, mod, "qllm_embed_qrope_bwd"), "qllm_embed_qrope_bwd");
    check_cu(cuModuleGetFunction(&k->cunit_fwd, mod, "qllm_cunit_norm_fwd"), "qllm_cunit_norm_fwd");
    check_cu(cuModuleGetFunction(&k->cunit_bwd, mod, "qllm_cunit_norm_bwd"), "qllm_cunit_norm_bwd");
    check_cu(cuModuleGetFunction(&k->pqc_fwd, mod, "qllm_pqc_fwd"), "qllm_pqc_fwd");
    check_cu(cuModuleGetFunction(&k->pqc_bwd, mod, "qllm_pqc_bwd"), "qllm_pqc_bwd");
    check_cu(cuModuleGetFunction(&k->qrope_fwd, mod, "qllm_qrope_heads_fwd"), "qllm_qrope_heads_fwd");
    check_cu(cuModuleGetFunction(&k->qrope_bwd, mod, "qllm_qrope_heads_bwd"), "qllm_qrope_heads_bwd");
    check_cu(cuModuleGetFunction(&k->attn_fwd, mod, "qllm_born_attn_fwd"), "qllm_born_attn_fwd");
    check_cu(cuModuleGetFunction(&k->attn_bwd_q, mod, "qllm_born_attn_bwd_q"), "qllm_born_attn_bwd_q");
    check_cu(cuModuleGetFunction(&k->attn_bwd_kv, mod, "qllm_born_attn_bwd_kv"), "qllm_born_attn_bwd_kv");
    check_cu(cuModuleGetFunction(&k->kerr_fwd, mod, "qllm_kerr_fwd"), "qllm_kerr_fwd");
    check_cu(cuModuleGetFunction(&k->kerr_bwd, mod, "qllm_kerr_bwd"), "qllm_kerr_bwd");
    check_cu(cuModuleGetFunction(&k->pack_quad, mod, "qllm_pack_quad"), "qllm_pack_quad");
    check_cu(cuModuleGetFunction(&k->unpack_quad, mod, "qllm_unpack_quad"), "qllm_unpack_quad");
    check_cu(cuModuleGetFunction(&k->linear_fwd, mod, "qllm_linear_fwd"), "qllm_linear_fwd");
    check_cu(cuModuleGetFunction(&k->linear_bwd_x, mod, "qllm_linear_bwd_x"), "qllm_linear_bwd_x");
    check_cu(cuModuleGetFunction(&k->linear_bwd_w, mod, "qllm_linear_bwd_w"), "qllm_linear_bwd_w");
    check_cu(cuModuleGetFunction(&k->linear_bwd_b, mod, "qllm_linear_bwd_b"), "qllm_linear_bwd_b");
    check_cu(cuModuleGetFunction(&k->silu_fwd, mod, "qllm_silu_fwd"), "qllm_silu_fwd");
    check_cu(cuModuleGetFunction(&k->silu_bwd, mod, "qllm_silu_bwd"), "qllm_silu_bwd");
    check_cu(cuModuleGetFunction(&k->add_f32, mod, "qllm_add_f32"), "qllm_add_f32");
    check_cu(cuModuleGetFunction(&k->add_inplace, mod, "qllm_add_inplace_f32"), "qllm_add_inplace_f32");
    check_cu(cuModuleGetFunction(&k->zero_f32, mod, "qllm_zero_f32"), "qllm_zero_f32");
    check_cu(cuModuleGetFunction(&k->reduce_rows, mod, "qllm_reduce_rows_add"), "qllm_reduce_rows_add");
    check_cu(cuModuleGetFunction(&k->reduce_head, mod, "qllm_reduce_head_add"), "qllm_reduce_head_add");
    check_cu(cuModuleGetFunction(&k->ce_loss_bwd, mod, "qllm_ce_loss_bwd"), "qllm_ce_loss_bwd");
    check_cu(cuModuleGetFunction(&k->adamw_step, mod, "qllm_adamw_step"), "qllm_adamw_step");
    check_cu(cuModuleGetFunction(&k->f32_to_f16, mod, "qllm_f32_to_f16"), "qllm_f32_to_f16");
    check_cu(cuModuleGetFunction(&k->transpose_f32_to_f16, mod, "qllm_transpose_f32_to_f16"), "qllm_transpose_f32_to_f16");
    check_cu(cuModuleGetFunction(&k->gemm_tc_f16, mod, "qllm_gemm_tc_f16"), "qllm_gemm_tc_f16");
    check_cu(cuModuleGetFunction(&k->add_bias_rows, mod, "qllm_add_bias_rows"), "qllm_add_bias_rows");
    check_cu(cuModuleGetFunction(&k->pqc_mr_fwd, mod, "qllm_pqc_mr_fwd"), "qllm_pqc_mr_fwd");
    check_cu(cuModuleGetFunction(&k->pqc_mr_bwd, mod, "qllm_pqc_mr_bwd"), "qllm_pqc_mr_bwd");
    check_cu(cuModuleGetFunction(&k->reg_bridge_fwd, mod, "qllm_reg_bridge_fwd"), "qllm_reg_bridge_fwd");
    check_cu(cuModuleGetFunction(&k->reg_bridge_bwd, mod, "qllm_reg_bridge_bwd"), "qllm_reg_bridge_bwd");
    check_cu(cuModuleGetFunction(&k->kv_store_slice, mod, "qllm_kv_store_slice"), "qllm_kv_store_slice");
    check_cu(cuModuleGetFunction(&k->attn_decode_step, mod, "qllm_born_attn_decode_step"), "qllm_born_attn_decode_step");
    check_cu(cuModuleGetFunction(&k->sq_sum_blocks, mod, "qllm_sq_sum_blocks"), "qllm_sq_sum_blocks");
    check_cu(cuModuleGetFunction(&k->scale_f32, mod, "qllm_scale_f32"), "qllm_scale_f32");
    check_cu(cuModuleGetFunction(&k->ce_loss_masked_bwd, mod, "qllm_ce_loss_masked_bwd"), "qllm_ce_loss_masked_bwd");
}

typedef struct {
    unsigned int off_theta_q;
    unsigned int off_theta_k;
    unsigned int off_theta_v;
    unsigned int off_theta_f1;
    unsigned int off_theta_f2;
    unsigned int off_beta;
    unsigned int off_gamma;
    unsigned int off_kerr_k;
    unsigned int off_kerr_b;
    unsigned int off_w1;
    unsigned int off_b1;
    unsigned int off_w2;
    unsigned int off_b2;
} QLayerOff;

typedef struct {
    unsigned int version;
    unsigned int vocab_size;
    unsigned int n_merges;
    unsigned int n_registers;
    unsigned int reg_dim;
    unsigned int n_qubits;
    unsigned int dim;
    unsigned int n_heads;
    unsigned int head_dim;
    unsigned int n_layers;
    unsigned int pqc_depth;
    unsigned int h_ff;
    unsigned int n_pqc_params;
    unsigned int total_params;
    uint64_t seed;

    uint16_t *bpe_left;
    uint16_t *bpe_right;
    uint8_t *tok_pool;
    uint32_t *tok_off;
    uint16_t *tok_len;

    unsigned int off_E_re;
    unsigned int off_E_im;
    QLayerOff layers[QLLM_MAX_LAYERS];
    unsigned int off_H_out;
    unsigned int off_b_out;

    float *h_weights;
    uint64_t d_weights;
    uint64_t d_grads;
    uint64_t d_adam_m;
    uint64_t d_adam_v;
} QModel;

static void compute_model_layout(QModel *m) {
    if (m->version == 0U) m->version = 1U;
    if (m->vocab_size == 0U) m->vocab_size = QLLM_VOCAB;
    if (m->n_registers == 0U) m->n_registers = 1U;
    m->reg_dim = 1U << m->n_qubits;
    m->dim = m->n_registers * m->reg_dim;
    m->head_dim = m->dim / m->n_heads;
    m->h_ff = 2U * m->dim;
    m->n_pqc_params = m->n_registers * (3U * m->pqc_depth * m->n_qubits) + ((m->n_registers > 1U) ? m->n_registers : 0U);

    unsigned int cur = 0U;
    m->off_E_re = cur; cur += m->vocab_size * m->dim;
    m->off_E_im = cur; cur += m->vocab_size * m->dim;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        QLayerOff *lo = &m->layers[l];
        lo->off_theta_q  = cur; cur += m->n_pqc_params;
        lo->off_theta_k  = cur; cur += m->n_pqc_params;
        lo->off_theta_v  = cur; cur += m->n_pqc_params;
        lo->off_theta_f1 = cur; cur += m->n_pqc_params;
        lo->off_theta_f2 = cur; cur += m->n_pqc_params;
        lo->off_beta     = cur; cur += m->n_heads;
        lo->off_gamma    = cur; cur += m->n_heads;
        lo->off_kerr_k   = cur; cur += m->dim;
        lo->off_kerr_b   = cur; cur += m->dim;
        lo->off_w1       = cur; cur += m->h_ff * (2U * m->dim);
        lo->off_b1       = cur; cur += m->h_ff;
        lo->off_w2       = cur; cur += (2U * m->dim) * m->h_ff;
        lo->off_b2       = cur; cur += (2U * m->dim);
    }
    m->off_H_out = cur; cur += m->vocab_size * (2U * m->dim);
    m->off_b_out = cur; cur += m->vocab_size;
    m->total_params = cur;
}

static void init_model_weights(QModel *m, uint64_t seed) {
    m->seed = (seed == 0ULL) ? 1337ULL : seed;
    m->h_weights = (float *)calloc((size_t)m->total_params, sizeof(float));
    if (!m->h_weights) { fprintf(stderr, "FATAL: OOM h_weights\n"); exit(1); }

    Qrng rng;
    rng.s = m->seed ^ 0x9E3779B97F4A7C15ULL;
    if (rng.s == 0ULL) rng.s = 1ULL;

    float std_e = (float)(1.0 / sqrt((double)(2U * m->dim)));
    for (unsigned int i = 0U; i < m->vocab_size * m->dim; i++) {
        m->h_weights[m->off_E_re + i] = qrng_randn(&rng, std_e);
        m->h_weights[m->off_E_im + i] = qrng_randn(&rng, std_e);
    }
    float std_w1 = (float)(1.0 / sqrt((double)(2U * m->dim)));
    float std_w2 = (float)(0.5 / sqrt((double)m->h_ff));
    float init_beta = 2.0f * (float)m->n_heads * (float)sqrt((double)m->head_dim);
    float init_gamma = 2.0f * (float)m->n_heads;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        QLayerOff *lo = &m->layers[l];
        unsigned int poff[5] = { lo->off_theta_q, lo->off_theta_k, lo->off_theta_v, lo->off_theta_f1, lo->off_theta_f2 };
        for (int s = 0; s < 5; s++) {
            for (unsigned int i = 0U; i < m->n_pqc_params; i++) {
                float u = (float)(qrng_u01(&rng) * 2.0 - 1.0);
                m->h_weights[poff[s] + i] = u * 0.35f;
            }
        }
        for (unsigned int h = 0U; h < m->n_heads; h++) {
            m->h_weights[lo->off_beta + h] = init_beta;
            m->h_weights[lo->off_gamma + h] = init_gamma;
        }
        for (unsigned int k = 0U; k < m->dim; k++) {
            m->h_weights[lo->off_kerr_k + k] = 0.30f;
            m->h_weights[lo->off_kerr_b + k] = 0.0f;
        }
        for (unsigned int i = 0U; i < m->h_ff * (2U * m->dim); i++) {
            m->h_weights[lo->off_w1 + i] = qrng_randn(&rng, std_w1);
        }
        for (unsigned int i = 0U; i < (2U * m->dim) * m->h_ff; i++) {
            m->h_weights[lo->off_w2 + i] = qrng_randn(&rng, std_w2);
        }
    }
    float std_h = (float)(1.0 / sqrt((double)(2U * m->dim)));
    for (unsigned int i = 0U; i < m->vocab_size * (2U * m->dim); i++) {
        m->h_weights[m->off_H_out + i] = qrng_randn(&rng, std_h);
    }
}

static void alloc_model_gpu(QModel *m, int with_optim) {
    size_t sz = (size_t)m->total_params * sizeof(float);
    check_cu(cuMemAlloc_v2(&m->d_weights, sz), "d_weights");
    check_cu(cuMemcpyHtoD_v2(m->d_weights, m->h_weights, sz), "HtoD(weights)");
    if (with_optim) {
        check_cu(cuMemAlloc_v2(&m->d_grads, sz), "d_grads");
        check_cu(cuMemAlloc_v2(&m->d_adam_m, sz), "d_adam_m");
        check_cu(cuMemAlloc_v2(&m->d_adam_v, sz), "d_adam_v");
        check_cu(cuMemsetD8_v2(m->d_grads, 0, sz), "zero(grads)");
        check_cu(cuMemsetD8_v2(m->d_adam_m, 0, sz), "zero(m)");
        check_cu(cuMemsetD8_v2(m->d_adam_v, 0, sz), "zero(v)");
    }
}

static void bpe_free_vocab(QModel *m) {
    if (m->bpe_left) { free(m->bpe_left); m->bpe_left = NULL; }
    if (m->bpe_right) { free(m->bpe_right); m->bpe_right = NULL; }
    if (m->tok_pool) { free(m->tok_pool); m->tok_pool = NULL; }
    if (m->tok_off) { free(m->tok_off); m->tok_off = NULL; }
    if (m->tok_len) { free(m->tok_len); m->tok_len = NULL; }
}

static int bpe_build_vocab_strings(QModel *m) {
    unsigned int V = m->vocab_size;
    if (m->tok_pool) { free(m->tok_pool); m->tok_pool = NULL; }
    if (m->tok_off) { free(m->tok_off); m->tok_off = NULL; }
    if (m->tok_len) { free(m->tok_len); m->tok_len = NULL; }

    m->tok_off = (uint32_t *)malloc((size_t)V * sizeof(uint32_t));
    m->tok_len = (uint16_t *)malloc((size_t)V * sizeof(uint16_t));
    if (!m->tok_off || !m->tok_len) return -1;

    size_t total_bytes = 0U;
    unsigned int base_end = (V < 256U) ? V : 256U;
    for (unsigned int id = 0U; id < base_end; id++) {
        m->tok_off[id] = (uint32_t)total_bytes;
        m->tok_len[id] = 1U;
        total_bytes += 1U;
    }
    for (unsigned int mi = 0U; mi < m->n_merges; mi++) {
        unsigned int id = 256U + mi;
        if (id >= V) break;
        unsigned int l = (unsigned int)m->bpe_left[mi];
        unsigned int r = (unsigned int)m->bpe_right[mi];
        if (l >= id || r >= id) return -1;
        unsigned int comb = (unsigned int)m->tok_len[l] + (unsigned int)m->tok_len[r];
        if (comb > 256U) comb = 256U;
        m->tok_off[id] = (uint32_t)total_bytes;
        m->tok_len[id] = (uint16_t)comb;
        total_bytes += (size_t)comb;
    }

    m->tok_pool = (uint8_t *)malloc(total_bytes + 1U);
    if (!m->tok_pool) return -1;
    for (unsigned int id = 0U; id < base_end; id++) {
        m->tok_pool[m->tok_off[id]] = (uint8_t)id;
    }
    for (unsigned int mi = 0U; mi < m->n_merges; mi++) {
        unsigned int id = 256U + mi;
        if (id >= V) break;
        unsigned int l = (unsigned int)m->bpe_left[mi];
        unsigned int r = (unsigned int)m->bpe_right[mi];
        unsigned int ll = (unsigned int)m->tok_len[l];
        unsigned int rl = (unsigned int)m->tok_len[r];
        if (ll > 256U) ll = 256U;
        if (ll + rl > 256U) rl = 256U - ll;
        uint8_t *dst = m->tok_pool + m->tok_off[id];
        memcpy(dst, m->tok_pool + m->tok_off[l], ll);
        if (rl > 0U) memcpy(dst + ll, m->tok_pool + m->tok_off[r], rl);
    }
    return 0;
}

static void bpe_train_merges(QModel *m, const uint8_t *raw, size_t raw_len, unsigned int target_vocab) {
    unsigned int target_merges = (target_vocab > 256U) ? (target_vocab - 256U) : 0U;
    if (target_merges > QLLM_MAX_MERGES) target_merges = QLLM_MAX_MERGES;

    size_t slen = (raw_len > 131072U) ? 131072U : raw_len;
    uint16_t *seq = (uint16_t *)malloc((slen + 1U) * sizeof(uint16_t));
    m->bpe_left = (uint16_t *)calloc(target_merges ? target_merges : 1U, sizeof(uint16_t));
    m->bpe_right = (uint16_t *)calloc(target_merges ? target_merges : 1U, sizeof(uint16_t));

    unsigned int ht_cap = 262144U;
    unsigned int ht_mask = ht_cap - 1U;
    uint32_t *ht_keys = (uint32_t *)malloc((size_t)ht_cap * sizeof(uint32_t));
    uint32_t *ht_cnts = (uint32_t *)malloc((size_t)ht_cap * sizeof(uint32_t));
    uint32_t *ht_stamps = (uint32_t *)calloc((size_t)ht_cap, sizeof(uint32_t));
    uint32_t *active_slots = (uint32_t *)malloc((slen + 1U) * sizeof(uint32_t));
    if (!seq || !m->bpe_left || !m->bpe_right || !ht_keys || !ht_cnts || !ht_stamps || !active_slots) exit(1);

    for (size_t i = 0U; i < slen; i++) seq[i] = (uint16_t)raw[i];

    unsigned int learned = 0U;
    uint32_t stamp = 1U;
    for (unsigned int mi = 0U; mi < target_merges; mi++) {
        if (slen < 2U) break;
        unsigned int n_act = 0U;
        for (size_t i = 0U; i + 1U < slen; i++) {
            uint32_t key = ((uint32_t)seq[i] << 16) | (uint32_t)seq[i + 1U];
            uint32_t slot = ((key * 2654435761U) ^ (key >> 16)) & ht_mask;
            while (ht_stamps[slot] == stamp && ht_keys[slot] != key) {
                slot = (slot + 1U) & ht_mask;
            }
            if (ht_stamps[slot] != stamp) {
                ht_stamps[slot] = stamp;
                ht_keys[slot] = key;
                ht_cnts[slot] = 1U;
                active_slots[n_act++] = slot;
            } else {
                ht_cnts[slot] += 1U;
            }
        }
        uint32_t best_cnt = 0U;
        uint32_t best_key = 0xFFFFFFFFU;
        for (unsigned int ai = 0U; ai < n_act; ai++) {
            uint32_t sl = active_slots[ai];
            uint32_t c = ht_cnts[sl];
            uint32_t k = ht_keys[sl];
            if (c > best_cnt || (c == best_cnt && k < best_key)) {
                best_cnt = c;
                best_key = k;
            }
        }
        stamp++;
        if (best_cnt < 2U) break;

        uint16_t ltok = (uint16_t)(best_key >> 16);
        uint16_t rtok = (uint16_t)(best_key & 0xFFFFU);
        uint16_t ntok = (uint16_t)(256U + mi);
        m->bpe_left[mi] = ltok;
        m->bpe_right[mi] = rtok;
        learned++;

        size_t w = 0U;
        size_t i = 0U;
        while (i < slen) {
            if (i + 1U < slen && seq[i] == ltok && seq[i + 1U] == rtok) {
                seq[w++] = ntok;
                i += 2U;
            } else {
                seq[w++] = seq[i++];
            }
        }
        slen = w;
    }

    free(seq);
    free(ht_keys);
    free(ht_cnts);
    free(ht_stamps);
    free(active_slots);

    m->version = (m->n_registers > 1U) ? 3U : 2U;
    m->n_merges = learned;
    m->vocab_size = 256U + learned;
    if (bpe_build_vocab_strings(m) != 0) { fprintf(stderr, "FATAL: bpe vocab build\n"); exit(1); }
}

static size_t bpe_encode(const QModel *m, const uint8_t *raw, size_t raw_len, uint32_t *out_ids, size_t max_out) {
    if (raw_len == 0U || max_out == 0U) return 0U;
    if (m->vocab_size < 256U) {
        size_t n = (raw_len < max_out) ? raw_len : max_out;
        for (size_t i = 0U; i < n; i++) {
            uint8_t ch = raw[i];
            out_ids[i] = (ch < 128U) ? (uint32_t)ch : (uint32_t)' ';
        }
        return n;
    }
    uint16_t *buf = (uint16_t *)malloc(raw_len * sizeof(uint16_t));
    if (!buf) exit(1);
    for (size_t i = 0U; i < raw_len; i++) buf[i] = (uint16_t)raw[i];
    size_t n = raw_len;
    for (unsigned int mi = 0U; mi < m->n_merges; mi++) {
        uint16_t ltok = m->bpe_left[mi];
        uint16_t rtok = m->bpe_right[mi];
        uint16_t ntok = (uint16_t)(256U + mi);
        size_t w = 0U;
        size_t i = 0U;
        while (i < n) {
            if (i + 1U < n && buf[i] == ltok && buf[i + 1U] == rtok) {
                buf[w++] = ntok;
                i += 2U;
            } else {
                buf[w++] = buf[i++];
            }
        }
        n = w;
    }
    size_t keep = (n < max_out) ? n : max_out;
    for (size_t i = 0U; i < keep; i++) out_ids[i] = (uint32_t)buf[i];
    free(buf);
    return keep;
}

static void free_model(QModel *m) {
    if (m->d_weights) { cuMemFree_v2(m->d_weights); m->d_weights = 0ULL; }
    if (m->d_grads) { cuMemFree_v2(m->d_grads); m->d_grads = 0ULL; }
    if (m->d_adam_m) { cuMemFree_v2(m->d_adam_m); m->d_adam_m = 0ULL; }
    if (m->d_adam_v) { cuMemFree_v2(m->d_adam_v); m->d_adam_v = 0ULL; }
    if (m->h_weights) { free(m->h_weights); m->h_weights = NULL; }
    bpe_free_vocab(m);
}

static uint64_t dp_off(uint64_t base, unsigned int float_off) {
    return base + (uint64_t)float_off * 4ULL;
}

typedef struct {
    uint64_t q_pqc_re, q_pqc_im;
    uint64_t k_pqc_re, k_pqc_im;
    uint64_t v_re, v_im;
    uint64_t q_rope_re, q_rope_im;
    uint64_t k_rope_re, k_rope_im;
    uint64_t att_re, att_im, att_w, att_dS;
    uint64_t attn_out_re, attn_out_im;
    uint64_t psi_mid_raw_re, psi_mid_raw_im;
    uint64_t psi_mid_re, psi_mid_im, mid_inv_norm;
    uint64_t u_re, u_im;
    uint64_t v_kerr_re, v_kerr_im;
    uint64_t ffn_in_quad, h1_pre, h1_act, delta_quad;
    uint64_t delta_re, delta_im;
    uint64_t pre_f2_re, pre_f2_im;
    uint64_t post_f2_re, post_f2_im;
    uint64_t psi_out_re, psi_out_im, out_inv_norm;
    uint64_t kv_k_re, kv_k_im, kv_v_re, kv_v_im;
} QLayerAct;

typedef struct {
    unsigned int max_bt;
    unsigned int max_att;
    int with_bwd;

    uint64_t d_tokens, d_targets;
    uint64_t emb_re, emb_im;
    uint64_t psi0_re, psi0_im, inv_norm0;
    QLayerAct layers[QLLM_MAX_LAYERS];
    uint64_t psi_L_quad, logits, losses;
    uint64_t kv_scores;

    /* Tensor Core FP16 scratch & global grad norm reduction */
    uint64_t d_tc_a, d_tc_b;
    uint64_t d_gnorm_blk;

    /* Backward & temporary buffers */
    uint64_t d_logits, d_psi_quad;
    uint64_t d_cur_re, d_cur_im;
    uint64_t d_nxt_re, d_nxt_im;
    uint64_t d_tmp1_re, d_tmp1_im;
    uint64_t d_tmp2_re, d_tmp2_im;
    uint64_t d_tmp3_re, d_tmp3_im;
    uint64_t d_q1_re, d_q1_im;
    uint64_t d_k1_re, d_k1_im;
    uint64_t d_v1_re, d_v1_im;
    uint64_t d_pqc_work_re, d_pqc_work_im;
    uint64_t d_bridge_re, d_bridge_im;
    uint64_t d_ffn_quad, d_h1;
    uint64_t d_pqc_tok, d_kerr_tok, d_kb_tok, d_bg1, d_bg2;
} QActArena;

static uint64_t qmalloc_f32(size_t n_floats, const char *tag) {
    uint64_t d = 0ULL;
    check_cu(cuMemAlloc_v2(&d, n_floats * 4U), tag);
    return d;
}

static void qfree_if(uint64_t *p) {
    if (*p) { cuMemFree_v2(*p); *p = 0ULL; }
}

static void alloc_act_arena(const QModel *m, QActArena *a, unsigned int max_bt, unsigned int max_att, int with_bwd) {
    memset(a, 0, sizeof(*a));
    a->max_bt = max_bt;
    a->max_att = max_att;
    a->with_bwd = with_bwd;

    size_t bt_d = (size_t)max_bt * (size_t)m->dim;
    size_t bt_2d = bt_d * 2U;
    size_t bt_h = (size_t)max_bt * (size_t)m->h_ff;
    size_t bt_v = (size_t)max_bt * (size_t)m->vocab_size;

    check_cu(cuMemAlloc_v2(&a->d_tokens, (size_t)max_bt * 4U), "d_tokens");
    if (with_bwd) check_cu(cuMemAlloc_v2(&a->d_targets, (size_t)max_bt * 4U), "d_targets");

    a->emb_re = qmalloc_f32(bt_d, "emb_re");
    a->emb_im = qmalloc_f32(bt_d, "emb_im");
    a->psi0_re = qmalloc_f32(bt_d, "psi0_re");
    a->psi0_im = qmalloc_f32(bt_d, "psi0_im");
    a->inv_norm0 = qmalloc_f32((size_t)max_bt, "inv0");

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        QLayerAct *la = &a->layers[l];
        la->q_pqc_re = qmalloc_f32(bt_d, "q_re");
        la->q_pqc_im = qmalloc_f32(bt_d, "q_im");
        la->k_pqc_re = qmalloc_f32(bt_d, "k_re");
        la->k_pqc_im = qmalloc_f32(bt_d, "k_im");
        la->v_re = qmalloc_f32(bt_d, "v_re");
        la->v_im = qmalloc_f32(bt_d, "v_im");
        la->q_rope_re = qmalloc_f32(bt_d, "qr_re");
        la->q_rope_im = qmalloc_f32(bt_d, "qr_im");
        la->k_rope_re = qmalloc_f32(bt_d, "kr_re");
        la->k_rope_im = qmalloc_f32(bt_d, "kr_im");
        la->att_re = qmalloc_f32((size_t)max_att, "att_re");
        la->att_im = qmalloc_f32((size_t)max_att, "att_im");
        la->att_w = qmalloc_f32((size_t)max_att, "att_w");
        if (with_bwd) la->att_dS = qmalloc_f32((size_t)max_att, "att_dS");
        la->attn_out_re = qmalloc_f32(bt_d, "ao_re");
        la->attn_out_im = qmalloc_f32(bt_d, "ao_im");
        la->psi_mid_raw_re = qmalloc_f32(bt_d, "pmr_re");
        la->psi_mid_raw_im = qmalloc_f32(bt_d, "pmr_im");
        la->psi_mid_re = qmalloc_f32(bt_d, "pm_re");
        la->psi_mid_im = qmalloc_f32(bt_d, "pm_im");
        la->mid_inv_norm = qmalloc_f32((size_t)max_bt, "mid_inv");
        la->u_re = qmalloc_f32(bt_d, "u_re");
        la->u_im = qmalloc_f32(bt_d, "u_im");
        la->v_kerr_re = qmalloc_f32(bt_d, "vk_re");
        la->v_kerr_im = qmalloc_f32(bt_d, "vk_im");
        la->ffn_in_quad = qmalloc_f32(bt_2d, "ffn_in");
        la->h1_pre = qmalloc_f32(bt_h, "h1_pre");
        la->h1_act = qmalloc_f32(bt_h, "h1_act");
        la->delta_quad = qmalloc_f32(bt_2d, "d_quad");
        la->delta_re = qmalloc_f32(bt_d, "d_re");
        la->delta_im = qmalloc_f32(bt_d, "d_im");
        la->pre_f2_re = qmalloc_f32(bt_d, "pf2_re");
        la->pre_f2_im = qmalloc_f32(bt_d, "pf2_im");
        la->post_f2_re = qmalloc_f32(bt_d, "post2_re");
        la->post_f2_im = qmalloc_f32(bt_d, "post2_im");
        la->psi_out_re = qmalloc_f32(bt_d, "po_re");
        la->psi_out_im = qmalloc_f32(bt_d, "po_im");
        la->out_inv_norm = qmalloc_f32((size_t)max_bt, "out_inv");
        if (!with_bwd) {
            la->kv_k_re = qmalloc_f32(bt_d, "kv_kr");
            la->kv_k_im = qmalloc_f32(bt_d, "kv_ki");
            la->kv_v_re = qmalloc_f32(bt_d, "kv_vr");
            la->kv_v_im = qmalloc_f32(bt_d, "kv_vi");
        }
    }

    a->psi_L_quad = qmalloc_f32(bt_2d, "psi_L_quad");
    a->logits = qmalloc_f32(bt_v, "logits");
    if (!with_bwd) {
        a->kv_scores = qmalloc_f32((size_t)m->n_heads * (size_t)max_bt, "kv_scores");
    }

    if (with_bwd) {
        size_t max_tc = bt_v;
        if (bt_h > max_tc) max_tc = bt_h;
        if (bt_2d > max_tc) max_tc = bt_2d;
        size_t w_tc = (size_t)m->vocab_size * (size_t)(2U * m->dim);
        if ((size_t)m->h_ff * (size_t)(2U * m->dim) > w_tc) w_tc = (size_t)m->h_ff * (size_t)(2U * m->dim);
        size_t tc_a_elems = (max_tc > w_tc) ? max_tc : w_tc;
        size_t tc_b_elems = (w_tc > max_tc) ? w_tc : max_tc;
        check_cu(cuMemAlloc_v2(&a->d_tc_a, tc_a_elems * 2U), "d_tc_a");
        check_cu(cuMemAlloc_v2(&a->d_tc_b, tc_b_elems * 2U), "d_tc_b");
        a->d_gnorm_blk = qmalloc_f32(256U, "d_gnorm_blk");

        a->losses = qmalloc_f32((size_t)max_bt, "losses");
        a->d_logits = qmalloc_f32(bt_v, "d_logits");
        a->d_psi_quad = qmalloc_f32(bt_2d, "d_psi_quad");
        a->d_cur_re = qmalloc_f32(bt_d, "dc_re");
        a->d_cur_im = qmalloc_f32(bt_d, "dc_im");
        a->d_nxt_re = qmalloc_f32(bt_d, "dn_re");
        a->d_nxt_im = qmalloc_f32(bt_d, "dn_im");
        a->d_tmp1_re = qmalloc_f32(bt_d, "dt1_re");
        a->d_tmp1_im = qmalloc_f32(bt_d, "dt1_im");
        a->d_tmp2_re = qmalloc_f32(bt_d, "dt2_re");
        a->d_tmp2_im = qmalloc_f32(bt_d, "dt2_im");
        a->d_tmp3_re = qmalloc_f32(bt_d, "dt3_re");
        a->d_tmp3_im = qmalloc_f32(bt_d, "dt3_im");
        a->d_q1_re = qmalloc_f32(bt_d, "dq1_re");
        a->d_q1_im = qmalloc_f32(bt_d, "dq1_im");
        a->d_k1_re = qmalloc_f32(bt_d, "dk1_re");
        a->d_k1_im = qmalloc_f32(bt_d, "dk1_im");
        a->d_v1_re = qmalloc_f32(bt_d, "dv1_re");
        a->d_v1_im = qmalloc_f32(bt_d, "dv1_im");
        a->d_pqc_work_re = qmalloc_f32(bt_d, "pw_re");
        a->d_pqc_work_im = qmalloc_f32(bt_d, "pw_im");
        a->d_bridge_re = qmalloc_f32(bt_d, "br_re");
        a->d_bridge_im = qmalloc_f32(bt_d, "br_im");
        a->d_ffn_quad = qmalloc_f32(bt_2d, "d_ffn_quad");
        a->d_h1 = qmalloc_f32(bt_h, "d_h1");
        a->d_pqc_tok = qmalloc_f32((size_t)max_bt * (size_t)m->n_pqc_params, "d_pqc_tok");
        a->d_kerr_tok = qmalloc_f32(bt_d, "d_kerr_tok");
        a->d_kb_tok = qmalloc_f32(bt_d, "d_kb_tok");
        a->d_bg1 = qmalloc_f32((size_t)max_bt * (size_t)m->n_heads, "d_bg1");
        a->d_bg2 = qmalloc_f32((size_t)max_bt * (size_t)m->n_heads, "d_bg2");
    }
}

static void free_act_arena(const QModel *m, QActArena *a) {
    qfree_if(&a->d_tokens);
    qfree_if(&a->d_targets);
    qfree_if(&a->emb_re);
    qfree_if(&a->emb_im);
    qfree_if(&a->psi0_re);
    qfree_if(&a->psi0_im);
    qfree_if(&a->inv_norm0);
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        QLayerAct *la = &a->layers[l];
        qfree_if(&la->q_pqc_re); qfree_if(&la->q_pqc_im);
        qfree_if(&la->k_pqc_re); qfree_if(&la->k_pqc_im);
        qfree_if(&la->v_re); qfree_if(&la->v_im);
        qfree_if(&la->q_rope_re); qfree_if(&la->q_rope_im);
        qfree_if(&la->k_rope_re); qfree_if(&la->k_rope_im);
        qfree_if(&la->att_re); qfree_if(&la->att_im); qfree_if(&la->att_w); qfree_if(&la->att_dS);
        qfree_if(&la->attn_out_re); qfree_if(&la->attn_out_im);
        qfree_if(&la->psi_mid_raw_re); qfree_if(&la->psi_mid_raw_im);
        qfree_if(&la->psi_mid_re); qfree_if(&la->psi_mid_im);
        qfree_if(&la->mid_inv_norm);
        qfree_if(&la->u_re); qfree_if(&la->u_im);
        qfree_if(&la->v_kerr_re); qfree_if(&la->v_kerr_im);
        qfree_if(&la->ffn_in_quad); qfree_if(&la->h1_pre); qfree_if(&la->h1_act); qfree_if(&la->delta_quad);
        qfree_if(&la->delta_re); qfree_if(&la->delta_im);
        qfree_if(&la->pre_f2_re); qfree_if(&la->pre_f2_im);
        qfree_if(&la->post_f2_re); qfree_if(&la->post_f2_im);
        qfree_if(&la->psi_out_re); qfree_if(&la->psi_out_im);
        qfree_if(&la->out_inv_norm);
        qfree_if(&la->kv_k_re); qfree_if(&la->kv_k_im);
        qfree_if(&la->kv_v_re); qfree_if(&la->kv_v_im);
    }
    qfree_if(&a->psi_L_quad);
    qfree_if(&a->logits);
    qfree_if(&a->kv_scores);
    qfree_if(&a->d_tc_a); qfree_if(&a->d_tc_b); qfree_if(&a->d_gnorm_blk);
    qfree_if(&a->losses);
    qfree_if(&a->d_logits);
    qfree_if(&a->d_psi_quad);
    qfree_if(&a->d_cur_re); qfree_if(&a->d_cur_im);
    qfree_if(&a->d_nxt_re); qfree_if(&a->d_nxt_im);
    qfree_if(&a->d_tmp1_re); qfree_if(&a->d_tmp1_im);
    qfree_if(&a->d_tmp2_re); qfree_if(&a->d_tmp2_im);
    qfree_if(&a->d_tmp3_re); qfree_if(&a->d_tmp3_im);
    qfree_if(&a->d_q1_re); qfree_if(&a->d_q1_im);
    qfree_if(&a->d_k1_re); qfree_if(&a->d_k1_im);
    qfree_if(&a->d_v1_re); qfree_if(&a->d_v1_im);
    qfree_if(&a->d_pqc_work_re); qfree_if(&a->d_pqc_work_im);
    qfree_if(&a->d_bridge_re); qfree_if(&a->d_bridge_im);
    qfree_if(&a->d_ffn_quad); qfree_if(&a->d_h1);
    qfree_if(&a->d_pqc_tok); qfree_if(&a->d_kerr_tok); qfree_if(&a->d_kb_tok);
    qfree_if(&a->d_bg1); qfree_if(&a->d_bg2);
}

static void launch_pqc_fwd(
    const QModel *m,
    const QKernels *k,
    uint64_t in_re, uint64_t in_im,
    uint64_t out_re, uint64_t out_im,
    uint64_t theta,
    unsigned int BT,
    const char *tag
) {
    unsigned int nq = m->n_qubits;
    unsigned int pd = m->pqc_depth;
    if (m->n_registers <= 1U) {
        void *ps[8] = { &in_re, &in_im, &out_re, &out_im, &theta, &BT, &nq, &pd };
        check_cu(cuLaunchKernel(k->pqc_fwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, ps, NULL), tag);
        return;
    }
    unsigned int n_reg = m->n_registers;
    unsigned int reg_dim = m->reg_dim;
    unsigned int phi_off = n_reg * 3U * pd * nq;
    uint64_t phi = dp_off(theta, phi_off);
    void *pmr[9] = { &in_re, &in_im, &out_re, &out_im, &theta, &BT, &n_reg, &nq, &pd };
    check_cu(cuLaunchKernel(k->pqc_mr_fwd, BT * n_reg, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pmr, NULL), tag);
    void *pbr[6] = { &out_re, &out_im, &phi, &BT, &n_reg, &reg_dim };
    check_cu(cuLaunchKernel(k->reg_bridge_fwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pbr, NULL), "reg_bridge_fwd");
}

static void launch_pqc_bwd(
    const QModel *m,
    const QKernels *k,
    const QActArena *a,
    uint64_t out_re, uint64_t out_im,
    uint64_t dout_re, uint64_t dout_im,
    uint64_t din_re, uint64_t din_im,
    uint64_t theta,
    unsigned int BT,
    const char *tag
) {
    unsigned int nq = m->n_qubits;
    unsigned int pd = m->pqc_depth;
    if (m->n_registers <= 1U) {
        void *pb[13] = {
            &out_re, &out_im, &dout_re, &dout_im,
            (void *)&a->d_pqc_work_re, (void *)&a->d_pqc_work_im, &din_re, &din_im,
            &theta, (void *)&a->d_pqc_tok, &BT, &nq, &pd
        };
        check_cu(cuLaunchKernel(k->pqc_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pb, NULL), tag);
        return;
    }
    unsigned int n_reg = m->n_registers;
    unsigned int reg_dim = m->reg_dim;
    unsigned int phi_off = n_reg * 3U * pd * nq;
    unsigned int np = m->n_pqc_params;
    uint64_t phi = dp_off(theta, phi_off);
    void *pbr[13] = {
        &out_re, &out_im, (void *)&a->d_bridge_re, (void *)&a->d_bridge_im,
        &dout_re, &dout_im, &phi, (void *)&a->d_pqc_tok,
        &BT, &n_reg, &reg_dim, &np, &phi_off
    };
    check_cu(cuLaunchKernel(k->reg_bridge_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pbr, NULL), "reg_bridge_bwd");
    void *pmr[14] = {
        (void *)&a->d_bridge_re, (void *)&a->d_bridge_im, &dout_re, &dout_im,
        (void *)&a->d_pqc_work_re, (void *)&a->d_pqc_work_im, &din_re, &din_im,
        &theta, (void *)&a->d_pqc_tok, &BT, &n_reg, &nq, &pd
    };
    check_cu(cuLaunchKernel(k->pqc_mr_bwd, BT * n_reg, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pmr, NULL), tag);
}

static void launch_linear_fwd(
    const QKernels *k,
    const QActArena *a,
    uint64_t X, uint64_t W, uint64_t b, uint64_t Y,
    unsigned int BT, unsigned int N, unsigned int K,
    int use_tc,
    const char *tag
) {
    if (use_tc && a->d_tc_a && BT >= 16U && (K % 16U) == 0U) {
        unsigned int nX = BT * K;
        unsigned int nW = N * K;
        unsigned int nY = BT * N;
        void *px[3] = { &X, (void *)&a->d_tc_a, &nX };
        check_cu(cuLaunchKernel(k->f32_to_f16, (nX + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), "tc_cast_x");
        void *pw[3] = { &W, (void *)&a->d_tc_b, &nW };
        check_cu(cuLaunchKernel(k->f32_to_f16, (nW + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pw, NULL), "tc_cast_w");
        void *pg[6] = { (void *)&a->d_tc_a, (void *)&a->d_tc_b, &Y, &BT, &N, &K };
        check_cu(cuLaunchKernel(k->gemm_tc_f16, (N + 7U) / 8U, (BT + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), tag);
        void *pb[4] = { &Y, &b, &BT, &N };
        check_cu(cuLaunchKernel(k->add_bias_rows, (nY + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "tc_bias");
        return;
    }
    void *pl[7] = { &X, &W, &b, &Y, &BT, &N, &K };
    check_cu(cuLaunchKernel(k->linear_fwd, (BT * N + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pl, NULL), tag);
}

static void launch_linear_bwd_x(
    const QKernels *k,
    const QActArena *a,
    uint64_t dY, uint64_t W, uint64_t dX,
    unsigned int BT, unsigned int N, unsigned int K,
    int use_tc,
    const char *tag
) {
    if (use_tc && a->d_tc_a && BT >= 16U && (N % 16U) == 0U) {
        unsigned int ndY = BT * N;
        void *py[3] = { &dY, (void *)&a->d_tc_a, &ndY };
        check_cu(cuLaunchKernel(k->f32_to_f16, (ndY + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, py, NULL), "tc_cast_dy");
        void *pw[4] = { &W, (void *)&a->d_tc_b, &N, &K };
        check_cu(cuLaunchKernel(k->transpose_f32_to_f16, (N * K + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pw, NULL), "tc_tr_w");
        void *pg[6] = { (void *)&a->d_tc_a, (void *)&a->d_tc_b, &dX, &BT, &K, &N };
        check_cu(cuLaunchKernel(k->gemm_tc_f16, (K + 7U) / 8U, (BT + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), tag);
        return;
    }
    void *px[6] = { &dY, &W, &dX, &BT, &N, &K };
    check_cu(cuLaunchKernel(k->linear_bwd_x, (BT * K + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), tag);
}

static void launch_linear_bwd_w(
    const QKernels *k,
    const QActArena *a,
    uint64_t dY, uint64_t X, uint64_t dW,
    unsigned int BT, unsigned int N, unsigned int K,
    int use_tc,
    const char *tag
) {
    if (use_tc && a->d_tc_a && BT >= 16U && (BT % 16U) == 0U) {
        void *py[4] = { &dY, (void *)&a->d_tc_a, &BT, &N };
        check_cu(cuLaunchKernel(k->transpose_f32_to_f16, (BT * N + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, py, NULL), "tc_tr_dy");
        void *px[4] = { &X, (void *)&a->d_tc_b, &BT, &K };
        check_cu(cuLaunchKernel(k->transpose_f32_to_f16, (BT * K + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), "tc_tr_x");
        void *pg[6] = { (void *)&a->d_tc_a, (void *)&a->d_tc_b, &dW, &N, &K, &BT };
        check_cu(cuLaunchKernel(k->gemm_tc_f16, (K + 7U) / 8U, (N + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), tag);
        return;
    }
    void *pw[6] = { &dY, &X, &dW, &BT, &N, &K };
    check_cu(cuLaunchKernel(k->linear_bwd_w, (N * K + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pw, NULL), tag);
}

static void run_forward(
    const QModel *m,
    const QKernels *k,
    QActArena *a,
    unsigned int B,
    unsigned int T,
    unsigned int pos_offset,
    int use_tc
) {
    unsigned int BT = B * T;
    unsigned int D = m->dim;
    unsigned int BT_D = BT * D;
    unsigned int BT_2D = BT * (2U * D);
    unsigned int BT_Hff = BT * m->h_ff;
    unsigned int two_D = 2U * D;
    float sqrt_d = (float)sqrt((double)D);
    float inv_sqrt_d = 1.0f / sqrt_d;
    unsigned int grid_bt_d = (BT_D + 255U) / 256U;
    unsigned int grid_bt_h = (BT_Hff + 255U) / 256U;

    {
        uint64_t ere = dp_off(m->d_weights, m->off_E_re);
        uint64_t eim = dp_off(m->d_weights, m->off_E_im);
        void *ps[9] = { &ere, &eim, &a->d_tokens, &a->emb_re, &a->emb_im, &BT, &T, &D, &pos_offset };
        check_cu(cuLaunchKernel(k->embed_fwd, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "embed_fwd");
    }
    {
        void *ps[7] = { &a->emb_re, &a->emb_im, &a->psi0_re, &a->psi0_im, &a->inv_norm0, &BT, &D };
        check_cu(cuLaunchKernel(k->cunit_fwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, ps, NULL), "cunit0_fwd");
    }

    uint64_t x_re = a->psi0_re;
    uint64_t x_im = a->psi0_im;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const QLayerOff *lo = &m->layers[l];
        QLayerAct *la = &a->layers[l];

        uint64_t tq = dp_off(m->d_weights, lo->off_theta_q);
        uint64_t tk = dp_off(m->d_weights, lo->off_theta_k);
        uint64_t tv = dp_off(m->d_weights, lo->off_theta_v);
        uint64_t tf1 = dp_off(m->d_weights, lo->off_theta_f1);
        uint64_t tf2 = dp_off(m->d_weights, lo->off_theta_f2);
        uint64_t db = dp_off(m->d_weights, lo->off_beta);
        uint64_t dg = dp_off(m->d_weights, lo->off_gamma);
        uint64_t dkk = dp_off(m->d_weights, lo->off_kerr_k);
        uint64_t dkb = dp_off(m->d_weights, lo->off_kerr_b);
        uint64_t dw1 = dp_off(m->d_weights, lo->off_w1);
        uint64_t db1 = dp_off(m->d_weights, lo->off_b1);
        uint64_t dw2 = dp_off(m->d_weights, lo->off_w2);
        uint64_t db2 = dp_off(m->d_weights, lo->off_b2);

        launch_pqc_fwd(m, k, x_re, x_im, la->q_pqc_re, la->q_pqc_im, tq, BT, "pqc_q_fwd");
        launch_pqc_fwd(m, k, x_re, x_im, la->k_pqc_re, la->k_pqc_im, tk, BT, "pqc_k_fwd");
        launch_pqc_fwd(m, k, x_re, x_im, la->v_re, la->v_im, tv, BT, "pqc_v_fwd");

        {
            unsigned int dh = m->head_dim;
            void *pr[13] = {
                &la->q_pqc_re, &la->q_pqc_im, &la->k_pqc_re, &la->k_pqc_im,
                &la->q_rope_re, &la->q_rope_im, &la->k_rope_re, &la->k_rope_im,
                &BT, &T, &D, &dh, &pos_offset
            };
            check_cu(cuLaunchKernel(k->qrope_fwd, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pr, NULL), "qrope_fwd");
        }
        {
            unsigned int nh = m->n_heads;
            unsigned int dh = m->head_dim;
            void *pa[17] = {
                &la->q_rope_re, &la->q_rope_im, &la->k_rope_re, &la->k_rope_im,
                &la->v_re, &la->v_im, &db, &dg,
                &la->att_re, &la->att_im, &la->att_w,
                &la->attn_out_re, &la->attn_out_im,
                &B, &T, &nh, &dh
            };
            check_cu(cuLaunchKernel(k->attn_fwd, T, B * nh, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "attn_fwd");
        }
        {
            void *pa1[4] = { &x_re, &la->attn_out_re, &la->psi_mid_raw_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa1, NULL), "add_mid_re");
            void *pa2[4] = { &x_im, &la->attn_out_im, &la->psi_mid_raw_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa2, NULL), "add_mid_im");
            void *pc[7] = { &la->psi_mid_raw_re, &la->psi_mid_raw_im, &la->psi_mid_re, &la->psi_mid_im, &la->mid_inv_norm, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_fwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "cunit_mid_fwd");
        }
        {
            launch_pqc_fwd(m, k, la->psi_mid_re, la->psi_mid_im, la->u_re, la->u_im, tf1, BT, "pqc_f1_fwd");
            void *pkr[8] = { &la->u_re, &la->u_im, &dkk, &dkb, &la->v_kerr_re, &la->v_kerr_im, &BT, &D };
            check_cu(cuLaunchKernel(k->kerr_fwd, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pkr, NULL), "kerr_fwd");
            void *ppq[6] = { &la->v_kerr_re, &la->v_kerr_im, &la->ffn_in_quad, &BT, &D, &sqrt_d };
            check_cu(cuLaunchKernel(k->pack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ppq, NULL), "pack_ffn");

            unsigned int hff = m->h_ff;
            launch_linear_fwd(k, a, la->ffn_in_quad, dw1, db1, la->h1_pre, BT, hff, two_D, use_tc, "lin1_fwd");
            void *psl[3] = { &la->h1_pre, &la->h1_act, &BT_Hff };
            check_cu(cuLaunchKernel(k->silu_fwd, grid_bt_h, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psl, NULL), "silu_fwd");
            launch_linear_fwd(k, a, la->h1_act, dw2, db2, la->delta_quad, BT, two_D, hff, use_tc, "lin2_fwd");
            void *pup[6] = { &la->delta_quad, &la->delta_re, &la->delta_im, &BT, &D, &inv_sqrt_d };
            check_cu(cuLaunchKernel(k->unpack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pup, NULL), "unpack_delta");

            void *pa3[4] = { &la->psi_mid_re, &la->delta_re, &la->pre_f2_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa3, NULL), "add_pf2_re");
            void *pa4[4] = { &la->psi_mid_im, &la->delta_im, &la->pre_f2_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa4, NULL), "add_pf2_im");
            launch_pqc_fwd(m, k, la->pre_f2_re, la->pre_f2_im, la->post_f2_re, la->post_f2_im, tf2, BT, "pqc_f2_fwd");
            void *pc2[7] = { &la->post_f2_re, &la->post_f2_im, &la->psi_out_re, &la->psi_out_im, &la->out_inv_norm, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_fwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc2, NULL), "cunit_out_fwd");
        }
        x_re = la->psi_out_re;
        x_im = la->psi_out_im;
    }

    {
        void *ppq[6] = { &x_re, &x_im, &a->psi_L_quad, &BT, &D, &sqrt_d };
        check_cu(cuLaunchKernel(k->pack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ppq, NULL), "pack_out");
        uint64_t dhout = dp_off(m->d_weights, m->off_H_out);
        uint64_t dbout = dp_off(m->d_weights, m->off_b_out);
        unsigned int V = m->vocab_size;
        launch_linear_fwd(k, a, a->psi_L_quad, dhout, dbout, a->logits, BT, V, two_D, use_tc, "head_fwd");
    }
    (void)BT_2D;
}

static void run_prefill_kv(
    const QModel *m,
    const QKernels *k,
    QActArena *a,
    const uint32_t *ctx,
    unsigned int T
) {
    check_cu(cuMemcpyHtoD_v2(a->d_tokens, ctx, (size_t)T * 4U), "HtoD(kv_prefill)");
    run_forward(m, k, a, 1U, T, 0U, 0);
    unsigned int D = m->dim;
    unsigned int zero_pos = 0U;
    unsigned int grid = (T * D + 255U) / 256U;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const QLayerAct *la = &a->layers[l];
        void *ps[11] = {
            (void *)&la->k_rope_re, (void *)&la->k_rope_im,
            (void *)&la->v_re, (void *)&la->v_im,
            (void *)&la->kv_k_re, (void *)&la->kv_k_im,
            (void *)&la->kv_v_re, (void *)&la->kv_v_im,
            &T, &D, &zero_pos
        };
        check_cu(cuLaunchKernel(k->kv_store_slice, grid, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "kv_prefill_store");
    }
}

static void run_decode_step_kv(
    const QModel *m,
    const QKernels *k,
    QActArena *a,
    uint32_t tok_id,
    unsigned int pos
) {
    unsigned int BT = 1U;
    unsigned int T1 = 1U;
    unsigned int D = m->dim;
    unsigned int two_D = 2U * D;
    float sqrt_d = (float)sqrt((double)D);
    float inv_sqrt_d = 1.0f / sqrt_d;
    unsigned int grid_d = (D + 255U) / 256U;
    unsigned int grid_h = (m->h_ff + 255U) / 256U;

    check_cu(cuMemcpyHtoD_v2(a->d_tokens, &tok_id, 4U), "HtoD(kv_tok)");
    {
        uint64_t ere = dp_off(m->d_weights, m->off_E_re);
        uint64_t eim = dp_off(m->d_weights, m->off_E_im);
        void *ps[9] = { &ere, &eim, &a->d_tokens, &a->emb_re, &a->emb_im, &BT, &T1, &D, &pos };
        check_cu(cuLaunchKernel(k->embed_fwd, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "kv_embed_fwd");
    }
    {
        void *ps[7] = { &a->emb_re, &a->emb_im, &a->psi0_re, &a->psi0_im, &a->inv_norm0, &BT, &D };
        check_cu(cuLaunchKernel(k->cunit_fwd, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, ps, NULL), "kv_cunit0_fwd");
    }

    uint64_t x_re = a->psi0_re;
    uint64_t x_im = a->psi0_im;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const QLayerOff *lo = &m->layers[l];
        QLayerAct *la = &a->layers[l];

        uint64_t tq = dp_off(m->d_weights, lo->off_theta_q);
        uint64_t tk = dp_off(m->d_weights, lo->off_theta_k);
        uint64_t tv = dp_off(m->d_weights, lo->off_theta_v);
        uint64_t tf1 = dp_off(m->d_weights, lo->off_theta_f1);
        uint64_t tf2 = dp_off(m->d_weights, lo->off_theta_f2);
        uint64_t db = dp_off(m->d_weights, lo->off_beta);
        uint64_t dg = dp_off(m->d_weights, lo->off_gamma);
        uint64_t dkk = dp_off(m->d_weights, lo->off_kerr_k);
        uint64_t dkb = dp_off(m->d_weights, lo->off_kerr_b);
        uint64_t dw1 = dp_off(m->d_weights, lo->off_w1);
        uint64_t db1 = dp_off(m->d_weights, lo->off_b1);
        uint64_t dw2 = dp_off(m->d_weights, lo->off_w2);
        uint64_t db2 = dp_off(m->d_weights, lo->off_b2);

        launch_pqc_fwd(m, k, x_re, x_im, la->q_pqc_re, la->q_pqc_im, tq, 1U, "kv_pqc_q");
        launch_pqc_fwd(m, k, x_re, x_im, la->k_pqc_re, la->k_pqc_im, tk, 1U, "kv_pqc_k");
        launch_pqc_fwd(m, k, x_re, x_im, la->v_re, la->v_im, tv, 1U, "kv_pqc_v");

        unsigned int nh = m->n_heads;
        unsigned int dh = m->head_dim;
        {
            void *pr[13] = {
                &la->q_pqc_re, &la->q_pqc_im, &la->k_pqc_re, &la->k_pqc_im,
                &la->q_rope_re, &la->q_rope_im, &la->k_rope_re, &la->k_rope_im,
                &BT, &T1, &D, &dh, &pos
            };
            check_cu(cuLaunchKernel(k->qrope_fwd, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pr, NULL), "kv_qrope");
        }
        {
            void *ps[11] = {
                &la->k_rope_re, &la->k_rope_im, &la->v_re, &la->v_im,
                &la->kv_k_re, &la->kv_k_im, &la->kv_v_re, &la->kv_v_im,
                &T1, &D, &pos
            };
            check_cu(cuLaunchKernel(k->kv_store_slice, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "kv_store1");
            void *pa[14] = {
                &la->q_rope_re, &la->q_rope_im,
                &la->kv_k_re, &la->kv_k_im, &la->kv_v_re, &la->kv_v_im,
                &db, &dg, &a->kv_scores, &la->attn_out_re, &la->attn_out_im,
                &pos, &nh, &dh
            };
            check_cu(cuLaunchKernel(k->attn_decode_step, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "kv_attn_dec");
        }
        {
            void *pa1[4] = { &x_re, &la->attn_out_re, &la->psi_mid_raw_re, &D };
            check_cu(cuLaunchKernel(k->add_f32, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa1, NULL), "kv_add_mr");
            void *pa2[4] = { &x_im, &la->attn_out_im, &la->psi_mid_raw_im, &D };
            check_cu(cuLaunchKernel(k->add_f32, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa2, NULL), "kv_add_mi");
            void *pc[7] = { &la->psi_mid_raw_re, &la->psi_mid_raw_im, &la->psi_mid_re, &la->psi_mid_im, &la->mid_inv_norm, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_fwd, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "kv_cunit_mid");
        }
        {
            launch_pqc_fwd(m, k, la->psi_mid_re, la->psi_mid_im, la->u_re, la->u_im, tf1, 1U, "kv_pqc_f1");
            void *pkr[8] = { &la->u_re, &la->u_im, &dkk, &dkb, &la->v_kerr_re, &la->v_kerr_im, &BT, &D };
            check_cu(cuLaunchKernel(k->kerr_fwd, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pkr, NULL), "kv_kerr");
            void *ppq[6] = { &la->v_kerr_re, &la->v_kerr_im, &la->ffn_in_quad, &BT, &D, &sqrt_d };
            check_cu(cuLaunchKernel(k->pack_quad, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ppq, NULL), "kv_pack_ffn");

            unsigned int hff = m->h_ff;
            launch_linear_fwd(k, a, la->ffn_in_quad, dw1, db1, la->h1_pre, 1U, hff, two_D, 0, "kv_lin1");
            void *psl[3] = { &la->h1_pre, &la->h1_act, &hff };
            check_cu(cuLaunchKernel(k->silu_fwd, grid_h, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psl, NULL), "kv_silu");
            launch_linear_fwd(k, a, la->h1_act, dw2, db2, la->delta_quad, 1U, two_D, hff, 0, "kv_lin2");
            void *pup[6] = { &la->delta_quad, &la->delta_re, &la->delta_im, &BT, &D, &inv_sqrt_d };
            check_cu(cuLaunchKernel(k->unpack_quad, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pup, NULL), "kv_unpack_d");

            void *pa3[4] = { &la->psi_mid_re, &la->delta_re, &la->pre_f2_re, &D };
            check_cu(cuLaunchKernel(k->add_f32, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa3, NULL), "kv_add_pf2r");
            void *pa4[4] = { &la->psi_mid_im, &la->delta_im, &la->pre_f2_im, &D };
            check_cu(cuLaunchKernel(k->add_f32, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa4, NULL), "kv_add_pf2i");
            launch_pqc_fwd(m, k, la->pre_f2_re, la->pre_f2_im, la->post_f2_re, la->post_f2_im, tf2, 1U, "kv_pqc_f2");
            void *pc2[7] = { &la->post_f2_re, &la->post_f2_im, &la->psi_out_re, &la->psi_out_im, &la->out_inv_norm, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_fwd, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc2, NULL), "kv_cunit_out");
        }
        x_re = la->psi_out_re;
        x_im = la->psi_out_im;
    }
    {
        void *ppq[6] = { &x_re, &x_im, &a->psi_L_quad, &BT, &D, &sqrt_d };
        check_cu(cuLaunchKernel(k->pack_quad, grid_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ppq, NULL), "kv_pack_out");
        uint64_t dhout = dp_off(m->d_weights, m->off_H_out);
        uint64_t dbout = dp_off(m->d_weights, m->off_b_out);
        launch_linear_fwd(k, a, a->psi_L_quad, dhout, dbout, a->logits, 1U, m->vocab_size, two_D, 0, "kv_head");
    }
}

static float run_loss_and_backward(
    const QModel *m,
    const QKernels *k,
    QActArena *a,
    unsigned int B,
    unsigned int T,
    int compute_loss_sum,
    int use_tc,
    unsigned int active_count
) {
    unsigned int BT = B * T;
    unsigned int D = m->dim;
    unsigned int BT_D = BT * D;
    unsigned int BT_Hff = BT * m->h_ff;
    unsigned int two_D = 2U * D;
    float sqrt_d = (float)sqrt((double)D);
    float inv_sqrt_d = 1.0f / sqrt_d;
    unsigned int V = m->vocab_size;
    unsigned int grid_bt_d = (BT_D + 255U) / 256U;
    unsigned int grid_bt_h = (BT_Hff + 255U) / 256U;

    check_cu(cuMemsetD8_v2(m->d_grads, 0, (size_t)m->total_params * 4U), "zero(d_grads)");

    if (active_count > 0U) {
        float inv_act = 1.0f / (float)active_count;
        void *pcm[7] = { &a->logits, &a->d_targets, &a->d_logits, &a->losses, &BT, &V, &inv_act };
        check_cu(cuLaunchKernel(k->ce_loss_masked_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pcm, NULL), "ce_loss_masked_bwd");
    } else {
        void *pc[6] = { &a->logits, &a->d_targets, &a->d_logits, &a->losses, &BT, &V };
        check_cu(cuLaunchKernel(k->ce_loss_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "ce_loss_bwd");
    }

    float mean_loss = 0.0f;
    if (compute_loss_sum) {
        float *hl = (float *)malloc((size_t)BT * 4U);
        if (!hl) exit(1);
        check_cu(cuMemcpyDtoH_v2(hl, a->losses, (size_t)BT * 4U), "DtoH(losses)");
        double sum = 0.0;
        for (unsigned int i = 0U; i < BT; i++) sum += (double)hl[i];
        free(hl);
        unsigned int denom = (active_count > 0U) ? active_count : BT;
        mean_loss = (float)(sum / (double)denom);
    }

    {
        uint64_t w_h = dp_off(m->d_weights, m->off_H_out);
        uint64_t g_h = dp_off(m->d_grads, m->off_H_out);
        uint64_t g_b = dp_off(m->d_grads, m->off_b_out);
        launch_linear_bwd_x(k, a, a->d_logits, w_h, a->d_psi_quad, BT, V, two_D, use_tc, "head_bwd_x");
        launch_linear_bwd_w(k, a, a->d_logits, a->psi_L_quad, g_h, BT, V, two_D, use_tc, "head_bwd_w");
        void *pb[4] = { &a->d_logits, &g_b, &BT, &V };
        check_cu(cuLaunchKernel(k->linear_bwd_b, (V + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "head_bwd_b");
        void *pu[6] = { &a->d_psi_quad, &a->d_cur_re, &a->d_cur_im, &BT, &D, &sqrt_d };
        check_cu(cuLaunchKernel(k->unpack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pu, NULL), "head_unpack");
    }

    for (int li = (int)m->n_layers - 1; li >= 0; li--) {
        unsigned int l = (unsigned int)li;
        const QLayerOff *lo = &m->layers[l];
        const QLayerAct *la = &a->layers[l];
        uint64_t x_re = (l == 0U) ? a->psi0_re : a->layers[l - 1U].psi_out_re;
        uint64_t x_im = (l == 0U) ? a->psi0_im : a->layers[l - 1U].psi_out_im;

        uint64_t w_tq = dp_off(m->d_weights, lo->off_theta_q);
        uint64_t w_tk = dp_off(m->d_weights, lo->off_theta_k);
        uint64_t w_tv = dp_off(m->d_weights, lo->off_theta_v);
        uint64_t w_tf1 = dp_off(m->d_weights, lo->off_theta_f1);
        uint64_t w_tf2 = dp_off(m->d_weights, lo->off_theta_f2);
        uint64_t w_db = dp_off(m->d_weights, lo->off_beta);
        uint64_t w_dg = dp_off(m->d_weights, lo->off_gamma);
        uint64_t w_dkk = dp_off(m->d_weights, lo->off_kerr_k);
        uint64_t w_dkb = dp_off(m->d_weights, lo->off_kerr_b);
        uint64_t w_w1 = dp_off(m->d_weights, lo->off_w1);
        uint64_t w_w2 = dp_off(m->d_weights, lo->off_w2);

        uint64_t g_tq = dp_off(m->d_grads, lo->off_theta_q);
        uint64_t g_tk = dp_off(m->d_grads, lo->off_theta_k);
        uint64_t g_tv = dp_off(m->d_grads, lo->off_theta_v);
        uint64_t g_tf1 = dp_off(m->d_grads, lo->off_theta_f1);
        uint64_t g_tf2 = dp_off(m->d_grads, lo->off_theta_f2);
        uint64_t g_db = dp_off(m->d_grads, lo->off_beta);
        uint64_t g_dg = dp_off(m->d_grads, lo->off_gamma);
        uint64_t g_dkk = dp_off(m->d_grads, lo->off_kerr_k);
        uint64_t g_dkb = dp_off(m->d_grads, lo->off_kerr_b);
        uint64_t g_w1 = dp_off(m->d_grads, lo->off_w1);
        uint64_t g_b1 = dp_off(m->d_grads, lo->off_b1);
        uint64_t g_w2 = dp_off(m->d_grads, lo->off_w2);
        uint64_t g_b2 = dp_off(m->d_grads, lo->off_b2);

        unsigned int np = m->n_pqc_params;
        unsigned int hff = m->h_ff;
        unsigned int nh = m->n_heads;
        unsigned int dh = m->head_dim;

        /* 1. Backprop cunit_norm(post_f2) -> d_tmp1 */
        {
            void *pc[9] = { &la->psi_out_re, &la->psi_out_im, &a->d_cur_re, &a->d_cur_im, &la->out_inv_norm, &a->d_tmp1_re, &a->d_tmp1_im, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "cunit_out_bwd");
        }
        /* 2. Backprop U_f2 -> d_tmp2 (= d_pre_f2) + g_tf2 */
        {
            launch_pqc_bwd(m, k, a, la->post_f2_re, la->post_f2_im, a->d_tmp1_re, a->d_tmp1_im, a->d_tmp2_re, a->d_tmp2_im, w_tf2, BT, "pqc_f2_bwd");
            void *pr[4] = { &a->d_pqc_tok, &g_tf2, &BT, &np };
            check_cu(cuLaunchKernel(k->reduce_rows, (np + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pr, NULL), "red_f2");
        }
        /* 3. Backprop W2 -> SiLU -> W1 -> unpack delta (= d_tmp2) */
        {
            void *pp[6] = { &a->d_tmp2_re, &a->d_tmp2_im, &a->d_ffn_quad, &BT, &D, &inv_sqrt_d };
            check_cu(cuLaunchKernel(k->pack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pp, NULL), "pack_df2");
            launch_linear_bwd_x(k, a, a->d_ffn_quad, w_w2, a->d_h1, BT, two_D, hff, use_tc, "lin2_bwd_x");
            launch_linear_bwd_w(k, a, a->d_ffn_quad, la->h1_act, g_w2, BT, two_D, hff, use_tc, "lin2_bwd_w");
            void *pb2[4] = { &a->d_ffn_quad, &g_b2, &BT, &two_D };
            check_cu(cuLaunchKernel(k->linear_bwd_b, (two_D + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pb2, NULL), "lin2_bwd_b");
            void *ps[4] = { &la->h1_pre, &a->d_h1, &a->d_h1, &BT_Hff };
            check_cu(cuLaunchKernel(k->silu_bwd, grid_bt_h, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "silu_bwd");
            launch_linear_bwd_x(k, a, a->d_h1, w_w1, a->d_ffn_quad, BT, hff, two_D, use_tc, "lin1_bwd_x");
            launch_linear_bwd_w(k, a, a->d_h1, la->ffn_in_quad, g_w1, BT, hff, two_D, use_tc, "lin1_bwd_w");
            void *pb1[4] = { &a->d_h1, &g_b1, &BT, &hff };
            check_cu(cuLaunchKernel(k->linear_bwd_b, (hff + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pb1, NULL), "lin1_bwd_b");
            void *pu[6] = { &a->d_ffn_quad, &a->d_tmp1_re, &a->d_tmp1_im, &BT, &D, &sqrt_d };
            check_cu(cuLaunchKernel(k->unpack_quad, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pu, NULL), "unpack_dvk");
        }
        /* 4. Backprop Optical Kerr (d_tmp1 -> d_tmp3) + reduce g_kerr_k, g_kerr_b */
        {
            void *pkb[14] = {
                &la->u_re, &la->u_im, &la->v_kerr_re, &la->v_kerr_im,
                &a->d_tmp1_re, &a->d_tmp1_im, &w_dkk, &w_dkb,
                &a->d_tmp3_re, &a->d_tmp3_im, &a->d_kerr_tok, &a->d_kb_tok,
                &BT, &D
            };
            check_cu(cuLaunchKernel(k->kerr_bwd, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pkb, NULL), "kerr_bwd");
            void *rk1[4] = { &a->d_kerr_tok, &g_dkk, &BT, &D };
            check_cu(cuLaunchKernel(k->reduce_rows, (D + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rk1, NULL), "red_kk");
            void *rk2[4] = { &a->d_kb_tok, &g_dkb, &BT, &D };
            check_cu(cuLaunchKernel(k->reduce_rows, (D + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rk2, NULL), "red_kb");
        }
        /* 5. Backprop U_f1 (d_tmp3 -> d_tmp1) + residual d_psi_mid = d_tmp2 + d_tmp1 */
        {
            launch_pqc_bwd(m, k, a, la->u_re, la->u_im, a->d_tmp3_re, a->d_tmp3_im, a->d_tmp1_re, a->d_tmp1_im, w_tf1, BT, "pqc_f1_bwd");
            void *pr[4] = { &a->d_pqc_tok, &g_tf1, &BT, &np };
            check_cu(cuLaunchKernel(k->reduce_rows, (np + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pr, NULL), "red_f1");
            void *pa1[4] = { &a->d_tmp1_re, &a->d_tmp2_re, &a->d_tmp1_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa1, NULL), "dmid_re");
            void *pa2[4] = { &a->d_tmp1_im, &a->d_tmp2_im, &a->d_tmp1_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pa2, NULL), "dmid_im");
        }
        /* 6. Backprop cunit_norm(psi_mid_raw) -> d_tmp2 (= d_attn_out AND skip to x_in) */
        {
            void *pc[9] = { &la->psi_mid_re, &la->psi_mid_im, &a->d_tmp1_re, &a->d_tmp1_im, &la->mid_inv_norm, &a->d_tmp2_re, &a->d_tmp2_im, &BT, &D };
            check_cu(cuLaunchKernel(k->cunit_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "cunit_mid_bwd");
        }
        /* 7. Backprop Born-Rule Attention -> d_q1, d_k1, d_v1 + g_beta, g_gamma */
        {
            void *paq[20] = {
                &la->k_rope_re, &la->k_rope_im, &la->v_re, &la->v_im,
                &w_db, &w_dg, &la->att_re, &la->att_im, &la->att_w,
                &a->d_tmp2_re, &a->d_tmp2_im, &la->att_dS,
                &a->d_q1_re, &a->d_q1_im, &a->d_bg1, &a->d_bg2,
                &B, &T, &nh, &dh
            };
            check_cu(cuLaunchKernel(k->attn_bwd_q, T, B * nh, 1U, 32U, 1U, 1U, 0U, NULL, paq, NULL), "attn_bwd_q");
            void *rh1[5] = { &a->d_bg1, &g_db, &B, &T, &nh };
            check_cu(cuLaunchKernel(k->reduce_head, (nh + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rh1, NULL), "red_beta");
            void *rh2[5] = { &a->d_bg2, &g_dg, &B, &T, &nh };
            check_cu(cuLaunchKernel(k->reduce_head, (nh + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rh2, NULL), "red_gamma");
            void *pakv[18] = {
                &la->q_rope_re, &la->q_rope_im, &w_db, &w_dg,
                &la->att_re, &la->att_im, &la->att_w, &la->att_dS,
                &a->d_tmp2_re, &a->d_tmp2_im,
                &a->d_k1_re, &a->d_k1_im, &a->d_v1_re, &a->d_v1_im,
                &B, &T, &nh, &dh
            };
            check_cu(cuLaunchKernel(k->attn_bwd_kv, T, B * nh, 1U, 32U, 1U, 1U, 0U, NULL, pakv, NULL), "attn_bwd_kv");
        }
        /* 8. Backprop Per-Head Quantum Phase-RoPE -> inverse rotate d_q1, d_k1 in-place */
        {
            void *pr[8] = { &a->d_q1_re, &a->d_q1_im, &a->d_k1_re, &a->d_k1_im, &BT, &T, &D, &dh };
            check_cu(cuLaunchKernel(k->qrope_bwd, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pr, NULL), "qrope_bwd");
        }
        /* 9. Backprop U_Q, U_K, U_V and sum with skip d_tmp2 -> d_nxt (which becomes next step d_cur) */
        {
            launch_pqc_bwd(m, k, a, la->q_pqc_re, la->q_pqc_im, a->d_q1_re, a->d_q1_im, a->d_nxt_re, a->d_nxt_im, w_tq, BT, "pqc_q_bwd");
            void *rq[4] = { &a->d_pqc_tok, &g_tq, &BT, &np };
            check_cu(cuLaunchKernel(k->reduce_rows, (np + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rq, NULL), "red_q");

            launch_pqc_bwd(m, k, a, la->k_pqc_re, la->k_pqc_im, a->d_k1_re, a->d_k1_im, a->d_tmp1_re, a->d_tmp1_im, w_tk, BT, "pqc_k_bwd");
            void *rk[4] = { &a->d_pqc_tok, &g_tk, &BT, &np };
            check_cu(cuLaunchKernel(k->reduce_rows, (np + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rk, NULL), "red_k");
            void *ak1[4] = { &a->d_nxt_re, &a->d_tmp1_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_inplace, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ak1, NULL), "add_dk_re");
            void *ak2[4] = { &a->d_nxt_im, &a->d_tmp1_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_inplace, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ak2, NULL), "add_dk_im");

            launch_pqc_bwd(m, k, a, la->v_re, la->v_im, a->d_v1_re, a->d_v1_im, a->d_tmp1_re, a->d_tmp1_im, w_tv, BT, "pqc_v_bwd");
            void *rv[4] = { &a->d_pqc_tok, &g_tv, &BT, &np };
            check_cu(cuLaunchKernel(k->reduce_rows, (np + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, rv, NULL), "red_v");
            void *av1[4] = { &a->d_nxt_re, &a->d_tmp1_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_inplace, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, av1, NULL), "add_dv_re");
            void *av2[4] = { &a->d_nxt_im, &a->d_tmp1_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_inplace, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, av2, NULL), "add_dv_im");

            void *as1[4] = { &a->d_nxt_re, &a->d_tmp2_re, &a->d_cur_re, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, as1, NULL), "add_skip_re");
            void *as2[4] = { &a->d_nxt_im, &a->d_tmp2_im, &a->d_cur_im, &BT_D };
            check_cu(cuLaunchKernel(k->add_f32, grid_bt_d, 1U, 1U, 256U, 1U, 1U, 0U, NULL, as2, NULL), "add_skip_im");
        }
        (void)x_re; (void)x_im;
    }

    /* 10. Backprop cunit_norm(emb) -> embed_qrope_bwd */
    {
        void *pc[9] = { &a->psi0_re, &a->psi0_im, &a->d_cur_re, &a->d_cur_im, &a->inv_norm0, &a->d_tmp1_re, &a->d_tmp1_im, &BT, &D };
        check_cu(cuLaunchKernel(k->cunit_bwd, BT, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pc, NULL), "cunit0_bwd");
        uint64_t g_ere = dp_off(m->d_grads, m->off_E_re);
        uint64_t g_eim = dp_off(m->d_grads, m->off_E_im);
        void *pe[9] = { &a->d_tmp1_re, &a->d_tmp1_im, &a->d_tokens, &g_ere, &g_eim, &BT, &T, &D, &V };
        check_cu(cuLaunchKernel(k->embed_bwd, (V * D + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pe, NULL), "embed_bwd");
    }

    return mean_loss;
}

static void run_adamw_update(
    const QModel *m,
    const QKernels *k,
    const QActArena *a,
    float lr,
    float wd,
    float grad_clip,
    unsigned int step_1based
) {
    unsigned int n = m->total_params;
    uint64_t dw = m->d_weights, dg = m->d_grads, dm = m->d_adam_m, dv = m->d_adam_v;
    if (grad_clip > 0.0f && a && a->d_gnorm_blk) {
        uint64_t dgb = a->d_gnorm_blk;
        void *psq[3] = { &dg, &dgb, &n };
        check_cu(cuLaunchKernel(k->sq_sum_blocks, 256U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, psq, NULL), "gnorm_sq");
        float hblk[256];
        check_cu(cuMemcpyDtoH_v2(hblk, a->d_gnorm_blk, 256U * 4U), "DtoH(gnorm)");
        double sum_sq = 0.0;
        for (unsigned int i = 0U; i < 256U; i++) sum_sq += (double)hblk[i];
        float gnorm = (float)sqrt(sum_sq);
        if (gnorm > grad_clip) {
            float scale = grad_clip / (gnorm + 1e-12f);
            void *psc[3] = { &dg, &scale, &n };
            check_cu(cuLaunchKernel(k->scale_f32, (n + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psc, NULL), "gnorm_scale");
        }
    }
    float elem_clip = 0.0f;
    float beta1 = 0.9f, beta2 = 0.999f, eps = 1e-8f;
    float bc1_inv = (float)(1.0 / (1.0 - pow((double)beta1, (double)step_1based)));
    float bc2_inv = (float)(1.0 / (1.0 - pow((double)beta2, (double)step_1based)));
    unsigned int g = (n + 255U) / 256U;
    void *ps[13] = { &dw, &dg, &dm, &dv, &n, &lr, &beta1, &beta2, &eps, &wd, &bc1_inv, &bc2_inv, &elem_clip };
    check_cu(cuLaunchKernel(k->adamw_step, g, 1U, 1U, 256U, 1U, 1U, 0U, NULL, ps, NULL), "adamw_step");
}

typedef struct {
    float max_norm_err;
    float mean_renyi2_bits;
    float mean_linear_entropy;
} QDiag;

static void compute_state_entropy(
    const float *pr,
    const float *pi,
    unsigned int n_qubits,
    double *out_norm_err,
    double *out_s2_bits,
    double *out_linear_s
) {
    unsigned int D = 1U << n_qubits;
    unsigned int nq_a = n_qubits / 2U;
    unsigned int nq_b = n_qubits - nq_a;
    unsigned int dA = 1U << nq_a;
    unsigned int dB = 1U << nq_b;

    double n2 = 0.0;
    for (unsigned int k = 0U; k < D; k++) {
        n2 += (double)pr[k] * (double)pr[k] + (double)pi[k] * (double)pi[k];
    }
    double err = fabs(n2 - 1.0);

    double tr_rho2 = 0.0;
    for (unsigned int a1 = 0U; a1 < dA; a1++) {
        for (unsigned int a2 = 0U; a2 < dA; a2++) {
            double rr = 0.0, ri = 0.0;
            for (unsigned int b = 0U; b < dB; b++) {
                unsigned int i1 = a1 * dB + b;
                unsigned int i2 = a2 * dB + b;
                double r1 = (double)pr[i1], im1 = (double)pi[i1];
                double r2 = (double)pr[i2], im2 = (double)pi[i2];
                rr += r1 * r2 + im1 * im2;
                ri += im1 * r2 - r1 * im2;
            }
            tr_rho2 += rr * rr + ri * ri;
        }
    }
    if (n2 > 1e-12) tr_rho2 /= (n2 * n2);
    else tr_rho2 = 1.0;
    if (tr_rho2 < 1.0 / (double)dA) tr_rho2 = 1.0 / (double)dA;
    if (tr_rho2 > 1.0) tr_rho2 = 1.0;

    if (out_norm_err) *out_norm_err = err;
    if (out_s2_bits) *out_s2_bits = -log(tr_rho2) * 1.4426950408889634;
    if (out_linear_s) *out_linear_s = 1.0 - tr_rho2;
}

static QDiag measure_quantum_diagnostics(const QModel *m, const QActArena *a, unsigned int BT) {
    unsigned int D = m->dim;
    size_t sz = (size_t)BT * (size_t)D * 4U;
    float *re = (float *)malloc(sz);
    float *im = (float *)malloc(sz);
    if (!re || !im) exit(1);

    const QLayerAct *last = &a->layers[m->n_layers - 1U];
    check_cu(cuMemcpyDtoH_v2(re, last->psi_out_re, sz), "DtoH(diag_re)");
    check_cu(cuMemcpyDtoH_v2(im, last->psi_out_im, sz), "DtoH(diag_im)");

    double max_err = 0.0, sum_s2 = 0.0, sum_sl = 0.0;
    unsigned int samples = (BT < 64U) ? BT : 64U;
    unsigned int n_reg = (m->n_registers > 0U) ? m->n_registers : 1U;
    unsigned int reg_dim = (m->reg_dim > 0U) ? m->reg_dim : D;
    for (unsigned int t = 0U; t < samples; t++) {
        const float *pr = re + (size_t)t * D;
        const float *pi = im + (size_t)t * D;
        if (n_reg <= 1U) {
            double err = 0.0, s2 = 0.0, sl = 0.0;
            compute_state_entropy(pr, pi, m->n_qubits, &err, &s2, &sl);
            if (err > max_err) max_err = err;
            sum_s2 += s2;
            sum_sl += sl;
        } else {
            double n2_tot = 0.0;
            for (unsigned int k = 0U; k < D; k++) {
                n2_tot += (double)pr[k] * (double)pr[k] + (double)pi[k] * (double)pi[k];
            }
            double err = fabs(n2_tot - 1.0);
            if (err > max_err) max_err = err;
            double s2_tok = 0.0, sl_tok = 0.0;
            for (unsigned int r = 0U; r < n_reg; r++) {
                const float *rr = pr + (size_t)r * reg_dim;
                const float *ri = pi + (size_t)r * reg_dim;
                double wr = 0.0;
                for (unsigned int k = 0U; k < reg_dim; k++) {
                    wr += (double)rr[k] * (double)rr[k] + (double)ri[k] * (double)ri[k];
                }
                if (wr > 1e-12 && n2_tot > 1e-12) {
                    double s2_r = 0.0, sl_r = 0.0;
                    compute_state_entropy(rr, ri, m->n_qubits, NULL, &s2_r, &sl_r);
                    double w = wr / n2_tot;
                    s2_tok += w * s2_r;
                    sl_tok += w * sl_r;
                }
            }
            sum_s2 += s2_tok;
            sum_sl += sl_tok;
        }
    }
    free(re); free(im);

    QDiag d;
    d.max_norm_err = (float)max_err;
    d.mean_renyi2_bits = (float)(sum_s2 / (double)samples);
    d.mean_linear_entropy = (float)(sum_sl / (double)samples);
    return d;
}

typedef struct {
    uint32_t magic0;
    uint32_t magic1;
    uint32_t version;
    uint32_t vocab_size;
    uint32_t n_qubits;
    uint32_t dim;
    uint32_t n_heads;
    uint32_t n_layers;
    uint32_t pqc_depth;
    uint32_t h_ff;
    uint32_t total_params;
    uint32_t pad0;
    uint64_t seed;
    uint64_t fnv64;
} ZqnnHeader;

static int save_zqnn(QModel *m, const char *path) {
    size_t sz = (size_t)m->total_params * sizeof(float);
    check_cu(cuMemcpyDtoH_v2(m->h_weights, m->d_weights, sz), "DtoH(save)");
    ZqnnHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic0 = QLLM_MAGIC0;
    hdr.magic1 = QLLM_MAGIC1;
    hdr.version = (m->n_registers > 1U) ? 3U : ((m->vocab_size >= 256U || m->version >= 2U) ? 2U : 1U);
    hdr.vocab_size = m->vocab_size;
    hdr.n_qubits = m->n_qubits;
    hdr.dim = m->dim;
    hdr.n_heads = m->n_heads;
    hdr.n_layers = m->n_layers;
    hdr.pqc_depth = m->pqc_depth;
    hdr.h_ff = m->h_ff;
    hdr.total_params = m->total_params;
    hdr.pad0 = (m->vocab_size >= 256U) ? m->n_merges : 0U;
    hdr.seed = m->seed;

    uint16_t *merge_buf = NULL;
    size_t merge_bytes = 0U;
    uint64_t h = 0xCBF29CE484222325ULL;
    if (m->vocab_size >= 256U && m->n_merges > 0U) {
        merge_bytes = (size_t)m->n_merges * 4U;
        merge_buf = (uint16_t *)malloc(merge_bytes);
        if (!merge_buf) return -1;
        for (unsigned int i = 0U; i < m->n_merges; i++) {
            merge_buf[i * 2U] = m->bpe_left[i];
            merge_buf[i * 2U + 1U] = m->bpe_right[i];
        }
        h = fnv1a_update64(h, merge_buf, merge_bytes);
    }
    hdr.fnv64 = fnv1a_update64(h, m->h_weights, sz);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        if (merge_buf) free(merge_buf);
        fprintf(stderr, "IO_ERROR: cannot open '%s' for writing\n", path);
        return -1;
    }
    int bad = (fwrite(&hdr, 1U, sizeof(hdr), fp) != sizeof(hdr));
    if (!bad && merge_bytes > 0U) bad = (fwrite(merge_buf, 1U, merge_bytes, fp) != merge_bytes);
    if (!bad) bad = (fwrite(m->h_weights, 1U, sz, fp) != sz);
    fclose(fp);
    if (merge_buf) free(merge_buf);
    if (bad) {
        fprintf(stderr, "IO_ERROR: short write to '%s'\n", path);
        return -1;
    }
    return 0;
}

static int load_zqnn(QModel *m, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "IO_ERROR: cannot open '%s'\n", path);
        return -1;
    }
    ZqnnHeader hdr;
    if (fread(&hdr, 1U, sizeof(hdr), fp) != sizeof(hdr)) {
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: truncated header in '%s'\n", path);
        return -1;
    }
    if (hdr.magic0 != QLLM_MAGIC0 || hdr.magic1 != QLLM_MAGIC1 ||
        (hdr.version != 1U && hdr.version != 2U && hdr.version != 3U)) {
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: bad magic or version in '%s'\n", path);
        return -1;
    }
    if (hdr.version == 1U) {
        if (hdr.vocab_size != QLLM_VOCAB || hdr.pad0 != 0U) {
            fclose(fp);
            fprintf(stderr, "ZQNN_INVALID: invalid v1 vocab header in '%s'\n", path);
            return -1;
        }
    } else if (hdr.version == 2U) {
        if (hdr.vocab_size < 256U || hdr.vocab_size > QLLM_MAX_VOCAB || hdr.pad0 != (hdr.vocab_size - 256U)) {
            fclose(fp);
            fprintf(stderr, "ZQNN_INVALID: invalid v2 BPE vocab header in '%s'\n", path);
            return -1;
        }
    } else {
        int ok_ascii = (hdr.vocab_size == QLLM_VOCAB && hdr.pad0 == 0U);
        int ok_bpe = (hdr.vocab_size >= 256U && hdr.vocab_size <= QLLM_MAX_VOCAB && hdr.pad0 == (hdr.vocab_size - 256U));
        if (!ok_ascii && !ok_bpe) {
            fclose(fp);
            fprintf(stderr, "ZQNN_INVALID: invalid v3 vocab header in '%s'\n", path);
            return -1;
        }
    }
    unsigned int reg_dim = (hdr.n_qubits >= 4U && hdr.n_qubits <= 7U) ? (1U << hdr.n_qubits) : 0U;
    unsigned int n_reg = (reg_dim > 0U && (hdr.dim % reg_dim) == 0U) ? (hdr.dim / reg_dim) : 0U;
    int valid_nreg = (hdr.version < 3U) ? (n_reg == 1U) : (n_reg == 1U || n_reg == 2U || n_reg == 4U || n_reg == 8U);
    if (reg_dim == 0U || !valid_nreg ||
        hdr.n_heads == 0U || (hdr.dim % hdr.n_heads) != 0U ||
        hdr.n_layers == 0U || hdr.n_layers > QLLM_MAX_LAYERS ||
        hdr.pqc_depth == 0U || hdr.pqc_depth > 6U || hdr.h_ff != 2U * hdr.dim) {
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: invalid model geometry in '%s'\n", path);
        return -1;
    }
    bpe_free_vocab(m);
    m->version = hdr.version;
    m->vocab_size = hdr.vocab_size;
    m->n_merges = (hdr.vocab_size >= 256U) ? hdr.pad0 : 0U;
    m->n_qubits = hdr.n_qubits;
    m->n_registers = n_reg;
    m->n_heads = hdr.n_heads;
    m->n_layers = hdr.n_layers;
    m->pqc_depth = hdr.pqc_depth;
    m->seed = hdr.seed;
    compute_model_layout(m);
    if (hdr.total_params != m->total_params) {
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: total_params mismatch (%u != %u)\n", hdr.total_params, m->total_params);
        return -1;
    }

    uint16_t *merge_buf = NULL;
    size_t merge_bytes = (size_t)m->n_merges * 4U;
    if (merge_bytes > 0U) {
        merge_buf = (uint16_t *)malloc(merge_bytes);
        if (!merge_buf) { fclose(fp); fprintf(stderr, "FATAL: OOM merges\n"); return -1; }
        if (fread(merge_buf, 1U, merge_bytes, fp) != merge_bytes) {
            free(merge_buf); fclose(fp);
            fprintf(stderr, "ZQNN_INVALID: truncated BPE merges in '%s'\n", path);
            return -1;
        }
    }

    size_t sz = (size_t)m->total_params * sizeof(float);
    m->h_weights = (float *)malloc(sz);
    if (!m->h_weights) { if (merge_buf) free(merge_buf); fclose(fp); fprintf(stderr, "FATAL: OOM load\n"); return -1; }
    if (fread(m->h_weights, 1U, sz, fp) != sz) {
        if (merge_buf) free(merge_buf);
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: truncated payload in '%s'\n", path);
        return -1;
    }
    uint8_t extra = 0U;
    if (fread(&extra, 1U, 1U, fp) != 0U) {
        if (merge_buf) free(merge_buf);
        fclose(fp);
        fprintf(stderr, "ZQNN_INVALID: unexpected trailing bytes in '%s'\n", path);
        return -1;
    }
    fclose(fp);

    uint64_t got = 0xCBF29CE484222325ULL;
    if (merge_bytes > 0U) got = fnv1a_update64(got, merge_buf, merge_bytes);
    got = fnv1a_update64(got, m->h_weights, sz);
    if (got != hdr.fnv64) {
        if (merge_buf) free(merge_buf);
        fprintf(stderr, "ZQNN_CHECKSUM_MISMATCH: expected=%016llx got=%016llx\n",
                (unsigned long long)hdr.fnv64, (unsigned long long)got);
        return -1;
    }

    if (m->vocab_size >= 256U && m->n_merges > 0U) {
        m->bpe_left = (uint16_t *)malloc((size_t)m->n_merges * sizeof(uint16_t));
        m->bpe_right = (uint16_t *)malloc((size_t)m->n_merges * sizeof(uint16_t));
        if (!m->bpe_left || !m->bpe_right) { if (merge_buf) free(merge_buf); return -1; }
        for (unsigned int i = 0U; i < m->n_merges; i++) {
            uint16_t l = merge_buf[i * 2U];
            uint16_t r = merge_buf[i * 2U + 1U];
            if ((unsigned int)l >= 256U + i || (unsigned int)r >= 256U + i) {
                free(merge_buf);
                fprintf(stderr, "ZQNN_INVALID: acyclic BPE merge violation at %u (%u, %u)\n", i, (unsigned int)l, (unsigned int)r);
                return -1;
            }
            m->bpe_left[i] = l;
            m->bpe_right[i] = r;
        }
        if (bpe_build_vocab_strings(m) != 0) {
            free(merge_buf);
            fprintf(stderr, "ZQNN_INVALID: failed building BPE string table\n");
            return -1;
        }
    }
    if (merge_buf) free(merge_buf);
    return 0;
}

static int export_openqasm3(QModel *m, const char *path) {
    size_t sz = (size_t)m->total_params * sizeof(float);
    if (m->d_weights) check_cu(cuMemcpyDtoH_v2(m->h_weights, m->d_weights, sz), "DtoH(qasm)");
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "IO_ERROR: cannot open '%s' for OpenQASM export\n", path);
        return -1;
    }
    fprintf(fp, "OPENQASM 3.0;\ninclude \"stdgates.inc\";\n");
    unsigned int n_reg = (m->n_registers > 0U) ? m->n_registers : 1U;
    unsigned int nq = m->n_qubits;
    unsigned int nqp = 3U * nq;
    static const char *const k_slot_names[5] = { "U_Q", "U_K", "U_V", "U_f1", "U_f2" };

    if (n_reg <= 1U) {
        fprintf(fp, "// ZKAEDI Sovereign Hybrid Q-Transformer PQC Circuit Export\n");
        fprintf(fp, "// n_qubits=%u n_layers=%u pqc_depth=%u\n", m->n_qubits, m->n_layers, m->pqc_depth);
        fprintf(fp, "qubit[%u] q;\n\n", m->n_qubits);

        for (unsigned int l = 0U; l < m->n_layers; l++) {
            const QLayerOff *lo = &m->layers[l];
            unsigned int offs[5] = { lo->off_theta_q, lo->off_theta_k, lo->off_theta_v, lo->off_theta_f1, lo->off_theta_f2 };
            for (int s = 0; s < 5; s++) {
                fprintf(fp, "// Layer %u Block %s\n", l, k_slot_names[s]);
                const float *th = m->h_weights + offs[s];
                for (unsigned int p = 0U; p < m->pqc_depth; p++) {
                    for (unsigned int q = 0U; q < nq; q++) {
                        fprintf(fp, "ry(%.9g) q[%u];\n", (double)th[p * nqp + q], q);
                    }
                    for (unsigned int q = 0U; q < nq; q++) {
                        fprintf(fp, "rz(%.9g) q[%u];\n", (double)th[p * nqp + nq + q], q);
                    }
                    for (unsigned int q = 0U; q < nq; q++) {
                        unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
                        fprintf(fp, "cx q[%u], q[%u];\n", q, tgt);
                    }
                    for (unsigned int q = 0U; q < nq; q++) {
                        unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
                        fprintf(fp, "cp(%.9g) q[%u], q[%u];\n", (double)th[p * nqp + 2U * nq + q], q, tgt);
                    }
                }
                fprintf(fp, "\n");
            }
        }
    } else {
        fprintf(fp, "// ZKAEDI Sovereign Multi-Register Coupled Q-Transformer PQC Export\n");
        fprintf(fp, "// n_registers=%u qubits_per_reg=%u logical_qubits=%u n_layers=%u pqc_depth=%u\n",
                n_reg, nq, n_reg * nq, m->n_layers, m->pqc_depth);
        for (unsigned int r = 0U; r < n_reg; r++) {
            fprintf(fp, "qubit[%u] r%u;\n", nq, r);
        }
        fprintf(fp, "\n");

        unsigned int per_reg = m->pqc_depth * nqp;
        for (unsigned int l = 0U; l < m->n_layers; l++) {
            const QLayerOff *lo = &m->layers[l];
            unsigned int offs[5] = { lo->off_theta_q, lo->off_theta_k, lo->off_theta_v, lo->off_theta_f1, lo->off_theta_f2 };
            for (int s = 0; s < 5; s++) {
                const float *base = m->h_weights + offs[s];
                for (unsigned int r = 0U; r < n_reg; r++) {
                    fprintf(fp, "// Layer %u Block %s Register %u\n", l, k_slot_names[s], r);
                    const float *th = base + r * per_reg;
                    for (unsigned int p = 0U; p < m->pqc_depth; p++) {
                        for (unsigned int q = 0U; q < nq; q++) {
                            fprintf(fp, "ry(%.9g) r%u[%u];\n", (double)th[p * nqp + q], r, q);
                        }
                        for (unsigned int q = 0U; q < nq; q++) {
                            fprintf(fp, "rz(%.9g) r%u[%u];\n", (double)th[p * nqp + nq + q], r, q);
                        }
                        for (unsigned int q = 0U; q < nq; q++) {
                            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
                            fprintf(fp, "cx r%u[%u], r%u[%u];\n", r, q, r, tgt);
                        }
                        for (unsigned int q = 0U; q < nq; q++) {
                            unsigned int tgt = (q + 1U == nq) ? 0U : (q + 1U);
                            fprintf(fp, "cp(%.9g) r%u[%u], r%u[%u];\n", (double)th[p * nqp + 2U * nq + q], r, q, r, tgt);
                        }
                    }
                    fprintf(fp, "\n");
                }
                const float *phi = base + n_reg * per_reg;
                fprintf(fp, "// Inter-Register Cyclic Givens Entangler Bridges (phi[0..%u]):", n_reg - 1U);
                for (unsigned int r = 0U; r < n_reg; r++) {
                    fprintf(fp, " %u->%u=%.9g", r, (r + 1U) % n_reg, (double)phi[r]);
                }
                fprintf(fp, "\n\n");
            }
        }
    }
    fclose(fp);
    return 0;
}

static const char k_builtin_corpus[] =
    "Q: What is ZKAEDI Q-Transformer?\n"
    "A: ZKAEDI Q-Transformer is a sovereign complex-wavefunction neural language model compiled directly by ZCC into Blackwell PTX with unitary parameterized quantum circuits, Born-rule interference attention, and optical Kerr nonlinearities.\n\n"
    "Q: What is ZCC?\n"
    "A: ZCC is a self-hosting zero-toolchain C99 compiler generating Linux ELF, WebAssembly WASI, Win64 PE32+, ARM64, RISC-V, and NVIDIA sm_120 PTX directly.\n\n"
    "Q: How does Born attention work?\n"
    "A: Each layer evolves unit states via U_Q, U_K, and U_V circuits, applies Quantum Phase-RoPE, and scores attention via Born overlap |<K|Q>|^2 and real coherence Re<K|Q>.\n\n"
    "Q: How does Optical Kerr nonlinearity work?\n"
    "A: Optical Kerr rotates each state amplitude by a norm-preserving self-phase phi_k proportional to intensity |u_k|^2.\n\n"
    "int fib(int n) {\n"
    "    int a = 0, b = 1;\n"
    "    for (int i = 0; i < n; i++) {\n"
    "        int t = a + b;\n"
    "        a = b;\n"
    "        b = t;\n"
    "    }\n"
    "    return a;\n"
    "}\n\n"
    "int add(int x, int y) {\n"
    "    return x + y;\n"
    "}\n\n"
    "Q: What is Blackwell PTX?\n"
    "A: Blackwell sm_120 PTX is NVIDIA compute target ISA 8.7 emitted straight from pure C99 without nvcc or CUDA SDK.\n\n";

static uint8_t *load_training_corpus(const char *data_path, size_t *out_len, int raw_bytes_mode) {
    if (!data_path) {
        size_t len = strlen(k_builtin_corpus);
        uint8_t *buf = (uint8_t *)malloc(len + 1U);
        if (!buf) exit(1);
        for (size_t i = 0U; i < len; i++) {
            uint8_t ch = (uint8_t)k_builtin_corpus[i];
            buf[i] = (raw_bytes_mode || ch < 128U) ? ch : (uint8_t)' ';
        }
        *out_len = len;
        return buf;
    }
    FILE *fp = fopen(data_path, "rb");
    if (!fp) {
        fprintf(stderr, "IO_ERROR: cannot open training file '%s'\n", data_path);
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long fsz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    if (fsz < 16L) {
        fclose(fp);
        fprintf(stderr, "INVALID_DATA: training corpus '%s' must be >= 16 bytes\n", data_path);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)fsz);
    if (!buf) { fclose(fp); exit(1); }
    if (fread(buf, 1U, (size_t)fsz, fp) != (size_t)fsz) {
        free(buf); fclose(fp);
        fprintf(stderr, "IO_ERROR: failed reading '%s'\n", data_path);
        return NULL;
    }
    fclose(fp);
    if (!raw_bytes_mode) {
        for (size_t i = 0U; i < (size_t)fsz; i++) {
            if (buf[i] >= 128U) buf[i] = (uint8_t)' ';
        }
    }
    *out_len = (size_t)fsz;
    return buf;
}

typedef struct {
    uint32_t *toks;
    unsigned int len;
    unsigned int prompt_len;
} SftExample;

typedef struct {
    SftExample *items;
    unsigned int count;
} SftDataset;

static void free_sft_dataset(SftDataset *ds) {
    if (!ds) return;
    if (ds->items) {
        for (unsigned int i = 0U; i < ds->count; i++) {
            if (ds->items[i].toks) free(ds->items[i].toks);
        }
        free(ds->items);
        ds->items = NULL;
    }
    ds->count = 0U;
}

static int find_subseq(const uint8_t *buf, size_t len, const char *pat, size_t plen, size_t *out_off) {
    if (!buf || !pat || plen == 0U || len < plen) return 0;
    for (size_t i = 0U; i + plen <= len; i++) {
        if (memcmp(buf + i, pat, plen) == 0) {
            *out_off = i;
            return 1;
        }
    }
    return 0;
}

static unsigned int parse_sft_dataset(
    const QModel *m,
    const uint8_t *corpus,
    size_t corpus_len,
    unsigned int seq_len,
    SftDataset *out
) {
    memset(out, 0, sizeof(*out));
    if (!corpus || corpus_len < 4U) return 0U;
    uint8_t *norm = (uint8_t *)malloc(corpus_len + 2U);
    if (!norm) exit(1);
    size_t nlen = 0U;
    for (size_t i = 0U; i < corpus_len; i++) {
        if (corpus[i] != (uint8_t)'\r') norm[nlen++] = corpus[i];
    }
    norm[nlen] = (uint8_t)'\0';

    unsigned int cap = 0U;
    size_t pos = 0U;
    while (pos < nlen) {
        while (pos < nlen && norm[pos] == (uint8_t)'\n') pos++;
        if (pos >= nlen) break;
        size_t sep = 0U;
        size_t end = find_subseq(norm + pos, nlen - pos, "\n\n", 2U, &sep) ? (pos + sep) : nlen;
        if (end > pos) cap++;
        pos = end + 2U;
    }
    if (cap == 0U) { free(norm); return 0U; }

    out->items = (SftExample *)calloc((size_t)cap, sizeof(SftExample));
    if (!out->items) exit(1);

    pos = 0U;
    while (pos < nlen && out->count < cap) {
        while (pos < nlen && norm[pos] == (uint8_t)'\n') pos++;
        if (pos >= nlen) break;
        size_t sep = 0U;
        size_t end = find_subseq(norm + pos, nlen - pos, "\n\n", 2U, &sep) ? (pos + sep) : nlen;
        while (end > pos && (norm[end - 1U] == (uint8_t)'\n' || norm[end - 1U] == (uint8_t)' ')) end--;
        size_t blk_len = end - pos;
        if (blk_len > 4U) {
            size_t aoff = 0U, sep_len = 0U;
            if (find_subseq(norm + pos, blk_len, "\nA: ", 4U, &aoff)) sep_len = 4U;
            else if (find_subseq(norm + pos, blk_len, "\nA:", 3U, &aoff)) sep_len = 3U;
            if (sep_len > 0U && aoff + sep_len < blk_len) {
                size_t p_bytes = aoff + sep_len;
                size_t a_bytes = blk_len - p_bytes;
                uint8_t *ans_buf = (uint8_t *)malloc(a_bytes + 3U);
                uint32_t *p_ids = (uint32_t *)malloc((p_bytes + 8U) * sizeof(uint32_t));
                uint32_t *a_ids = (uint32_t *)malloc((a_bytes + 8U) * sizeof(uint32_t));
                if (!ans_buf || !p_ids || !a_ids) exit(1);
                memcpy(ans_buf, norm + pos + p_bytes, a_bytes);
                ans_buf[a_bytes] = (uint8_t)'\n';
                ans_buf[a_bytes + 1U] = (uint8_t)'\n';
                ans_buf[a_bytes + 2U] = (uint8_t)'\0';

                size_t p_len = bpe_encode(m, norm + pos, p_bytes, p_ids, p_bytes + 8U);
                size_t a_len = bpe_encode(m, ans_buf, a_bytes + 2U, a_ids, a_bytes + 8U);
                if (p_len > 0U && p_len < (size_t)seq_len && a_len > 0U) {
                    size_t tot = p_len + a_len;
                    if (tot > (size_t)seq_len + 1U) tot = (size_t)seq_len + 1U;
                    uint32_t *toks = (uint32_t *)malloc(tot * sizeof(uint32_t));
                    if (!toks) exit(1);
                    memcpy(toks, p_ids, p_len * sizeof(uint32_t));
                    memcpy(toks + p_len, a_ids, (tot - p_len) * sizeof(uint32_t));
                    out->items[out->count].toks = toks;
                    out->items[out->count].len = (unsigned int)tot;
                    out->items[out->count].prompt_len = (unsigned int)p_len;
                    out->count++;
                }
                free(ans_buf);
                free(p_ids);
                free(a_ids);
            }
        }
        pos = end + 2U;
    }
    free(norm);
    return out->count;
}

static double eval_double_ce_loss(const QModel *m, const QKernels *k, QActArena *a,
                                  const uint32_t *h_targets, unsigned int B, unsigned int T) {
    unsigned int BT = B * T;
    unsigned int V = m->vocab_size;
    run_forward(m, k, a, B, T, 0U, 0);
    float *hlog = (float *)malloc((size_t)BT * (size_t)V * sizeof(float));
    if (!hlog) exit(1);
    check_cu(cuMemcpyDtoH_v2(hlog, a->logits, (size_t)BT * (size_t)V * 4U), "DtoH(grad_logits)");
    double total = 0.0;
    unsigned int act = 0U;
    for (unsigned int bt = 0U; bt < BT; bt++) {
        if (h_targets[bt] >= V) continue;
        const float *row = hlog + (size_t)bt * V;
        double mx = (double)row[0];
        for (unsigned int v = 1U; v < V; v++) if ((double)row[v] > mx) mx = (double)row[v];
        double sm = 0.0;
        for (unsigned int v = 0U; v < V; v++) sm += exp((double)row[v] - mx);
        total += log(sm) - ((double)row[h_targets[bt]] - mx);
        act++;
    }
    free(hlog);
    return (act > 0U) ? (total / (double)act) : 0.0;
}

static int run_gradient_check(QModel *m, const QKernels *k, int sft_mask_check) {
    unsigned int B = 2U, T = 8U, BT = B * T;
    QActArena a;
    alloc_act_arena(m, &a, BT, B * m->n_heads * T * T, 1);

    uint32_t *h_tok = (uint32_t *)malloc((size_t)BT * 4U);
    uint32_t *h_tgt = (uint32_t *)malloc((size_t)BT * 4U);
    float *h_grad = (float *)malloc((size_t)m->total_params * 4U);
    if (!h_tok || !h_tgt || !h_grad) exit(1);

    const char *sample = "int fib(int n) { return a + b; }";
    size_t slen = strlen(sample);
    unsigned int active_cnt = 0U;
    for (unsigned int i = 0U; i < BT; i++) {
        h_tok[i] = (uint32_t)((uint8_t)sample[i % slen] & 127U);
        if (sft_mask_check && (i % T) < (T / 2U)) {
            h_tgt[i] = 0xFFFFFFFFU;
        } else {
            h_tgt[i] = (uint32_t)((uint8_t)sample[(i + 1U) % slen] & 127U);
            active_cnt++;
        }
    }
    check_cu(cuMemcpyHtoD_v2(a.d_tokens, h_tok, (size_t)BT * 4U), "HtoD(gc_tok)");
    check_cu(cuMemcpyHtoD_v2(a.d_targets, h_tgt, (size_t)BT * 4U), "HtoD(gc_tgt)");

    run_forward(m, k, &a, B, T, 0U, 0);
    run_loss_and_backward(m, k, &a, B, T, 1, 0, sft_mask_check ? active_cnt : 0U);
    check_cu(cuCtxSynchronize(), "gc_sync");
    check_cu(cuMemcpyDtoH_v2(h_grad, m->d_grads, (size_t)m->total_params * 4U), "DtoH(gc_grads)");

    unsigned int probes[40];
    const char *pnames[40];
    int n_probes = 0;

    unsigned int probe_pos = sft_mask_check ? (T - 1U) : 3U;
    unsigned int tok_used = h_tok[probe_pos];
    unsigned int tgt_used = h_tgt[probe_pos];
    probes[n_probes] = m->off_E_re + tok_used * m->dim + 5U; pnames[n_probes++] = "E_re";
    probes[n_probes] = m->off_E_im + tok_used * m->dim + 9U; pnames[n_probes++] = "E_im";

    for (unsigned int li = 0U; li < 2U && li < m->n_layers; li++) {
        unsigned int l = (li == 0U) ? 0U : (m->n_layers - 1U);
        const QLayerOff *lo = &m->layers[l];
        unsigned int nq = m->n_qubits;
        probes[n_probes] = lo->off_theta_q + 1U;             pnames[n_probes++] = "theta_q.ry";
        probes[n_probes] = lo->off_theta_q + nq + 2U;        pnames[n_probes++] = "theta_q.rz";
        probes[n_probes] = lo->off_theta_q + 2U * nq + 3U;   pnames[n_probes++] = "theta_q.cp";
        probes[n_probes] = lo->off_theta_k + 0U;             pnames[n_probes++] = "theta_k.ry";
        probes[n_probes] = lo->off_theta_v + 2U * nq + 1U;   pnames[n_probes++] = "theta_v.cp";
        probes[n_probes] = lo->off_theta_f1 + nq + 4U;       pnames[n_probes++] = "theta_f1.rz";
        probes[n_probes] = lo->off_theta_f2 + 3U * nq + 2U;  pnames[n_probes++] = "theta_f2.ry";
        if (m->n_registers > 1U && li == 0U) {
            unsigned int phi_off = m->n_registers * m->pqc_depth * 3U * nq;
            probes[n_probes] = lo->off_theta_q + phi_off;  pnames[n_probes++] = "theta_q.phi";
            probes[n_probes] = lo->off_theta_f1 + phi_off; pnames[n_probes++] = "theta_f1.phi";
        }
        probes[n_probes] = lo->off_beta + 1U;                pnames[n_probes++] = "beta";
        probes[n_probes] = lo->off_gamma + 2U;               pnames[n_probes++] = "gamma";
        probes[n_probes] = lo->off_kerr_k + 7U;              pnames[n_probes++] = "kerr_k";
        probes[n_probes] = lo->off_kerr_b + 11U;             pnames[n_probes++] = "kerr_b";
        probes[n_probes] = lo->off_w1 + 3U * (2U * m->dim) + 7U; pnames[n_probes++] = "W1";
        probes[n_probes] = lo->off_w2 + 5U * m->h_ff + 4U;   pnames[n_probes++] = "W2";
    }
    probes[n_probes] = m->off_H_out + tgt_used * (2U * m->dim) + 3U; pnames[n_probes++] = "H_out";
    probes[n_probes] = m->off_b_out + tgt_used;                       pnames[n_probes++] = "b_out";

    double eps = 2e-3;
    double max_rel = 0.0;
    double max_abs = 0.0;
    for (int pi = 0; pi < n_probes; pi++) {
        unsigned int idx = probes[pi];
        float orig = m->h_weights[idx];
        float vp = orig + (float)eps;
        float vm = orig - (float)eps;
        check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, idx), &vp, 4U), "gc_p");
        double lp = eval_double_ce_loss(m, k, &a, h_tgt, B, T);
        check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, idx), &vm, 4U), "gc_m");
        double lm = eval_double_ce_loss(m, k, &a, h_tgt, B, T);
        check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, idx), &orig, 4U), "gc_restore");

        double num = (lp - lm) / (2.0 * eps);
        double ana = (double)h_grad[idx];
        double diff = fabs(num - ana);
        double denom = fabs(num) + fabs(ana);
        if (denom < 1e-3) denom = 1e-3;
        double rel = diff / denom;
        if (rel > max_rel) max_rel = rel;
        if (diff > max_abs) max_abs = diff;
        if (rel > 2e-2 && diff > 2e-4) {
            fprintf(stderr, "GRADCHECK_FAIL probe=%s idx=%u ana=%.8f num=%.8f rel=%.6f diff=%.6f\n",
                    pnames[pi], idx, ana, num, rel, diff);
            free(h_tok); free(h_tgt); free(h_grad);
            free_act_arena(m, &a);
            return 1;
        }
    }

    /* Exact Quantum Parameter-Shift Verification on Hermitian Observable E(theta) = <psi(theta)|Z_0|psi(theta)> */
    unsigned int D = m->dim;
    unsigned int nq = m->n_qubits;
    unsigned int pd = m->pqc_depth;
    unsigned int n_reg = (m->n_registers > 0U) ? m->n_registers : 1U;
    float *h_in_r = (float *)calloc(D, 4U);
    float *h_in_i = (float *)calloc(D, 4U);
    float *h_out_r = (float *)malloc(D * 4U);
    float *h_out_i = (float *)malloc(D * 4U);
    float *h_dr = (float *)malloc(D * 4U);
    float *h_di = (float *)malloc(D * 4U);
    float *h_ptok = (float *)malloc(m->n_pqc_params * 4U);
    uint64_t din_r = qmalloc_f32(D, "ps_ir"), din_i = qmalloc_f32(D, "ps_ii");
    uint64_t dout_r = qmalloc_f32(D, "ps_or"), dout_i = qmalloc_f32(D, "ps_oi");
    double invn = 0.0;
    for (unsigned int i = 0U; i < D; i++) {
        h_in_r[i] = sin((double)i * 0.37 + 0.1);
        h_in_i[i] = cos((double)i * 0.23 - 0.2);
        invn += (double)h_in_r[i] * h_in_r[i] + (double)h_in_i[i] * h_in_i[i];
    }
    invn = 1.0 / sqrt(invn);
    for (unsigned int i = 0U; i < D; i++) { h_in_r[i] = (float)(h_in_r[i] * invn); h_in_i[i] = (float)(h_in_i[i] * invn); }
    check_cu(cuMemcpyHtoD_v2(din_r, h_in_r, D * 4U), "ps_in_r");
    check_cu(cuMemcpyHtoD_v2(din_i, h_in_i, D * 4U), "ps_in_i");

    unsigned int off_th = m->layers[0].off_theta_q;
    uint64_t d_th = dp_off(m->d_weights, off_th);
    launch_pqc_fwd(m, k, din_r, din_i, dout_r, dout_i, d_th, 1U, "ps_fwd0");
    check_cu(cuMemcpyDtoH_v2(h_out_r, dout_r, D * 4U), "ps_dor");
    check_cu(cuMemcpyDtoH_v2(h_out_i, dout_i, D * 4U), "ps_doi");
    for (unsigned int i = 0U; i < D; i++) {
        double z0 = ((i & 1U) == 0U) ? 1.0 : -1.0;
        h_dr[i] = (float)(2.0 * z0 * (double)h_out_r[i]);
        h_di[i] = (float)(2.0 * z0 * (double)h_out_i[i]);
    }
    check_cu(cuMemcpyHtoD_v2(a.d_tmp1_re, h_dr, D * 4U), "ps_dr");
    check_cu(cuMemcpyHtoD_v2(a.d_tmp1_im, h_di, D * 4U), "ps_di");
    launch_pqc_bwd(m, k, &a, dout_r, dout_i, a.d_tmp1_re, a.d_tmp1_im, a.d_nxt_re, a.d_nxt_im, d_th, 1U, "ps_bwd0");
    check_cu(cuMemcpyDtoH_v2(h_ptok, a.d_pqc_tok, m->n_pqc_params * 4U), "ps_ptok");

    double ps_max_err = 0.0;
    double half_pi = 1.5707963267948966;
    for (unsigned int r = 0U; r < n_reg; r++) {
        for (unsigned int p = 0U; p < pd; p++) {
            for (int typ = 0; typ < 2; typ++) { /* RY and RZ exact parameter-shift */
                for (unsigned int q = 0U; q < nq; q++) {
                    unsigned int pidx = r * (pd * 3U * nq) + p * (3U * nq) + (unsigned int)typ * nq + q;
                    float orig = m->h_weights[off_th + pidx];
                    float vp = orig + (float)half_pi;
                    float vm = orig - (float)half_pi;

                    check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, off_th + pidx), &vp, 4U), "ps_vp");
                    launch_pqc_fwd(m, k, din_r, din_i, dout_r, dout_i, d_th, 1U, "ps_fwdp");
                    check_cu(cuMemcpyDtoH_v2(h_out_r, dout_r, D * 4U), "ps_p_r");
                    check_cu(cuMemcpyDtoH_v2(h_out_i, dout_i, D * 4U), "ps_p_i");
                    double ep = 0.0;
                    for (unsigned int i = 0U; i < D; i++) {
                        double z0 = ((i & 1U) == 0U) ? 1.0 : -1.0;
                        ep += z0 * ((double)h_out_r[i] * h_out_r[i] + (double)h_out_i[i] * h_out_i[i]);
                    }

                    check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, off_th + pidx), &vm, 4U), "ps_vm");
                    launch_pqc_fwd(m, k, din_r, din_i, dout_r, dout_i, d_th, 1U, "ps_fwdm");
                    check_cu(cuMemcpyDtoH_v2(h_out_r, dout_r, D * 4U), "ps_m_r");
                    check_cu(cuMemcpyDtoH_v2(h_out_i, dout_i, D * 4U), "ps_m_i");
                    double em = 0.0;
                    for (unsigned int i = 0U; i < D; i++) {
                        double z0 = ((i & 1U) == 0U) ? 1.0 : -1.0;
                        em += z0 * ((double)h_out_r[i] * h_out_r[i] + (double)h_out_i[i] * h_out_i[i]);
                    }

                    check_cu(cuMemcpyHtoD_v2(dp_off(m->d_weights, off_th + pidx), &orig, 4U), "ps_rest");
                    double ps_grad = 0.5 * (ep - em);
                    double err = fabs(ps_grad - (double)h_ptok[pidx]);
                    if (err > ps_max_err) ps_max_err = err;
                    if (err > 1e-4) {
                        fprintf(stderr, "PARAM_SHIFT_FAIL pidx=%u ana=%.7f shift=%.7f err=%.7f\n",
                                pidx, (double)h_ptok[pidx], ps_grad, err);
                        free(h_in_r); free(h_in_i); free(h_out_r); free(h_out_i);
                        free(h_dr); free(h_di); free(h_ptok);
                        cuMemFree_v2(din_r); cuMemFree_v2(din_i); cuMemFree_v2(dout_r); cuMemFree_v2(dout_i);
                        free(h_tok); free(h_tgt); free(h_grad);
                        free_act_arena(m, &a);
                        return 1;
                    }
                }
            }
        }
    }

    printf("QLLM_GRADCHECK_OK probes=%d max_rel_err=%.6f max_abs_err=%.6f param_shift_max_err=%.7f\n",
           n_probes, max_rel, max_abs, ps_max_err);
    fflush(stdout);

    free(h_in_r); free(h_in_i); free(h_out_r); free(h_out_i);
    free(h_dr); free(h_di); free(h_ptok);
    cuMemFree_v2(din_r); cuMemFree_v2(din_i); cuMemFree_v2(dout_r); cuMemFree_v2(dout_i);
    free(h_tok); free(h_tgt); free(h_grad);
    free_act_arena(m, &a);
    return 0;
}

static uint32_t sample_next_char(
    const float *logits,
    unsigned int V,
    float temp,
    int top_k,
    float top_p,
    float rep_penalty,
    const uint32_t *recent_ctx,
    unsigned int recent_len,
    uint64_t *rng_state
) {
    float *adj = (float *)malloc((size_t)V * sizeof(float));
    if (!adj) exit(1);
    memcpy(adj, logits, (size_t)V * sizeof(float));
    if (rep_penalty > 1.0f && recent_ctx && recent_len > 0U) {
        unsigned int win = (recent_len < 64U) ? recent_len : 64U;
        const uint32_t *tail = recent_ctx + (recent_len - win);
        for (unsigned int v = 0U; v < V; v++) {
            int seen = 0;
            for (unsigned int r = 0U; r < win; r++) {
                if (tail[r] == v) { seen = 1; break; }
            }
            if (seen) {
                adj[v] = (adj[v] > 0.0f) ? (adj[v] / rep_penalty) : (adj[v] * rep_penalty);
            }
        }
    }
    if (temp <= 0.0f) {
        uint32_t best = 0U;
        float bval = adj[0];
        for (unsigned int v = 1U; v < V; v++) {
            if (adj[v] > bval) { bval = adj[v]; best = v; }
        }
        free(adj);
        return best;
    }
    double *probs = (double *)malloc((size_t)V * sizeof(double));
    unsigned int *idx = (unsigned int *)malloc((size_t)V * sizeof(unsigned int));
    if (!probs || !idx) exit(1);
    double maxv = (double)adj[0];
    for (unsigned int v = 0U; v < V; v++) {
        idx[v] = v;
        if ((double)adj[v] > maxv) maxv = (double)adj[v];
    }
    unsigned int n_cand = V;
    if (top_k > 0 || top_p < 1.0f) {
        unsigned int sort_k = (top_k > 0 && (unsigned int)top_k < V) ? (unsigned int)top_k : ((V < 128U) ? V : 128U);
        for (unsigned int i = 0U; i < sort_k; i++) {
            unsigned int bi = i;
            for (unsigned int j = i + 1U; j < V; j++) {
                if (adj[idx[j]] > adj[idx[bi]]) bi = j;
            }
            unsigned int tmp = idx[i]; idx[i] = idx[bi]; idx[bi] = tmp;
        }
        n_cand = sort_k;
    }
    double sum = 0.0;
    for (unsigned int i = 0U; i < n_cand; i++) {
        double p = exp(((double)adj[idx[i]] - maxv) / (double)temp);
        probs[i] = p;
        sum += p;
    }
    if (top_p < 1.0f && sum > 0.0) {
        double cdf_p = 0.0;
        unsigned int cut = n_cand;
        for (unsigned int i = 0U; i < n_cand; i++) {
            cdf_p += probs[i] / sum;
            if (cdf_p >= (double)top_p) { cut = i + 1U; break; }
        }
        if (cut < 1U) cut = 1U;
        n_cand = cut;
        sum = 0.0;
        for (unsigned int i = 0U; i < n_cand; i++) sum += probs[i];
    }
    *rng_state = (*rng_state) * 6364136223846793005ULL + 1442695040888963407ULL;
    double u = ((double)((*rng_state) >> 11) * (1.0 / 9007199254740992.0)) * sum;
    double cdf = 0.0;
    uint32_t chosen = idx[n_cand - 1U];
    for (unsigned int i = 0U; i < n_cand; i++) {
        cdf += probs[i];
        if (cdf >= u) { chosen = idx[i]; break; }
    }
    free(adj);
    free(probs);
    free(idx);
    return chosen;
}

static void print_escaped(const char *s, size_t n) {
    for (size_t i = 0U; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\t') printf("\\t");
        else if (c == '"') printf("\\\"");
        else if (c == '\\') printf("\\\\");
        else if (c >= 32U && c < 127U) putchar((int)c);
        else printf("\\x%02x", (unsigned int)c);
    }
}

static void generate_sequence(
    const QModel *m,
    const QKernels *k,
    QActArena *a,
    uint32_t *ctx,
    unsigned int *io_len,
    unsigned int max_ctx,
    int use_kv_cache,
    int steps,
    float temp,
    int top_k,
    float top_p,
    float rep_penalty,
    uint64_t *rng,
    int stream_out,
    const char *stream_prefix,
    char *out_buf,
    size_t out_cap,
    uint32_t *out_ids,
    int *out_gen_count,
    const char *dump_logits_path,
    const char *dump_state_path,
    int stop_on_double_nl,
    float *out_tok_per_sec,
    uint32_t *out_top1,
    float *out_top1_logit,
    uint32_t *out_fnv
) {
    float *h_logits = (float *)malloc((size_t)m->vocab_size * 4U);
    if (!h_logits) exit(1);
    if (out_cap > 0U) out_buf[0] = '\0';
    size_t out_len = 0U;
    int gen_cnt = 0;

    void *ev0 = NULL, *ev1 = NULL;
    check_cu(cuEventCreate(&ev0, 0U), "ev0");
    check_cu(cuEventCreate(&ev1, 0U), "ev1");
    check_cu(cuEventRecord(ev0, NULL), "ev0_rec");

    int kv_on = (use_kv_cache && m->n_layers > 0U && a->layers[0].kv_k_re != 0ULL && a->kv_scores != 0ULL);
    int printed_prefix = 0;
    for (int st = 0; ; st++) {
        if (*io_len == 0U) {
            ctx[0] = (uint32_t)' ';
            *io_len = 1U;
        }
        int slid = 0;
        if (*io_len >= max_ctx) {
            unsigned int keep = (max_ctx * 3U) / 4U;
            unsigned int drop = *io_len - keep;
            memmove(ctx, ctx + drop, (size_t)keep * 4U);
            *io_len = keep;
            slid = 1;
        }
        unsigned int cur_T = *io_len;
        unsigned int state_pos = cur_T - 1U;
        if (kv_on) {
            if (st == 0 || slid) {
                run_prefill_kv(m, k, a, ctx, cur_T);
                uint64_t last_off = dp_off(a->logits, (cur_T - 1U) * m->vocab_size);
                check_cu(cuMemcpyDtoH_v2(h_logits, last_off, (size_t)m->vocab_size * 4U), "DtoH(gen_logits_kvp)");
            } else {
                unsigned int pos = cur_T - 1U;
                run_decode_step_kv(m, k, a, ctx[pos], pos);
                check_cu(cuMemcpyDtoH_v2(h_logits, a->logits, (size_t)m->vocab_size * 4U), "DtoH(gen_logits_kvd)");
                state_pos = 0U;
            }
        } else {
            check_cu(cuMemcpyHtoD_v2(a->d_tokens, ctx, (size_t)cur_T * 4U), "HtoD(gen_ctx)");
            run_forward(m, k, a, 1U, cur_T, 0U, 0);
            uint64_t last_off = dp_off(a->logits, (cur_T - 1U) * m->vocab_size);
            check_cu(cuMemcpyDtoH_v2(h_logits, last_off, (size_t)m->vocab_size * 4U), "DtoH(gen_logits)");
        }

        if (st == 0) {
            uint32_t b1 = 0U; float bv = h_logits[0];
            for (unsigned int v = 1U; v < m->vocab_size; v++) {
                if (h_logits[v] > bv) { bv = h_logits[v]; b1 = v; }
            }
            if (out_top1) *out_top1 = b1;
            if (out_top1_logit) *out_top1_logit = bv;
            if (out_fnv) *out_fnv = (uint32_t)fnv1a_bytes64(h_logits, (size_t)m->vocab_size * 4U);

            if (dump_logits_path) {
                FILE *fl = fopen(dump_logits_path, "wb");
                if (!fl) { fprintf(stderr, "IO_ERROR: cannot write logits '%s'\n", dump_logits_path); exit(1); }
                fwrite(h_logits, 4U, m->vocab_size, fl);
                fclose(fl);
            }
            if (dump_state_path) {
                size_t dsz = (size_t)m->dim * 4U;
                size_t qsz = dsz * 2U;
                float *hs = (float *)malloc(qsz);
                if (!hs) exit(1);
                const QLayerAct *last = &a->layers[m->n_layers - 1U];
                check_cu(cuMemcpyDtoH_v2(hs, dp_off(last->psi_out_re, state_pos * m->dim), dsz), "DtoH(state_re)");
                check_cu(cuMemcpyDtoH_v2(hs + m->dim, dp_off(last->psi_out_im, state_pos * m->dim), dsz), "DtoH(state_im)");
                FILE *fs = fopen(dump_state_path, "wb");
                if (!fs) { fprintf(stderr, "IO_ERROR: cannot write state '%s'\n", dump_state_path); exit(1); }
                fwrite(hs, 1U, qsz, fs);
                fclose(fs);
                free(hs);
            }
        }
        if (st >= steps) break;

        uint32_t nxt = sample_next_char(h_logits, m->vocab_size, temp, top_k, top_p, rep_penalty, ctx, *io_len, rng);
        ctx[*io_len] = nxt;
        (*io_len)++;
        if (out_ids) out_ids[gen_cnt] = nxt;
        gen_cnt++;

        uint8_t single_b = (uint8_t)nxt;
        const uint8_t *piece = &single_b;
        size_t piece_len = 1U;
        if (m->vocab_size >= 256U && m->tok_pool && nxt < m->vocab_size) {
            piece = m->tok_pool + m->tok_off[nxt];
            piece_len = (size_t)m->tok_len[nxt];
        }
        for (size_t pi = 0U; pi < piece_len; pi++) {
            if (out_len + 1U < out_cap) {
                out_buf[out_len++] = (char)piece[pi];
                out_buf[out_len] = '\0';
            }
        }
        if (stream_out) {
            if (!printed_prefix && stream_prefix) {
                printf("%s", stream_prefix);
                printed_prefix = 1;
            }
            fwrite(piece, 1U, piece_len, stdout);
            fflush(stdout);
        }
        if (stop_on_double_nl && out_len >= 2U && out_buf[out_len - 1U] == '\n' && out_buf[out_len - 2U] == '\n') {
            break;
        }
    }
    if (stream_out && printed_prefix) { putchar('\n'); fflush(stdout); }

    check_cu(cuEventRecord(ev1, NULL), "ev1_rec");
    check_cu(cuEventSynchronize(ev1), "ev1_sync");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, ev0, ev1), "ev_ms");
    cuEventDestroy_v2(ev0);
    cuEventDestroy_v2(ev1);
    if (out_tok_per_sec) {
        *out_tok_per_sec = (ms > 0.0f && gen_cnt > 0) ? ((float)gen_cnt * 1000.0f / ms) : 0.0f;
    }
    if (out_gen_count) *out_gen_count = gen_cnt;
    free(h_logits);
}

static void fprint_json_bytes(FILE *fp, const uint8_t *s, size_t n) {
    fputc('"', fp);
    for (size_t i = 0U; i < n; i++) {
        uint8_t c = s[i];
        if (c == (uint8_t)'"') fprintf(fp, "\\\"");
        else if (c == (uint8_t)'\\') fprintf(fp, "\\\\");
        else if (c == (uint8_t)'\n') fprintf(fp, "\\n");
        else if (c == (uint8_t)'\r') fprintf(fp, "\\r");
        else if (c == (uint8_t)'\t') fprintf(fp, "\\t");
        else if (c < 0x20U || c >= 0x7FU) fprintf(fp, "\\u%04x", (unsigned int)c);
        else fputc((int)c, fp);
    }
    fputc('"', fp);
}

static void fprint_farray(FILE *fp, const float *arr, unsigned int n) {
    fputc('[', fp);
    for (unsigned int i = 0U; i < n; i++) {
        fprintf(fp, "%s%.6f", (i > 0U) ? "," : "", (double)arr[i]);
    }
    fputc(']', fp);
}

static int dump_telemetry_json(
    QModel *m,
    const QKernels *k,
    QActArena *a,
    const uint32_t *ctx,
    unsigned int T,
    const char *prompt,
    const char *generated,
    float tok_per_sec,
    uint32_t logits_fnv,
    uint32_t top1_id,
    const char *dev_name,
    size_t vram_bytes,
    int drv_ver,
    unsigned int ptx_bytes,
    const char *path
) {
    if (T == 0U) return -1;
    unsigned int D = m->dim;
    unsigned int V = m->vocab_size;
    unsigned int H = m->n_heads;

    size_t w_sz = (size_t)m->total_params * sizeof(float);
    if (m->d_weights) check_cu(cuMemcpyDtoH_v2(m->h_weights, m->d_weights, w_sz), "DtoH(tel_w)");

    check_cu(cuMemcpyHtoD_v2(a->d_tokens, ctx, (size_t)T * 4U), "HtoD(tel_tok)");
    run_forward(m, k, a, 1U, T, 0U, 0);
    check_cu(cuCtxSynchronize(), "tel_sync");

    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "IO_ERROR: cannot write telemetry JSON '%s'\n", path);
        return -1;
    }

    float *re = (float *)malloc((size_t)D * 4U);
    float *im = (float *)malloc((size_t)D * 4U);
    float *att = (float *)malloc((size_t)H * (size_t)T * (size_t)T * 4U);
    float *h_logits = (float *)malloc((size_t)V * 4U);
    double *l_probs = (double *)malloc((size_t)V * sizeof(double));
    unsigned int *top_idx = (unsigned int *)malloc((size_t)V * sizeof(unsigned int));
    if (!re || !im || !att || !h_logits || !l_probs || !top_idx) exit(1);

    check_cu(cuMemcpyDtoH_v2(h_logits, dp_off(a->logits, (T - 1U) * V), (size_t)V * 4U), "DtoH(tel_log)");

    fprintf(fp, "{\n  \"device\": {\"name\": ");
    fprint_json_bytes(fp, (const uint8_t *)dev_name, strlen(dev_name));
    fprintf(fp, ", \"vram_mib\": %u, \"driver\": %d, \"ptx_bytes\": %u},\n",
            (unsigned int)(vram_bytes >> 20), drv_ver, ptx_bytes);

    unsigned int n_reg = (m->n_registers > 0U) ? m->n_registers : 1U;
    unsigned int reg_dim = (m->reg_dim > 0U) ? m->reg_dim : D;
    fprintf(fp, "  \"model\": {\"arch\": \"zkaedi_hybrid_qtransformer\", \"version\": %u, \"vocab_size\": %u, \"n_merges\": %u, \"qubits\": %u, \"registers\": %u, \"logical_qubits\": %u, \"dim\": %u, \"heads\": %u, \"layers\": %u, \"pqc_depth\": %u, \"h_ff\": %u, \"params\": %u},\n",
            m->version, m->vocab_size, m->n_merges, m->n_qubits, n_reg, n_reg * m->n_qubits, m->dim, m->n_heads, m->n_layers, m->pqc_depth, m->h_ff, m->total_params);

    fprintf(fp, "  \"prompt\": ");
    fprint_json_bytes(fp, (const uint8_t *)prompt, strlen(prompt));
    fprintf(fp, ",\n  \"generated\": ");
    fprint_json_bytes(fp, (const uint8_t *)generated, strlen(generated));
    fprintf(fp, ",\n  \"tok_per_sec\": %.2f,\n  \"logits_fnv\": \"%08x\",\n  \"top1_id\": %u,\n",
            (double)tok_per_sec, logits_fnv, top1_id);

    fprintf(fp, "  \"tokens\": [\n");
    for (unsigned int t = 0U; t < T; t++) {
        uint32_t tid = ctx[t];
        uint8_t sb = (uint8_t)tid;
        const uint8_t *tp = &sb;
        size_t tl = 1U;
        if (m->vocab_size >= 256U && m->tok_pool && tid < V) {
            tp = m->tok_pool + m->tok_off[tid];
            tl = (size_t)m->tok_len[tid];
        }
        fprintf(fp, "    {\"pos\": %u, \"id\": %u, \"text\": ", t, tid);
        fprint_json_bytes(fp, tp, tl);
        fprintf(fp, "}%s\n", (t + 1U < T) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"layers\": [\n");
    for (unsigned int st = 0U; st <= m->n_layers; st++) {
        if (st == 0U) {
            check_cu(cuMemcpyDtoH_v2(re, dp_off(a->psi0_re, (T - 1U) * D), (size_t)D * 4U), "DtoH(tel_p0r)");
            check_cu(cuMemcpyDtoH_v2(im, dp_off(a->psi0_im, (T - 1U) * D), (size_t)D * 4U), "DtoH(tel_p0i)");
        } else {
            const QLayerAct *la = &a->layers[st - 1U];
            check_cu(cuMemcpyDtoH_v2(re, dp_off(la->psi_out_re, (T - 1U) * D), (size_t)D * 4U), "DtoH(tel_pr)");
            check_cu(cuMemcpyDtoH_v2(im, dp_off(la->psi_out_im, (T - 1U) * D), (size_t)D * 4U), "DtoH(tel_pi)");
        }
        double nerr = 0.0, s2 = 0.0, sl = 0.0;
        double reg_w[8];
        for (unsigned int r = 0U; r < 8U; r++) reg_w[r] = 0.0;
        if (n_reg <= 1U) {
            compute_state_entropy(re, im, m->n_qubits, &nerr, &s2, &sl);
            reg_w[0] = 1.0;
        } else {
            double n2_tot = 0.0;
            for (unsigned int i = 0U; i < D; i++) {
                n2_tot += (double)re[i] * (double)re[i] + (double)im[i] * (double)im[i];
            }
            nerr = fabs(n2_tot - 1.0);
            for (unsigned int r = 0U; r < n_reg && r < 8U; r++) {
                const float *rr = re + (size_t)r * reg_dim;
                const float *ri = im + (size_t)r * reg_dim;
                double wr = 0.0;
                for (unsigned int j = 0U; j < reg_dim; j++) {
                    wr += (double)rr[j] * (double)rr[j] + (double)ri[j] * (double)ri[j];
                }
                double w = (n2_tot > 1e-12) ? (wr / n2_tot) : (1.0 / (double)n_reg);
                reg_w[r] = w;
                if (wr > 1e-12 && n2_tot > 1e-12) {
                    double s2_r = 0.0, sl_r = 0.0;
                    compute_state_entropy(rr, ri, m->n_qubits, NULL, &s2_r, &sl_r);
                    s2 += w * s2_r;
                    sl += w * sl_r;
                }
            }
        }

        fprintf(fp, "    {\"index\": %u, \"name\": ", st);
        if (st == 0U) fprintf(fp, "\"Embed + Phase-RoPE\"");
        else fprintf(fp, "\"Layer %u\"", st - 1U);
        fprintf(fp, ", \"norm_err\": %.8f, \"renyi2_bits\": %.6f, \"linear_entropy\": %.6f",
                nerr, s2, sl);
        if (n_reg > 1U) {
            fprintf(fp, ", \"reg_weights\": [");
            for (unsigned int r = 0U; r < n_reg && r < 8U; r++) {
                fprintf(fp, "%s%.6f", (r > 0U) ? "," : "", reg_w[r]);
            }
            fprintf(fp, "]");
        }
        fprintf(fp, ",\n      \"bloch\": [");

        double r0_norm = 0.0;
        for (unsigned int i = 0U; i < reg_dim; i++) {
            r0_norm += (double)re[i] * (double)re[i] + (double)im[i] * (double)im[i];
        }
        double inv_r0 = (r0_norm > 1e-12) ? (1.0 / r0_norm) : 0.0;
        for (unsigned int q = 0U; q < m->n_qubits; q++) {
            unsigned int bit = 1U << q;
            double bx = 0.0, by = 0.0, p0 = 0.0, p1 = 0.0;
            for (unsigned int i = 0U; i < reg_dim; i++) {
                if ((i & bit) == 0U) {
                    unsigned int j = i | bit;
                    double r0 = (double)re[i], i0 = (double)im[i];
                    double r1 = (double)re[j], i1 = (double)im[j];
                    p0 += r0 * r0 + i0 * i0;
                    p1 += r1 * r1 + i1 * i1;
                    bx += 2.0 * (r0 * r1 + i0 * i1);
                    by += 2.0 * (r0 * i1 - i0 * r1);
                }
            }
            p0 *= inv_r0;
            p1 *= inv_r0;
            bx *= inv_r0;
            by *= inv_r0;
            double bz = p0 - p1;
            double r = sqrt(bx * bx + by * by + bz * bz);
            double purity = 0.5 * (1.0 + r * r);
            double l1 = 0.5 * (1.0 + r); if (l1 < 1e-12) l1 = 1e-12; if (l1 > 1.0) l1 = 1.0;
            double l2 = 0.5 * (1.0 - r); if (l2 < 1e-12) l2 = 1e-12; if (l2 > 1.0) l2 = 1.0;
            double sv = -(l1 * log(l1) + l2 * log(l2)) * 1.4426950408889634;
            fprintf(fp, "{\"q\":%u,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"r\":%.6f,\"purity\":%.6f,\"vn_entropy\":%.6f}%s",
                    q, bx, by, bz, r, purity, sv, (q + 1U < m->n_qubits) ? "," : "");
        }
        fprintf(fp, "]");

        if (st > 0U) {
            const QLayerOff *lo = &m->layers[st - 1U];
            const QLayerAct *la = &a->layers[st - 1U];
            check_cu(cuMemcpyDtoH_v2(att, la->att_w, (size_t)H * (size_t)T * (size_t)T * 4U), "DtoH(tel_att)");

            fprintf(fp, ",\n      \"beta\": ");
            fprint_farray(fp, m->h_weights + lo->off_beta, H);
            fprintf(fp, ",\n      \"gamma\": ");
            fprint_farray(fp, m->h_weights + lo->off_gamma, H);
            fprintf(fp, ",\n      \"attn_heads\": [");
            for (unsigned int h = 0U; h < H; h++) {
                fprintf(fp, "[");
                for (unsigned int tr = 0U; tr < T; tr++) {
                    fprintf(fp, "[");
                    for (unsigned int ts = 0U; ts < T; ts++) {
                        float val = (ts <= tr) ? att[((size_t)h * T + tr) * T + ts] : 0.0f;
                        fprintf(fp, "%s%.5f", (ts > 0U) ? "," : "", (double)val);
                    }
                    fprintf(fp, "]%s", (tr + 1U < T) ? "," : "");
                }
                fprintf(fp, "]%s", (h + 1U < H) ? "," : "");
            }
            fprintf(fp, "],\n      \"attn_mean\": [");
            for (unsigned int tr = 0U; tr < T; tr++) {
                fprintf(fp, "[");
                for (unsigned int ts = 0U; ts < T; ts++) {
                    double acc = 0.0;
                    if (ts <= tr) {
                        for (unsigned int h = 0U; h < H; h++) acc += (double)att[((size_t)h * T + tr) * T + ts];
                        acc /= (double)H;
                    }
                    fprintf(fp, "%s%.5f", (ts > 0U) ? "," : "", acc);
                }
                fprintf(fp, "]%s", (tr + 1U < T) ? "," : "");
            }
            fprintf(fp, "],\n      \"pqc\": {\"theta_q\": ");
            fprint_farray(fp, m->h_weights + lo->off_theta_q, m->n_pqc_params);
            fprintf(fp, ", \"theta_k\": ");
            fprint_farray(fp, m->h_weights + lo->off_theta_k, m->n_pqc_params);
            fprintf(fp, ", \"theta_v\": ");
            fprint_farray(fp, m->h_weights + lo->off_theta_v, m->n_pqc_params);
            fprintf(fp, ", \"theta_f1\": ");
            fprint_farray(fp, m->h_weights + lo->off_theta_f1, m->n_pqc_params);
            fprintf(fp, ", \"theta_f2\": ");
            fprint_farray(fp, m->h_weights + lo->off_theta_f2, m->n_pqc_params);
            fprintf(fp, "}");
        }
        fprintf(fp, "}%s\n", (st < m->n_layers) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"final_state\": {\n    \"re\": ");
    fprint_farray(fp, re, D);
    fprintf(fp, ",\n    \"im\": ");
    fprint_farray(fp, im, D);
    fprintf(fp, ",\n    \"prob\": [");
    for (unsigned int i = 0U; i < D; i++) {
        double p = (double)re[i] * (double)re[i] + (double)im[i] * (double)im[i];
        fprintf(fp, "%s%.7f", (i > 0U) ? "," : "", p);
    }
    fprintf(fp, "],\n    \"phase\": [");
    for (unsigned int i = 0U; i < D; i++) {
        double ph = atan2((double)im[i], (double)re[i]);
        fprintf(fp, "%s%.6f", (i > 0U) ? "," : "", ph);
    }
    fprintf(fp, "]\n  },\n");

    double maxv = (double)h_logits[0];
    for (unsigned int v = 0U; v < V; v++) {
        top_idx[v] = v;
        if ((double)h_logits[v] > maxv) maxv = (double)h_logits[v];
    }
    double psum = 0.0;
    for (unsigned int v = 0U; v < V; v++) {
        l_probs[v] = exp((double)h_logits[v] - maxv);
        psum += l_probs[v];
    }
    if (psum > 0.0) {
        for (unsigned int v = 0U; v < V; v++) l_probs[v] /= psum;
    }
    unsigned int ktop = (V < 16U) ? V : 16U;
    for (unsigned int i = 0U; i < ktop; i++) {
        unsigned int bi = i;
        for (unsigned int j = i + 1U; j < V; j++) {
            if (h_logits[top_idx[j]] > h_logits[top_idx[bi]]) bi = j;
        }
        unsigned int tmp = top_idx[i]; top_idx[i] = top_idx[bi]; top_idx[bi] = tmp;
    }
    fprintf(fp, "  \"top_predictions\": [\n");
    for (unsigned int i = 0U; i < ktop; i++) {
        unsigned int vid = top_idx[i];
        uint8_t sb = (uint8_t)vid;
        const uint8_t *tp = &sb;
        size_t tl = 1U;
        if (m->vocab_size >= 256U && m->tok_pool && vid < V) {
            tp = m->tok_pool + m->tok_off[vid];
            tl = (size_t)m->tok_len[vid];
        }
        fprintf(fp, "    {\"id\": %u, \"text\": ", vid);
        fprint_json_bytes(fp, tp, tl);
        fprintf(fp, ", \"logit\": %.6f, \"prob\": %.6f}%s\n",
                (double)h_logits[vid], l_probs[vid], (i + 1U < ktop) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");

    fclose(fp);
    free(re); free(im); free(att); free(h_logits); free(l_probs); free(top_idx);
    return 0;
}

static uint64_t parse_u64_decimal(const char *s) {
    uint64_t v = 0ULL;
    if (!s) return 0ULL;
    while (*s == ' ' || *s == '\t') s++;
    while (*s >= '0' && *s <= '9') {
        v = v * 10ULL + (uint64_t)(unsigned char)(*s - '0');
        s++;
    }
    return v;
}

int main(int argc, char **argv) {
    int do_train = 0;
    int sft_mode = 0;
    int do_grad_check = 0;
    int interactive = 0;
    int clean_mode = 0;
    int chat_mode = 0;
    int keep_history = 0;
    int saw_prompt = 0;
    int saw_seq = 0;
    int saw_rep_penalty = 0;
    const char *save_path = NULL;
    const char *load_path = NULL;
    const char *resume_path = NULL;
    const char *data_path = NULL;
    const char *qasm_path = NULL;
    const char *prompt = "Q: What is ZKAEDI";
    const char *dump_logits_path = NULL;
    const char *dump_state_path = NULL;
    const char *dump_telemetry_path = NULL;

    unsigned int n_qubits = 6U;
    unsigned int n_registers = 1U;
    unsigned int n_heads = 4U;
    unsigned int n_layers = 2U;
    unsigned int pqc_depth = 2U;
    unsigned int bpe_vocab = 0U;
    unsigned int batch = 8U;
    unsigned int seq_len = 64U;
    int steps = -1;
    int warmup_steps = -1;
    int log_every = 40;
    int use_tc = -1;
    int use_kv_cache = 1;
    float lr = 0.008f;
    float wd = 0.01f;
    float grad_clip = 1.0f;
    float temp = 0.0f;
    int top_k = 0;
    float top_p = 1.0f;
    float rep_penalty = 1.0f;
    uint64_t seed = 1337ULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--train") == 0) do_train = 1;
        else if (strcmp(arg, "--sft") == 0) sft_mode = 1;
        else if (strcmp(arg, "--grad-check") == 0) do_grad_check = 1;
        else if (strcmp(arg, "--interactive") == 0) interactive = 1;
        else if (strcmp(arg, "--clean") == 0) clean_mode = 1;
        else if (strcmp(arg, "--chat") == 0) chat_mode = 1;
        else if (strcmp(arg, "--keep-history") == 0) keep_history = 1;
        else if (strcmp(arg, "--tensor-core") == 0) use_tc = 1;
        else if (strcmp(arg, "--no-tensor-core") == 0) use_tc = 0;
        else if (strcmp(arg, "--no-kv-cache") == 0) use_kv_cache = 0;
        else if (strcmp(arg, "--save") == 0 && i + 1 < argc) save_path = argv[++i];
        else if (strcmp(arg, "--load") == 0 && i + 1 < argc) load_path = argv[++i];
        else if (strcmp(arg, "--resume") == 0 && i + 1 < argc) { resume_path = argv[++i]; load_path = resume_path; do_train = 1; }
        else if (strcmp(arg, "--data") == 0 && i + 1 < argc) data_path = argv[++i];
        else if (strcmp(arg, "--export-qasm") == 0 && i + 1 < argc) qasm_path = argv[++i];
        else if (strcmp(arg, "--prompt") == 0 && i + 1 < argc) { prompt = argv[++i]; saw_prompt = 1; }
        else if (strcmp(arg, "--dump-logits") == 0 && i + 1 < argc) dump_logits_path = argv[++i];
        else if (strcmp(arg, "--dump-state") == 0 && i + 1 < argc) dump_state_path = argv[++i];
        else if (strcmp(arg, "--dump-telemetry") == 0 && i + 1 < argc) dump_telemetry_path = argv[++i];
        else if (strcmp(arg, "--bpe-vocab") == 0 && i + 1 < argc) bpe_vocab = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--qubits") == 0 && i + 1 < argc) n_qubits = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--registers") == 0 && i + 1 < argc) n_registers = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--heads") == 0 && i + 1 < argc) n_heads = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--layers") == 0 && i + 1 < argc) n_layers = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--pqc-depth") == 0 && i + 1 < argc) pqc_depth = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--batch") == 0 && i + 1 < argc) batch = (unsigned int)atoi(argv[++i]);
        else if (strcmp(arg, "--seq") == 0 && i + 1 < argc) { seq_len = (unsigned int)atoi(argv[++i]); saw_seq = 1; }
        else if (strcmp(arg, "--steps") == 0 && i + 1 < argc) steps = atoi(argv[++i]);
        else if (strcmp(arg, "--warmup") == 0 && i + 1 < argc) warmup_steps = atoi(argv[++i]);
        else if (strcmp(arg, "--log-every") == 0 && i + 1 < argc) log_every = atoi(argv[++i]);
        else if (strcmp(arg, "--lr") == 0 && i + 1 < argc) lr = (float)strtod(argv[++i], NULL);
        else if (strcmp(arg, "--wd") == 0 && i + 1 < argc) wd = (float)strtod(argv[++i], NULL);
        else if (strcmp(arg, "--grad-clip") == 0 && i + 1 < argc) grad_clip = (float)strtod(argv[++i], NULL);
        else if (strcmp(arg, "--temp") == 0 && i + 1 < argc) temp = (float)strtod(argv[++i], NULL);
        else if (strcmp(arg, "--top-k") == 0 && i + 1 < argc) top_k = atoi(argv[++i]);
        else if (strcmp(arg, "--top-p") == 0 && i + 1 < argc) top_p = (float)strtod(argv[++i], NULL);
        else if (strcmp(arg, "--rep-penalty") == 0 && i + 1 < argc) { rep_penalty = (float)strtod(argv[++i], NULL); saw_rep_penalty = 1; }
        else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) seed = parse_u64_decimal(argv[++i]);
        else {
            fprintf(stderr, "INVALID_ARG: unrecognized or incomplete flag '%s'\n", arg);
            return 1;
        }
    }
    (void)resume_path;

    if (n_qubits < 4U || n_qubits > 7U) {
        fprintf(stderr, "INVALID_ARG: --qubits must be in [4, 7] (got %u)\n", n_qubits);
        return 1;
    }
    if (n_registers != 1U && n_registers != 2U && n_registers != 4U && n_registers != 8U) {
        fprintf(stderr, "INVALID_ARG: --registers must be 1, 2, 4, or 8 (got %u)\n", n_registers);
        return 1;
    }
    unsigned int dim_chk = n_registers * (1U << n_qubits);
    if (n_heads == 0U || (dim_chk % n_heads) != 0U) {
        fprintf(stderr, "INVALID_ARG: --heads (%u) must divide registers*2^qubits (%u)\n", n_heads, dim_chk);
        return 1;
    }
    if (n_layers == 0U || n_layers > QLLM_MAX_LAYERS) {
        fprintf(stderr, "INVALID_ARG: --layers must be in [1, %u] (got %u)\n", QLLM_MAX_LAYERS, n_layers);
        return 1;
    }
    if (pqc_depth == 0U || pqc_depth > 6U) {
        fprintf(stderr, "INVALID_ARG: --pqc-depth must be in [1, 6] (got %u)\n", pqc_depth);
        return 1;
    }
    if (bpe_vocab != 0U && (bpe_vocab < 256U || bpe_vocab > QLLM_MAX_VOCAB)) {
        fprintf(stderr, "INVALID_ARG: --bpe-vocab must be 0 or in [256, %u] (got %u)\n", QLLM_MAX_VOCAB, bpe_vocab);
        return 1;
    }
    if (batch == 0U || batch > 32U || seq_len < 4U || seq_len > 256U) {
        fprintf(stderr, "INVALID_ARG: --batch [1..32] / --seq [4..256] out of bounds (%u, %u)\n", batch, seq_len);
        return 1;
    }
    if (!(lr > 0.0f && lr <= 10.0f) || !(temp >= 0.0f && temp <= 100.0f) || top_k < 0) {
        fprintf(stderr, "INVALID_ARG: invalid --lr, --temp, or --top-k\n");
        return 1;
    }
    if ((saw_rep_penalty && !(rep_penalty >= 1.0f && rep_penalty <= 5.0f)) || !(top_p > 0.0f && top_p <= 1.0f)) {
        fprintf(stderr, "INVALID_ARG: --rep-penalty [1.0..5.0] or --top-p (0.0..1.0] out of bounds\n");
        return 1;
    }
    if (!saw_seq && interactive) seq_len = 128U;
    float eff_rep = saw_rep_penalty ? rep_penalty : (chat_mode ? 1.15f : 1.0f);
    if (steps < 0) steps = do_train ? 360 : 96;
    if (log_every < 1) log_every = 1;

    check_cu(cuInit(0U), "cuInit");
    int dev_cnt = 0, drv_ver = 0, dev = 0;
    check_cu(cuDriverGetVersion(&drv_ver), "cuDriverGetVersion");
    check_cu(cuDeviceGetCount(&dev_cnt), "cuDeviceGetCount");
    if (dev_cnt < 1) { fprintf(stderr, "FATAL: no CUDA device\n"); return 1; }
    check_cu(cuDeviceGet(&dev, 0), "cuDeviceGet");
    char dev_name[128];
    memset(dev_name, 0, sizeof(dev_name));
    check_cu(cuDeviceGetName(dev_name, sizeof(dev_name) - 1, dev), "cuDeviceGetName");
    size_t vram_bytes = 0U;
    check_cu(cuDeviceTotalMem_v2(&vram_bytes, dev), "cuDeviceTotalMem_v2");

    void *ctx = NULL, *mod = NULL;
    check_cu(cuCtxCreate_v2(&ctx, 0U, dev), "cuCtxCreate_v2");
    check_cu(cuModuleLoadDataEx(&mod, g_gpu_qllm_kernels_ptx, 0U, NULL, NULL), "cuModuleLoadDataEx");
    QKernels k;
    load_qkernels(mod, &k);

    FILE *info_fp = clean_mode ? stderr : stdout;
    uint8_t *corpus = NULL;
    size_t corpus_len = 0U;
    uint32_t *corpus_toks = NULL;
    size_t corpus_ntoks = 0U;

    QModel m;
    memset(&m, 0, sizeof(m));
    if (load_path) {
        if (load_zqnn(&m, load_path) != 0) {
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (do_train) {
            corpus = load_training_corpus(data_path, &corpus_len, (m.vocab_size >= 256U || m.version >= 2U) ? 1 : 0);
            if (!corpus) {
                free_model(&m); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
                return 1;
            }
        }
    } else {
        m.n_qubits = n_qubits;
        m.n_registers = n_registers;
        m.n_heads = n_heads;
        m.n_layers = n_layers;
        m.pqc_depth = pqc_depth;
        if (do_train) {
            corpus = load_training_corpus(data_path, &corpus_len, (bpe_vocab > 0U) ? 1 : 0);
            if (!corpus) {
                cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
                return 1;
            }
            if (bpe_vocab > 0U) {
                bpe_train_merges(&m, corpus, corpus_len, bpe_vocab);
            } else {
                m.version = (m.n_registers > 1U) ? 3U : 1U;
                m.vocab_size = QLLM_VOCAB;
            }
        } else {
            m.version = (m.n_registers > 1U) ? 3U : ((bpe_vocab > 0U) ? 2U : 1U);
            m.vocab_size = (bpe_vocab > 0U) ? bpe_vocab : QLLM_VOCAB;
        }
        compute_model_layout(&m);
        init_model_weights(&m, seed);
    }

    int tc_on = (use_tc >= 0) ? use_tc : ((m.dim >= 128U && (batch * seq_len) >= 64U) ? 1 : 0);
    unsigned int out_regs = (m.n_registers > 0U) ? m.n_registers : 1U;
    fprintf(info_fp, "QLLM_DEVICE name=\"%s\" vram_mib=%u driver=%d ptx_bytes=%u\n",
            dev_name, (unsigned int)(vram_bytes >> 20), drv_ver, (unsigned int)(sizeof(g_gpu_qllm_kernels_ptx) - 1U));
    fprintf(info_fp, "QLLM_MODEL arch=zkaedi_hybrid_qtransformer qubits=%u regs=%u logical_qubits=%u hilbert_dim=%u heads=%u layers=%u pqc_depth=%u h_ff=%u params=%u vocab=%u version=%u tc=%d kv=%d\n",
            m.n_qubits, out_regs, out_regs * m.n_qubits, m.dim, m.n_heads, m.n_layers, m.pqc_depth, m.h_ff, m.total_params, m.vocab_size, m.version, tc_on, use_kv_cache);
    fflush(info_fp);

    alloc_model_gpu(&m, (do_train || do_grad_check) ? 1 : 0);

    if (do_grad_check) {
        if (corpus) free(corpus);
        int rc = run_gradient_check(&m, &k, sft_mode);
        free_model(&m);
        cuModuleUnload(mod);
        cuCtxDestroy_v2(ctx);
        return rc;
    }

    unsigned int max_gen_ctx = seq_len;
    if (do_train) {
        corpus_toks = (uint32_t *)malloc(corpus_len * sizeof(uint32_t));
        if (!corpus_toks) exit(1);
        corpus_ntoks = bpe_encode(&m, corpus, corpus_len, corpus_toks, corpus_len);
        if (corpus_ntoks < 2U) {
            free(corpus_toks); free(corpus);
            fprintf(stderr, "INVALID_DATA: encoded corpus has < 2 tokens\n");
            free_model(&m); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (m.vocab_size >= 256U) {
            printf("QLLM_BPE vocab=%u merges=%u raw_bytes=%u tokens=%u compression=%.3fx\n",
                   m.vocab_size, m.n_merges, (unsigned int)corpus_len, (unsigned int)corpus_ntoks,
                   (double)corpus_len / (double)corpus_ntoks);
            fflush(stdout);
        }

        SftDataset sft_ds;
        memset(&sft_ds, 0, sizeof(sft_ds));
        if (sft_mode) {
            if (parse_sft_dataset(&m, corpus, corpus_len, seq_len, &sft_ds) == 0U) {
                free_sft_dataset(&sft_ds);
                free(corpus_toks); free(corpus);
                fprintf(stderr, "INVALID_DATA: --sft requires Q:/A: examples separated by blank lines\n");
                free_model(&m); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
                return 1;
            }
            double sum_p = 0.0, sum_a = 0.0;
            for (unsigned int ei = 0U; ei < sft_ds.count; ei++) {
                sum_p += (double)sft_ds.items[ei].prompt_len;
                sum_a += (double)(sft_ds.items[ei].len - sft_ds.items[ei].prompt_len);
            }
            printf("QLLM_SFT examples=%u avg_prompt_tok=%.1f avg_answer_tok=%.1f\n",
                   sft_ds.count, sum_p / (double)sft_ds.count, sum_a / (double)sft_ds.count);
            fflush(stdout);
        }

        unsigned int BT = batch * seq_len;
        unsigned int max_bt = (BT > max_gen_ctx) ? BT : max_gen_ctx;
        unsigned int max_att_train = batch * m.n_heads * seq_len * seq_len;
        unsigned int max_att_gen = m.n_heads * max_gen_ctx * max_gen_ctx;
        unsigned int max_att = (max_att_train > max_att_gen) ? max_att_train : max_att_gen;

        QActArena a;
        alloc_act_arena(&m, &a, max_bt, max_att, 1);

        uint32_t *h_tok = (uint32_t *)malloc((size_t)BT * 4U);
        uint32_t *h_tgt = (uint32_t *)malloc((size_t)BT * 4U);
        if (!h_tok || !h_tgt) exit(1);

        Qrng batch_rng;
        batch_rng.s = (seed == 0ULL ? 1337ULL : seed) ^ 0xA5A5A5A55A5A5A5AULL;
        size_t max_start = (corpus_ntoks > (size_t)seq_len + 1U) ? (corpus_ntoks - (size_t)seq_len - 1U) : 1U;

        void *ev0 = NULL, *ev1 = NULL;
        check_cu(cuEventCreate(&ev0, 0U), "tr_ev0");
        check_cu(cuEventCreate(&ev1, 0U), "tr_ev1");
        check_cu(cuEventRecord(ev0, NULL), "tr_ev0_rec");

        float init_loss = 0.0f, final_loss = 0.0f;
        QDiag final_diag;
        memset(&final_diag, 0, sizeof(final_diag));
        int eff_warmup = (warmup_steps >= 0) ? warmup_steps : ((steps >= 40 && (m.n_layers >= 3U || m.n_registers > 1U)) ? (steps / 20) : 0);
        if (eff_warmup >= steps) eff_warmup = (steps > 1) ? (steps / 4) : 0;

        for (int step = 1; step <= steps; step++) {
            unsigned int active_cnt = 0U;
            if (sft_mode) {
                for (unsigned int b = 0U; b < batch; b++) {
                    unsigned int eidx = (unsigned int)(qrng_next_u64(&batch_rng) % (uint64_t)sft_ds.count);
                    const SftExample *ex = &sft_ds.items[eidx];
                    for (unsigned int t = 0U; t < seq_len; t++) {
                        if (t < ex->len) {
                            h_tok[b * seq_len + t] = ex->toks[t];
                            if ((t + 1U) < ex->prompt_len || (t + 1U) >= ex->len) {
                                h_tgt[b * seq_len + t] = 0xFFFFFFFFU;
                            } else {
                                h_tgt[b * seq_len + t] = ex->toks[t + 1U];
                                active_cnt++;
                            }
                        } else {
                            h_tok[b * seq_len + t] = (uint32_t)' ';
                            h_tgt[b * seq_len + t] = 0xFFFFFFFFU;
                        }
                    }
                }
                if (active_cnt == 0U) active_cnt = 1U;
            } else {
                for (unsigned int b = 0U; b < batch; b++) {
                    size_t start = (size_t)(qrng_next_u64(&batch_rng) % (uint64_t)max_start);
                    for (unsigned int t = 0U; t < seq_len; t++) {
                        size_t idx_t = (start + t) % corpus_ntoks;
                        size_t idx_n = (start + t + 1U) % corpus_ntoks;
                        h_tok[b * seq_len + t] = corpus_toks[idx_t];
                        h_tgt[b * seq_len + t] = corpus_toks[idx_n];
                    }
                }
            }
            check_cu(cuMemcpyHtoD_v2(a.d_tokens, h_tok, (size_t)BT * 4U), "HtoD(tr_tok)");
            check_cu(cuMemcpyHtoD_v2(a.d_targets, h_tgt, (size_t)BT * 4U), "HtoD(tr_tgt)");

            int want_diag = (step == 1 || step == steps || (step % log_every) == 0);
            run_forward(&m, &k, &a, batch, seq_len, 0U, tc_on);
            float step_loss = run_loss_and_backward(&m, &k, &a, batch, seq_len, want_diag, tc_on, active_cnt);
            if (step == 1) init_loss = step_loss;
            if (want_diag) {
                check_cu(cuCtxSynchronize(), "tr_diag_sync");
                final_loss = step_loss;
                final_diag = measure_quantum_diagnostics(&m, &a, BT);
                printf("QLLM_TRAIN_STEP step=%d/%d loss=%.6f ppl=%.2f norm_err=%.7f renyi2_bits=%.4f linear_S=%.4f\n",
                       step, steps, (double)step_loss, exp((double)step_loss),
                       (double)final_diag.max_norm_err, (double)final_diag.mean_renyi2_bits,
                       (double)final_diag.mean_linear_entropy);
                fflush(stdout);
            }
            float cur_lr;
            if (eff_warmup > 0 && step <= eff_warmup) {
                cur_lr = lr * ((float)step / (float)eff_warmup);
            } else {
                int rem = steps - eff_warmup;
                double frac = (rem > 1) ? ((double)(step - eff_warmup - 1) / (double)(rem - 1)) : 1.0;
                cur_lr = lr * (0.1f + 0.45f * (float)(1.0 + cos(3.14159265358979323846 * frac)));
            }
            run_adamw_update(&m, &k, &a, cur_lr, wd, grad_clip, (unsigned int)step);
        }

        check_cu(cuEventRecord(ev1, NULL), "tr_ev1_rec");
        check_cu(cuEventSynchronize(ev1), "tr_ev1_sync");
        float train_ms = 0.0f;
        check_cu(cuEventElapsedTime(&train_ms, ev0, ev1), "tr_ms");
        cuEventDestroy_v2(ev0); cuEventDestroy_v2(ev1);

        double tok_per_sec = (train_ms > 0.0f) ? ((double)steps * (double)BT * 1000.0 / (double)train_ms) : 0.0;
        printf("QLLM_TRAIN_OK steps=%d corpus_bytes=%u init_loss=%.6f final_loss=%.6f train_ms=%.2f train_tok_per_sec=%.2f norm_err=%.7f renyi2_bits=%.4f\n",
               steps, (unsigned int)corpus_len, (double)init_loss, (double)final_loss,
               (double)train_ms, tok_per_sec, (double)final_diag.max_norm_err, (double)final_diag.mean_renyi2_bits);
        fflush(stdout);

        if (save_path && save_zqnn(&m, save_path) != 0) {
            free_sft_dataset(&sft_ds);
            free(h_tok); free(h_tgt); free(corpus_toks); free(corpus);
            free_act_arena(&m, &a); free_model(&m);
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (qasm_path && export_openqasm3(&m, qasm_path) != 0) {
            free_sft_dataset(&sft_ds);
            free(h_tok); free(h_tgt); free(corpus_toks); free(corpus);
            free_act_arena(&m, &a); free_model(&m);
            cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 1;
        }
        if (save_path) { printf("QLLM_SAVED path=\"%s\"\n", save_path); fflush(stdout); }
        if (qasm_path) { printf("QLLM_QASM_EXPORTED path=\"%s\"\n", qasm_path); fflush(stdout); }

        free_sft_dataset(&sft_ds);
        free(h_tok); free(h_tgt); free(corpus_toks); free(corpus);
        free_act_arena(&m, &a);
        free_model(&m);
        cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
        return 0;
    }

    if (qasm_path) {
        int qrc = export_openqasm3(&m, qasm_path);
        if (qrc != 0) { free_model(&m); cuModuleUnload(mod); cuCtxDestroy_v2(ctx); return 1; }
        printf("QLLM_QASM_EXPORTED path=\"%s\"\n", qasm_path);
        fflush(stdout);
        if (!interactive && !saw_prompt && !dump_logits_path && !dump_state_path && !dump_telemetry_path && steps == 96) {
            free_model(&m); cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
            return 0;
        }
    }

    QActArena a;
    alloc_act_arena(&m, &a, max_gen_ctx, m.n_heads * max_gen_ctx * max_gen_ctx, 0);
    uint64_t rng = (seed == 0ULL) ? 1337ULL : seed;
    uint32_t *g_ctx = (uint32_t *)malloc((size_t)max_gen_ctx * 4U);
    uint32_t *g_ids = (uint32_t *)malloc((size_t)(steps > 0 ? steps + 4 : 8) * 4U);
    size_t g_cap = (size_t)(steps > 0 ? steps + 4 : 8) * 32U;
    char *g_text = (char *)malloc(g_cap);
    if (!g_ctx || !g_ids || !g_text) exit(1);

    if (interactive) {
        unsigned int ctx_len = 0U;
        if (clean_mode) {
            fprintf(stderr, "[Quantum interactive ready: qubits=%u regs=%u dim=%u layers=%u pqc_depth=%u vocab=%u v=%u ctx=%u | /reset or /quit]\n",
                    m.n_qubits, out_regs, m.dim, m.n_layers, m.pqc_depth, m.vocab_size, m.version, max_gen_ctx);
        } else {
            printf("QLLM_INTERACTIVE_READY max_seq=%u\n", max_gen_ctx);
            fflush(stdout);
        }
        char line[1024];
        int turn = 0;
        int need_sep = 0;
        for (;;) {
            if (clean_mode) { fprintf(stderr, "\nYou> "); fflush(stderr); }
            if (!fgets(line, sizeof(line), stdin)) break;
            size_t llen = strlen(line);
            if (llen >= 3U && (uint8_t)line[0] == 0xEFU && (uint8_t)line[1] == 0xBBU && (uint8_t)line[2] == 0xBFU) {
                memmove(line, line + 3U, llen - 2U);
                llen -= 3U;
            }
            while (llen > 0U && (line[llen - 1U] == '\n' || line[llen - 1U] == '\r')) line[--llen] = '\0';
            if (llen == 0U) continue;
            if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) break;
            if (strcmp(line, "/reset") == 0) {
                ctx_len = 0U;
                turn = 0;
                need_sep = 0;
                if (clean_mode) { fprintf(stderr, "[Quantum state cleared, pos=0]\n"); fflush(stderr); }
                else { printf("QLLM_REPL_RESET kv_cleared=1 pos=0\n"); fflush(stdout); }
                continue;
            }
            char fmt_buf[1400];
            const char *in_str = line;
            if (chat_mode) {
                if (!keep_history) ctx_len = 0U;
                if (strncmp(line, "Q:", 2U) != 0) {
                    if (ctx_len > 0U && need_sep) snprintf(fmt_buf, sizeof(fmt_buf), "\n\nQ: %s\nA: ", line);
                    else snprintf(fmt_buf, sizeof(fmt_buf), "Q: %s\nA: ", line);
                    in_str = fmt_buf;
                }
            }
            size_t ilen = strlen(in_str);
            uint32_t *enc_ids = (uint32_t *)malloc((ilen + 4U) * sizeof(uint32_t));
            if (!enc_ids) exit(1);
            size_t n_enc = bpe_encode(&m, (const uint8_t *)in_str, ilen, enc_ids, ilen + 4U);
            for (size_t ci = 0U; ci < n_enc; ci++) {
                if (ctx_len >= max_gen_ctx - 1U) {
                    unsigned int keep = (max_gen_ctx * 3U) / 4U;
                    unsigned int drop = ctx_len - keep;
                    memmove(g_ctx, g_ctx + drop, (size_t)keep * 4U);
                    ctx_len = keep;
                }
                g_ctx[ctx_len++] = enc_ids[ci];
            }
            free(enc_ids);
            float tps = 0.0f, t1_log = 0.0f;
            uint32_t t1 = 0U, fnv = 0U;
            int g_cnt = 0;
            generate_sequence(&m, &k, &a, g_ctx, &ctx_len, max_gen_ctx, use_kv_cache, steps, temp, top_k, top_p, eff_rep, &rng,
                              clean_mode ? 1 : 0, "Assistant> ", g_text, g_cap, g_ids, &g_cnt,
                              NULL, NULL, chat_mode ? 1 : 0, &tps, &t1, &t1_log, &fnv);
            size_t gl = strlen(g_text);
            need_sep = (gl >= 2U && g_text[gl - 1U] == '\n' && g_text[gl - 2U] == '\n') ? 0 : 1;
            if (clean_mode) {
                fprintf(stderr, "[%d gen tok | %.1f tok/s | pos=%u/%u]\n", g_cnt, (double)tps, ctx_len, max_gen_ctx);
                fflush(stderr);
            } else {
                printf("QLLM_TURN turn=%d gen_tokens=%d pos=%u top1_id=%u logits_fnv=%08x text=\"",
                       turn, g_cnt, ctx_len, t1, fnv);
                print_escaped(g_text, strlen(g_text));
                printf("\"\n");
                fflush(stdout);
            }
            turn++;
        }
        if (clean_mode) fprintf(stderr, "\n[Session ended: turns=%d final_pos=%u]\n", turn, ctx_len);
        else { printf("QLLM_INTERACTIVE_OK turns=%d final_pos=%u\n", turn, ctx_len); fflush(stdout); }
    } else {
        size_t plen = strlen(prompt);
        uint32_t *enc_ids = (uint32_t *)malloc((plen + 4U) * sizeof(uint32_t));
        if (!enc_ids) exit(1);
        size_t n_enc = bpe_encode(&m, (const uint8_t *)prompt, plen, enc_ids, plen + 4U);
        unsigned int ctx_len = 0U;
        for (size_t i = 0U; i < n_enc; i++) {
            if (ctx_len >= max_gen_ctx - 1U) break;
            g_ctx[ctx_len++] = enc_ids[i];
        }
        free(enc_ids);
        float tps = 0.0f, t1_log = 0.0f;
        uint32_t t1 = 0U, fnv = 0U;
        int g_cnt = 0;
        generate_sequence(&m, &k, &a, g_ctx, &ctx_len, max_gen_ctx, use_kv_cache, steps, temp, top_k, top_p, eff_rep, &rng,
                          0, NULL, g_text, g_cap, g_ids, &g_cnt,
                          dump_logits_path, dump_state_path, 0, &tps, &t1, &t1_log, &fnv);
        QDiag qd = measure_quantum_diagnostics(&m, &a, (ctx_len > 0U) ? 1U : 0U);
        printf("QLLM_TEXT prompt=\""); print_escaped(prompt, plen); printf("\" generated=\"");
        print_escaped(g_text, strlen(g_text)); printf("\"\n");
        printf("QLLM_OK steps=%d tok_per_sec=%.2f top1_id=%u top1_logit=%.6f logits_fnv=%08x norm_err=%.7f\n",
               g_cnt, (double)tps, t1, (double)t1_log, fnv, (double)qd.max_norm_err);
        fflush(stdout);
        if (dump_telemetry_path) {
            if (dump_telemetry_json(&m, &k, &a, g_ctx, ctx_len, prompt, g_text, tps, fnv, t1,
                                    dev_name, vram_bytes, drv_ver,
                                    (unsigned int)(sizeof(g_gpu_qllm_kernels_ptx) - 1U),
                                    dump_telemetry_path) != 0) {
                free(g_ctx); free(g_ids); free(g_text);
                free_act_arena(&m, &a); free_model(&m);
                cuModuleUnload(mod); cuCtxDestroy_v2(ctx);
                return 1;
            }
            fprintf(info_fp, "QLLM_TELEMETRY_SAVED path=\"%s\"\n", dump_telemetry_path);
            fflush(info_fp);
        }
    }

    free(g_ctx); free(g_ids); free(g_text);
    free_act_arena(&m, &a);
    free_model(&m);
    cuModuleUnload(mod);
    cuCtxDestroy_v2(ctx);
    return 0;
}
