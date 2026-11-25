/*
 * ZKAEDI ZCC // Sovereign Zero-PyTorch Blackwell SM 12.0 GPU LoRA Fine-Tuner (Track 19)
 * ====================================================================================
 * Full forward + causal GQA/RoPE/SwiGLU/RMSNorm backward autodiff + AdamW optimizer
 * compiled directly to Win64 PE32+ + PTX ISA 8.7 with ZERO Python, ZERO PyTorch,
 * ZERO MSVC, ZERO MinGW, ZERO CUDA Toolkit (System32 msvcrt.dll + nvcuda.dll ONLY).
 */

#define ZCC_GPU_LLM_LIBRARY 1
#include "win64_gpu_llm.c"
#include "gpu_lora_kernels_ptx.h"

/* LoRA kernel handles (mod_lora loaded from g_gpu_lora_kernels_ptx) */
static void *lfn_zero = NULL;
static void *lfn_add_out = NULL;
static void *lfn_add_inplace = NULL;
static void *lfn_amax_blocks = NULL;
static void *lfn_amax_slice_blocks = NULL;
static void *lfn_amax_finalize = NULL;
static void *lfn_copy_scaled_f32 = NULL;
static void *lfn_copy_cols_scaled_f32 = NULL;
static void *lfn_f32_to_f16_pad = NULL;
static void *lfn_transpose_f16 = NULL;
static void *lfn_copy_cols_f32 = NULL;
static void *lfn_slice_f32_to_f16 = NULL;
static void *lfn_mm_f32w_nt = NULL;
static void *lfn_mm_f32w_tx = NULL;
static void *lfn_mm_f16w_nt = NULL;
static void *lfn_mm_f16w_tx = NULL;
static void *lfn_fwd_a = NULL;
static void *lfn_fwd_b_add = NULL;
static void *lfn_bwd_dh = NULL;
static void *lfn_bwd_db = NULL;
static void *lfn_bwd_da = NULL;
static void *lfn_bwd_dx = NULL;
static void *lfn_rmsnorm_bwd = NULL;
static void *lfn_rope_bwd = NULL;
static void *lfn_attn_fwd = NULL;
static void *lfn_attn_bwd_ds = NULL;
static void *lfn_attn_bwd_q = NULL;
static void *lfn_attn_bwd_kv = NULL;
static void *lfn_swiglu_fwd = NULL;
static void *lfn_swiglu_bwd = NULL;
static void *lfn_ce_masked_bwd = NULL;
static void *lfn_sq_sum = NULL;
static void *lfn_scale = NULL;
static void *lfn_adamw = NULL;

/* Track 23A: Evolved Blackwell sm_120 Decoder kernel handles */
static void *dfn_rmsnorm = NULL;
static void *dfn_matvec_q4_k_4w = NULL;
static void *dfn_matvec_q4_k_8r = NULL;
static void *dfn_matvec_q6_k_4w = NULL;
static void *dfn_matvec_f32_4w = NULL;
static void *dfn_matvec_f16_4w = NULL;
static void *dfn_matvec_bf16_4w = NULL;
static void *dfn_matvec_q8_0_4w = NULL;
static void *dfn_matvec_q4_0_4w = NULL;
static void *dfn_embed_ind_f32 = NULL;
static void *dfn_embed_ind_f16 = NULL;
static void *dfn_embed_ind_bf16 = NULL;
static void *dfn_embed_ind_q8_0 = NULL;
static void *dfn_embed_ind_q4_0 = NULL;
static void *dfn_embed_ind_q4_k = NULL;
static void *dfn_embed_ind_q6_k = NULL;
static void *dfn_rope_kv_ind_f16 = NULL;
static void *dfn_rope_kv_ind_f32 = NULL;
static void *dfn_attn_flash_ind_f16 = NULL;
static void *dfn_attn_flash_ind_f32 = NULL;
static void *dfn_lora_fwd_a_ind = NULL;
static void *dfn_lora_fwd_b_add_ind = NULL;
static void *dfn_lora_qkv_a_ind = NULL;
static void *dfn_lora_qkv_b_add_ind = NULL;
static void *dfn_argmax_blocks = NULL;
static void *dfn_argmax_finalize = NULL;

static int load_lora_kernels(void *mod) {
    check_cu(cuModuleGetFunction(&lfn_zero, mod, "lora_zero_f32"), "get(lora_zero_f32)");
    check_cu(cuModuleGetFunction(&lfn_add_out, mod, "lora_add_f32_out"), "get(lora_add_f32_out)");
    check_cu(cuModuleGetFunction(&lfn_add_inplace, mod, "lora_add_inplace_f32"), "get(lora_add_inplace_f32)");
    check_cu(cuModuleGetFunction(&lfn_amax_blocks, mod, "lora_amax_blocks"), "get(lora_amax_blocks)");
    check_cu(cuModuleGetFunction(&lfn_amax_slice_blocks, mod, "lora_amax_slice_blocks"), "get(lora_amax_slice_blocks)");
    check_cu(cuModuleGetFunction(&lfn_amax_finalize, mod, "lora_amax_finalize"), "get(lora_amax_finalize)");
    check_cu(cuModuleGetFunction(&lfn_copy_scaled_f32, mod, "lora_copy_scaled_f32"), "get(lora_copy_scaled_f32)");
    check_cu(cuModuleGetFunction(&lfn_copy_cols_scaled_f32, mod, "lora_copy_cols_scaled_f32"), "get(lora_copy_cols_scaled_f32)");
    check_cu(cuModuleGetFunction(&lfn_f32_to_f16_pad, mod, "lora_f32_to_f16_pad"), "get(lora_f32_to_f16_pad)");
    check_cu(cuModuleGetFunction(&lfn_transpose_f16, mod, "lora_transpose_f16"), "get(lora_transpose_f16)");
    check_cu(cuModuleGetFunction(&lfn_copy_cols_f32, mod, "lora_copy_cols_f32"), "get(lora_copy_cols_f32)");
    check_cu(cuModuleGetFunction(&lfn_slice_f32_to_f16, mod, "lora_slice_f32_to_f16"), "get(lora_slice_f32_to_f16)");
    check_cu(cuModuleGetFunction(&lfn_mm_f32w_nt, mod, "lora_matmul_f32w_nt"), "get(lora_matmul_f32w_nt)");
    check_cu(cuModuleGetFunction(&lfn_mm_f32w_tx, mod, "lora_matmul_f32w_tx"), "get(lora_matmul_f32w_tx)");
    check_cu(cuModuleGetFunction(&lfn_mm_f16w_nt, mod, "lora_matmul_f16w_nt"), "get(lora_matmul_f16w_nt)");
    check_cu(cuModuleGetFunction(&lfn_mm_f16w_tx, mod, "lora_matmul_f16w_tx"), "get(lora_matmul_f16w_tx)");
    check_cu(cuModuleGetFunction(&lfn_fwd_a, mod, "lora_fwd_a"), "get(lora_fwd_a)");
    check_cu(cuModuleGetFunction(&lfn_fwd_b_add, mod, "lora_fwd_b_add"), "get(lora_fwd_b_add)");
    check_cu(cuModuleGetFunction(&lfn_bwd_dh, mod, "lora_bwd_dh"), "get(lora_bwd_dh)");
    check_cu(cuModuleGetFunction(&lfn_bwd_db, mod, "lora_bwd_db_accum"), "get(lora_bwd_db_accum)");
    check_cu(cuModuleGetFunction(&lfn_bwd_da, mod, "lora_bwd_da_accum"), "get(lora_bwd_da_accum)");
    check_cu(cuModuleGetFunction(&lfn_bwd_dx, mod, "lora_bwd_dx_add"), "get(lora_bwd_dx_add)");
    check_cu(cuModuleGetFunction(&lfn_rmsnorm_bwd, mod, "lora_rmsnorm_bwd"), "get(lora_rmsnorm_bwd)");
    check_cu(cuModuleGetFunction(&lfn_rope_bwd, mod, "lora_rope_bwd"), "get(lora_rope_bwd)");
    check_cu(cuModuleGetFunction(&lfn_attn_fwd, mod, "lora_attn_gqa_fwd"), "get(lora_attn_gqa_fwd)");
    check_cu(cuModuleGetFunction(&lfn_attn_bwd_ds, mod, "lora_attn_gqa_bwd_ds"), "get(lora_attn_gqa_bwd_ds)");
    check_cu(cuModuleGetFunction(&lfn_attn_bwd_q, mod, "lora_attn_gqa_bwd_q"), "get(lora_attn_gqa_bwd_q)");
    check_cu(cuModuleGetFunction(&lfn_attn_bwd_kv, mod, "lora_attn_gqa_bwd_kv"), "get(lora_attn_gqa_bwd_kv)");
    check_cu(cuModuleGetFunction(&lfn_swiglu_fwd, mod, "lora_swiglu_fwd_batch"), "get(lora_swiglu_fwd_batch)");
    check_cu(cuModuleGetFunction(&lfn_swiglu_bwd, mod, "lora_swiglu_bwd_batch"), "get(lora_swiglu_bwd_batch)");
    check_cu(cuModuleGetFunction(&lfn_ce_masked_bwd, mod, "lora_ce_loss_masked_bwd"), "get(lora_ce_loss_masked_bwd)");
    check_cu(cuModuleGetFunction(&lfn_sq_sum, mod, "lora_sq_sum_blocks"), "get(lora_sq_sum_blocks)");
    check_cu(cuModuleGetFunction(&lfn_scale, mod, "lora_scale_f32"), "get(lora_scale_f32)");
    check_cu(cuModuleGetFunction(&lfn_adamw, mod, "lora_adamw_step"), "get(lora_adamw_step)");
    check_cu(cuModuleGetFunction(&dfn_rmsnorm, mod, "dec_rmsnorm_f32"), "get(dec_rmsnorm_f32)");
    check_cu(cuModuleGetFunction(&dfn_matvec_q4_k_4w, mod, "dec_matvec_q4_k_4w"), "get(dec_matvec_q4_k_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_q4_k_8r, mod, "dec_matvec_q4_k_8r"), "get(dec_matvec_q4_k_8r)");
    check_cu(cuModuleGetFunction(&dfn_matvec_q6_k_4w, mod, "dec_matvec_q6_k_4w"), "get(dec_matvec_q6_k_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_f32_4w, mod, "dec_matvec_f32_4w"), "get(dec_matvec_f32_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_f16_4w, mod, "dec_matvec_f16_4w"), "get(dec_matvec_f16_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_bf16_4w, mod, "dec_matvec_bf16_4w"), "get(dec_matvec_bf16_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_q8_0_4w, mod, "dec_matvec_q8_0_4w"), "get(dec_matvec_q8_0_4w)");
    check_cu(cuModuleGetFunction(&dfn_matvec_q4_0_4w, mod, "dec_matvec_q4_0_4w"), "get(dec_matvec_q4_0_4w)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_f32, mod, "dec_embed_ind_f32"), "get(dec_embed_ind_f32)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_f16, mod, "dec_embed_ind_f16"), "get(dec_embed_ind_f16)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_bf16, mod, "dec_embed_ind_bf16"), "get(dec_embed_ind_bf16)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_q8_0, mod, "dec_embed_ind_q8_0"), "get(dec_embed_ind_q8_0)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_q4_0, mod, "dec_embed_ind_q4_0"), "get(dec_embed_ind_q4_0)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_q4_k, mod, "dec_embed_ind_q4_k"), "get(dec_embed_ind_q4_k)");
    check_cu(cuModuleGetFunction(&dfn_embed_ind_q6_k, mod, "dec_embed_ind_q6_k"), "get(dec_embed_ind_q6_k)");
    check_cu(cuModuleGetFunction(&dfn_rope_kv_ind_f16, mod, "dec_rope_kv_ind_f16"), "get(dec_rope_kv_ind_f16)");
    check_cu(cuModuleGetFunction(&dfn_rope_kv_ind_f32, mod, "dec_rope_kv_ind_f32"), "get(dec_rope_kv_ind_f32)");
    check_cu(cuModuleGetFunction(&dfn_attn_flash_ind_f16, mod, "dec_attn_flash_ind_f16"), "get(dec_attn_flash_ind_f16)");
    check_cu(cuModuleGetFunction(&dfn_attn_flash_ind_f32, mod, "dec_attn_flash_ind_f32"), "get(dec_attn_flash_ind_f32)");
    check_cu(cuModuleGetFunction(&dfn_lora_fwd_a_ind, mod, "dec_lora_fwd_a_ind"), "get(dec_lora_fwd_a_ind)");
    check_cu(cuModuleGetFunction(&dfn_lora_fwd_b_add_ind, mod, "dec_lora_fwd_b_add_ind"), "get(dec_lora_fwd_b_add_ind)");
    check_cu(cuModuleGetFunction(&dfn_lora_qkv_a_ind, mod, "dec_lora_qkv_a_ind"), "get(dec_lora_qkv_a_ind)");
    check_cu(cuModuleGetFunction(&dfn_lora_qkv_b_add_ind, mod, "dec_lora_qkv_b_add_ind"), "get(dec_lora_qkv_b_add_ind)");
    check_cu(cuModuleGetFunction(&dfn_argmax_blocks, mod, "dec_argmax_blocks"), "get(dec_argmax_blocks)");
    check_cu(cuModuleGetFunction(&dfn_argmax_finalize, mod, "dec_argmax_finalize"), "get(dec_argmax_finalize)");
    return g_zcc_llm_failed ? -1 : 0;
}

/* LoRA Adapter Structures */
typedef struct {
    unsigned int N_out;
    unsigned int K_in;
    unsigned int r;
    uint64_t d_A, d_B;
    uint64_t d_dA, d_dB;
    uint64_t d_mA, d_vA;
    uint64_t d_mB, d_vB;
} LoraProj;

typedef struct {
    LoraProj q, k, v, o;
} LoraLayer;

typedef struct {
    uint64_t x_in;
    uint64_t x_norm1;
    uint64_t q;
    uint64_t k;
    uint64_t v;
    uint64_t h_q, h_k, h_v, h_o;
    uint64_t attn_probs;
    uint64_t attn_ctx;
    uint64_t attn_out;
    uint64_t x_mid;
    uint64_t x_norm2;
    uint64_t gate_pre;
    uint64_t up_pre;
} LoraLayerAct;

typedef struct {
    unsigned int max_T;
    unsigned int max_T_pad;
    unsigned int lm_chunk;
    int fp32_matmul;
    LoraLayerAct *layers;
    uint64_t x_final;
    uint64_t x_final_norm;
    uint64_t logits;
    uint64_t dlogits;
    uint64_t losses;
    uint64_t targets;
    /* Scratch backward & matmul buffers */
    uint64_t x_f16;
    uint64_t w_f16;
    uint64_t w_f16_T;
    uint64_t tc_tmp;
    uint64_t chunk_f32;
    uint64_t ffn_act;
    uint64_t dx_cur;
    uint64_t dx_mid;
    uint64_t dx_norm;
    uint64_t dffn_act;
    uint64_t dgate;
    uint64_t dup;
    uint64_t dattn_ctx;
    uint64_t attn_ds;
    uint64_t dq, dk, dv;
    uint64_t dh_tmp;
    uint64_t sq_blk;
    uint64_t scale_pair;
    uint64_t inf_h; /* single-token inference LoRA hidden scratch */
} LoraTrainMem;

typedef struct {
    uint32_t *toks;
    uint32_t *targets;
    unsigned int T;
    unsigned int active_cnt;
    unsigned int pref_len;
} LoraExample;

typedef struct {
    LoraExample *items;
    size_t count;
    unsigned int max_seq_len;
} LoraDataset;

/* Deterministic PRNG & helpers */
static uint32_t lora_xorshift32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 2463534242U;
    return *s;
}

static float lora_rand_sym(uint32_t *s, float bound) {
    uint32_t r = lora_xorshift32(s);
    float u = (float)(r & 0xFFFFFFU) / 16777216.0f;
    return (2.0f * u - 1.0f) * bound;
}

static uint64_t ggml_row_bytes(uint32_t wtype, unsigned int K) {
    if (wtype == ZCC_GGML_F32) return (uint64_t)K * 4ULL;
    if (wtype == ZCC_GGML_F16 || wtype == ZCC_GGML_BF16) return (uint64_t)K * 2ULL;
    if (wtype == ZCC_GGML_Q4_0) return (uint64_t)(K >> 5) * 18ULL;
    if (wtype == ZCC_GGML_Q8_0) return (uint64_t)(K >> 5) * 34ULL;
    if (wtype == ZCC_GGML_Q4_K) return (uint64_t)(K >> 8) * 144ULL;
    if (wtype == ZCC_GGML_Q6_K) return (uint64_t)(K >> 8) * 210ULL;
    return (uint64_t)K * 4ULL;
}

static void lora_fail_check(void) {
    if (g_zcc_llm_failed) {
#ifndef ZCC_GPU_LORA_LIBRARY
        fprintf(stderr, "FATAL: %s\n", g_zcc_llm_err[0] ? g_zcc_llm_err : "CUDA failure");
        exit(1);
#endif
    }
}

static void lora_gpu_zero(uint64_t dptr, unsigned int n) {
    if (n == 0U) return;
    void *p[2] = { &dptr, &n };
    check_cu(cuLaunchKernel(lfn_zero, (n + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, p, NULL), "lora_zero");
    lora_fail_check();
}

static void lora_gpu_add_out(uint64_t a, uint64_t b, uint64_t out, unsigned int n) {
    void *p[4] = { &a, &b, &out, &n };
    check_cu(cuLaunchKernel(lfn_add_out, (n + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, p, NULL), "lora_add_out");
    lora_fail_check();
}

static void lora_gpu_add_inplace(uint64_t dst, uint64_t src, unsigned int n) {
    void *p[3] = { &dst, &src, &n };
    check_cu(cuLaunchKernel(lfn_add_inplace, (n + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, p, NULL), "lora_add_inplace");
    lora_fail_check();
}

static void *resolve_dequant_fn(uint32_t wtype) {
    if (wtype == ZCC_GGML_BF16) return fn_dequant_bf16_to_f16;
    if (wtype == ZCC_GGML_Q8_0) return fn_dequant_q8_0_to_f16;
    if (wtype == ZCC_GGML_Q4_0) return fn_dequant_q4_0_to_f16;
    if (wtype == ZCC_GGML_Q4_K) return fn_dequant_q4_k_to_f16;
    if (wtype == ZCC_GGML_Q6_K) return fn_dequant_q6_k_to_f16;
    return fn_dequant_f32_to_f16;
}

static int can_use_tc_shape(unsigned int T_pad, unsigned int N, unsigned int K, int fp32_matmul) {
    if (fp32_matmul) return 0;
    return ((T_pad & 15U) == 0U) && ((N & 7U) == 0U) && ((K & 15U) == 0U);
}

static void lora_compute_amax_scale(const LoraTrainMem *tm, uint64_t d_src, unsigned int n) {
    uint64_t sb = tm->sq_blk, sp = tm->scale_pair;
    void *pa[3] = { &d_src, &sb, &n };
    check_cu(cuLaunchKernel(lfn_amax_blocks, 64U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "amax_blk");
    void *pf[2] = { &sb, &sp };
    check_cu(cuLaunchKernel(lfn_amax_finalize, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pf, NULL), "amax_fin");
    lora_fail_check();
}

static void lora_compute_amax_slice_scale(
    const LoraTrainMem *tm,
    uint64_t d_src,
    unsigned int src_stride,
    unsigned int T_real,
    unsigned int v0,
    unsigned int nc
) {
    uint64_t sb = tm->sq_blk, sp = tm->scale_pair;
    void *pa[6] = { &d_src, &src_stride, &T_real, &v0, &nc, &sb };
    check_cu(cuLaunchKernel(lfn_amax_slice_blocks, 64U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "amax_sl_blk");
    void *pf[2] = { &sb, &sp };
    check_cu(cuLaunchKernel(lfn_amax_finalize, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pf, NULL), "amax_sl_fin");
    lora_fail_check();
}

/* Y[T, N] = (add ? Y : 0) + X[T, K] @ W[N, K]^T (N <= 65535) */
static void lora_frozen_linear_fwd(
    const LoraTrainMem *tm,
    uint64_t d_w,
    uint32_t w_type,
    uint64_t d_x,
    uint64_t d_y,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    int use_tc = can_use_tc_shape(T_pad, N, K, tm->fp32_matmul);
    if (!use_tc) {
        if (w_type == ZCC_GGML_F32) {
            void *p[7] = { &d_x, &d_w, &d_y, &T_real, &N, &K, &add_flag };
            check_cu(cuLaunchKernel(lfn_mm_f32w_nt, N, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "mm_f32_nt");
            lora_fail_check();
            return;
        }
        uint64_t wf16 = (w_type == ZCC_GGML_F16) ? d_w : tm->w_f16;
        if (w_type != ZCC_GGML_F16) {
            void *df = resolve_dequant_fn(w_type);
            void *pdq[4] = { &d_w, &wf16, &N, &K };
            check_cu(cuLaunchKernel(df, (K + 255U) / 256U, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "dq_f16_fwd");
        }
        void *p[7] = { &d_x, &wf16, &d_y, &T_real, &N, &K, &add_flag };
        check_cu(cuLaunchKernel(lfn_mm_f16w_nt, N, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "mm_f16_nt");
        lora_fail_check();
        return;
    }

    lora_compute_amax_scale(tm, d_x, T_real * K);
    uint64_t xf16 = tm->x_f16, sp = tm->scale_pair;
    unsigned int tot_xf16 = T_pad * K;
    void *px[6] = { &d_x, &xf16, &T_real, &T_pad, &K, &sp };
    check_cu(cuLaunchKernel(lfn_f32_to_f16_pad, (tot_xf16 + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), "pad_xf16");

    uint64_t wf16 = (w_type == ZCC_GGML_F16) ? d_w : tm->w_f16;
    if (w_type != ZCC_GGML_F16) {
        void *df = resolve_dequant_fn(w_type);
        void *pdq[4] = { &d_w, &wf16, &N, &K };
        check_cu(cuLaunchKernel(df, (K + 255U) / 256U, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "dq_tc_fwd");
    }

    uint64_t dst = tm->tc_tmp;
    void *pg[6] = { &xf16, &wf16, &dst, &T_pad, &N, &K };
    check_cu(cuLaunchKernel(fn_gemm_tc, (N + 7U) / 8U, (T_pad + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), "tc_fwd");

    unsigned int tot_y = T_real * N;
    void *pcs[5] = { &d_y, &dst, &sp, &tot_y, &add_flag };
    check_cu(cuLaunchKernel(lfn_copy_scaled_f32, (tot_y + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcs, NULL), "tc_fwd_unscale");
    lora_fail_check();
}

/* dX[T, K] = (add ? dX : 0) + dY[T, N] @ W[N, K] (N <= 65535) */
static void lora_frozen_linear_bwd(
    const LoraTrainMem *tm,
    uint64_t d_w,
    uint32_t w_type,
    uint64_t d_dy,
    uint64_t d_dx,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int N,
    unsigned int K,
    unsigned int add_flag
) {
    /* Note Tensor Core computes A[T_pad, N] @ B[K, N]^T, so K is out-dim and N is inner K-dim! */
    int use_tc = (!tm->fp32_matmul) && ((T_pad & 15U) == 0U) && ((K & 7U) == 0U) && ((N & 15U) == 0U);
    if (!use_tc) {
        if (w_type == ZCC_GGML_F32) {
            void *p[7] = { &d_dy, &d_w, &d_dx, &T_real, &N, &K, &add_flag };
            check_cu(cuLaunchKernel(lfn_mm_f32w_tx, K, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "mm_f32_tx");
            lora_fail_check();
            return;
        }
        uint64_t wf16 = (w_type == ZCC_GGML_F16) ? d_w : tm->w_f16;
        if (w_type != ZCC_GGML_F16) {
            void *df = resolve_dequant_fn(w_type);
            void *pdq[4] = { &d_w, &wf16, &N, &K };
            check_cu(cuLaunchKernel(df, (K + 255U) / 256U, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "dq_f16_bwd");
        }
        void *p[7] = { &d_dy, &wf16, &d_dx, &T_real, &N, &K, &add_flag };
        check_cu(cuLaunchKernel(lfn_mm_f16w_tx, K, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "mm_f16_tx");
        lora_fail_check();
        return;
    }

    lora_compute_amax_scale(tm, d_dy, T_real * N);
    uint64_t dy16 = tm->x_f16, sp = tm->scale_pair;
    unsigned int tot_dy16 = T_pad * N;
    void *px[6] = { &d_dy, &dy16, &T_real, &T_pad, &N, &sp };
    check_cu(cuLaunchKernel(lfn_f32_to_f16_pad, (tot_dy16 + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), "pad_dy16");

    uint64_t wf16 = (w_type == ZCC_GGML_F16) ? d_w : tm->w_f16;
    if (w_type != ZCC_GGML_F16) {
        void *df = resolve_dequant_fn(w_type);
        void *pdq[4] = { &d_w, &wf16, &N, &K };
        check_cu(cuLaunchKernel(df, (K + 255U) / 256U, N, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "dq_tc_bwd");
    }

    uint64_t wf16_t = tm->w_f16_T;
    unsigned int tot_w = N * K;
    void *pt[4] = { &wf16, &wf16_t, &N, &K };
    check_cu(cuLaunchKernel(lfn_transpose_f16, (tot_w + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pt, NULL), "tr_wf16");

    uint64_t dst = tm->tc_tmp;
    void *pg[6] = { &dy16, &wf16_t, &dst, &T_pad, &K, &N };
    check_cu(cuLaunchKernel(fn_gemm_tc, (K + 7U) / 8U, (T_pad + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), "tc_bwd");

    unsigned int tot_dx = T_real * K;
    void *pcs[5] = { &d_dx, &dst, &sp, &tot_dx, &add_flag };
    check_cu(cuLaunchKernel(lfn_copy_scaled_f32, (tot_dx + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcs, NULL), "tc_bwd_unscale");
    lora_fail_check();
}

/* Chunked LM Head Forward: logits[T, V] = x[T, K] @ W[V, K]^T (each chunk N_c <= lm_chunk) */
static void lora_lmhead_fwd(
    const LoraTrainMem *tm,
    uint64_t d_wout,
    uint32_t w_type,
    uint64_t d_x,
    uint64_t d_logits,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int V,
    unsigned int K
) {
    uint64_t rbytes = ggml_row_bytes(w_type, K);
    int use_base_tc = (!tm->fp32_matmul) && ((T_pad & 15U) == 0U) && ((K & 15U) == 0U);
    if (use_base_tc) {
        lora_compute_amax_scale(tm, d_x, T_real * K);
        uint64_t xf16 = tm->x_f16, sp = tm->scale_pair;
        unsigned int tot_xf16 = T_pad * K;
        void *px[6] = { &d_x, &xf16, &T_real, &T_pad, &K, &sp };
        check_cu(cuLaunchKernel(lfn_f32_to_f16_pad, (tot_xf16 + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, px, NULL), "lm_pad_xf16");
    }

    for (unsigned int v0 = 0U; v0 < V; v0 += tm->lm_chunk) {
        unsigned int nc = V - v0;
        if (nc > tm->lm_chunk) nc = tm->lm_chunk;
        uint64_t w_ptr = d_wout + (uint64_t)v0 * rbytes;

        if (use_base_tc && ((nc & 7U) == 0U)) {
            uint64_t wf16 = (w_type == ZCC_GGML_F16) ? w_ptr : tm->w_f16;
            if (w_type != ZCC_GGML_F16) {
                void *df = resolve_dequant_fn(w_type);
                void *pdq[4] = { &w_ptr, &wf16, &nc, &K };
                check_cu(cuLaunchKernel(df, (K + 255U) / 256U, nc, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "lm_dq_fwd");
            }
            uint64_t xf16 = tm->x_f16, cf32 = tm->chunk_f32, sp = tm->scale_pair;
            void *pg[6] = { &xf16, &wf16, &cf32, &T_pad, &nc, &K };
            check_cu(cuLaunchKernel(fn_gemm_tc, (nc + 7U) / 8U, (T_pad + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), "lm_tc_fwd");

            unsigned int tot = T_real * nc;
            void *pcs[8] = { &cf32, &nc, &d_logits, &V, &T_real, &v0, &nc, &sp };
            check_cu(cuLaunchKernel(lfn_copy_cols_scaled_f32, (tot + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcs, NULL), "lm_copy_fwd");
        } else {
            uint64_t cf32 = tm->chunk_f32;
            unsigned int zero = 0U;
            if (w_type == ZCC_GGML_F32) {
                void *p[7] = { &d_x, &w_ptr, &cf32, &T_real, &nc, &K, &zero };
                check_cu(cuLaunchKernel(lfn_mm_f32w_nt, nc, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "lm_f32_fwd");
            } else {
                uint64_t wf16 = (w_type == ZCC_GGML_F16) ? w_ptr : tm->w_f16;
                if (w_type != ZCC_GGML_F16) {
                    void *df = resolve_dequant_fn(w_type);
                    void *pdq[4] = { &w_ptr, &wf16, &nc, &K };
                    check_cu(cuLaunchKernel(df, (K + 255U) / 256U, nc, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "lm_dq_f16_fwd");
                }
                void *p[7] = { &d_x, &wf16, &cf32, &T_real, &nc, &K, &zero };
                check_cu(cuLaunchKernel(lfn_mm_f16w_nt, nc, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "lm_f16_fwd");
            }
            unsigned int one = 1U;
            unsigned int tot = T_real * nc;
            void *pc[8] = { &cf32, &nc, &d_logits, &V, &T_real, &v0, &nc, &one };
            check_cu(cuLaunchKernel(lfn_copy_cols_f32, (tot + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pc, NULL), "lm_copy_f32_fwd");
        }
        lora_fail_check();
    }
}

/* Chunked LM Head Backward: d_x[T, K] = d_logits[T, V] @ W[V, K] */
static void lora_lmhead_bwd_dx(
    const LoraTrainMem *tm,
    uint64_t d_wout,
    uint32_t w_type,
    uint64_t d_dlogits,
    uint64_t d_dx,
    unsigned int T_real,
    unsigned int T_pad,
    unsigned int V,
    unsigned int K
) {
    uint64_t rbytes = ggml_row_bytes(w_type, K);
    lora_gpu_zero(d_dx, T_real * K);

    for (unsigned int v0 = 0U; v0 < V; v0 += tm->lm_chunk) {
        unsigned int nc = V - v0;
        if (nc > tm->lm_chunk) nc = tm->lm_chunk;
        uint64_t w_ptr = d_wout + (uint64_t)v0 * rbytes;
        int use_tc = (!tm->fp32_matmul) && ((T_pad & 15U) == 0U) && ((K & 7U) == 0U) && ((nc & 15U) == 0U);

        if (use_tc) {
            lora_compute_amax_slice_scale(tm, d_dlogits, V, T_real, v0, nc);
            uint64_t dy16 = tm->x_f16, sp = tm->scale_pair;
            unsigned int tot_dy = T_pad * nc;
            void *psl[8] = { &d_dlogits, &V, &dy16, &T_real, &T_pad, &v0, &nc, &sp };
            check_cu(cuLaunchKernel(lfn_slice_f32_to_f16, (tot_dy + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psl, NULL), "lm_slice_dy");

            uint64_t wf16 = (w_type == ZCC_GGML_F16) ? w_ptr : tm->w_f16;
            if (w_type != ZCC_GGML_F16) {
                void *df = resolve_dequant_fn(w_type);
                void *pdq[4] = { &w_ptr, &wf16, &nc, &K };
                check_cu(cuLaunchKernel(df, (K + 255U) / 256U, nc, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "lm_dq_bwd");
            }
            uint64_t wf16_t = tm->w_f16_T;
            unsigned int tot_w = nc * K;
            void *pt[4] = { &wf16, &wf16_t, &nc, &K };
            check_cu(cuLaunchKernel(lfn_transpose_f16, (tot_w + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pt, NULL), "lm_tr_bwd");

            uint64_t tmp_dx = tm->tc_tmp;
            void *pg[6] = { &dy16, &wf16_t, &tmp_dx, &T_pad, &K, &nc };
            check_cu(cuLaunchKernel(fn_gemm_tc, (K + 7U) / 8U, (T_pad + 15U) / 16U, 1U, 32U, 1U, 1U, 0U, NULL, pg, NULL), "lm_tc_bwd");

            unsigned int one = 1U, tot_dx = T_real * K;
            void *pcs[5] = { &d_dx, &tmp_dx, &sp, &tot_dx, &one };
            check_cu(cuLaunchKernel(lfn_copy_scaled_f32, (tot_dx + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pcs, NULL), "lm_tc_bwd_unscale");
        } else {
            uint64_t cf32 = tm->chunk_f32;
            unsigned int zero = 0U, one = 1U;
            unsigned int tot = T_real * nc;
            void *pc[8] = { &d_dlogits, &V, &cf32, &nc, &T_real, &v0, &nc, &zero };
            check_cu(cuLaunchKernel(lfn_copy_cols_f32, (tot + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pc, NULL), "lm_slice_f32");

            if (w_type == ZCC_GGML_F32) {
                void *p[7] = { &cf32, &w_ptr, &d_dx, &T_real, &nc, &K, &one };
                check_cu(cuLaunchKernel(lfn_mm_f32w_tx, K, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "lm_f32_bwd");
            } else {
                uint64_t wf16 = (w_type == ZCC_GGML_F16) ? w_ptr : tm->w_f16;
                if (w_type != ZCC_GGML_F16) {
                    void *df = resolve_dequant_fn(w_type);
                    void *pdq[4] = { &w_ptr, &wf16, &nc, &K };
                    check_cu(cuLaunchKernel(df, (K + 255U) / 256U, nc, 1U, 256U, 1U, 1U, 0U, NULL, pdq, NULL), "lm_dq_f16_bwd");
                }
                void *p[7] = { &cf32, &wf16, &d_dx, &T_real, &nc, &K, &one };
                check_cu(cuLaunchKernel(lfn_mm_f16w_tx, K, T_real, 1U, 32U, 1U, 1U, 0U, NULL, p, NULL), "lm_f16_bwd");
            }
        }
        lora_fail_check();
    }
}

/* LoRA Adapter Forward & Backward Helpers */
static void lora_proj_fwd(const LoraProj *lp, uint64_t d_x, uint64_t d_h, uint64_t d_y, unsigned int T, float scale) {
    unsigned int r = lp->r, N = lp->N_out, K = lp->K_in;
    uint64_t dA = lp->d_A, dB = lp->d_B;
    void *pa[6] = { &d_x, &dA, &d_h, &T, &r, &K };
    check_cu(cuLaunchKernel(lfn_fwd_a, r, T, 1U, 32U, 1U, 1U, 0U, NULL, pa, NULL), "lora_fwd_a");
    void *pb[7] = { &d_h, &dB, &d_y, &T, &N, &r, &scale };
    check_cu(cuLaunchKernel(lfn_fwd_b_add, (N + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, pb, NULL), "lora_fwd_b");
    lora_fail_check();
}

static void lora_proj_bwd(
    const LoraProj *lp,
    uint64_t d_x,
    uint64_t d_h,
    uint64_t d_dy,
    uint64_t d_dx,
    uint64_t d_dh_tmp,
    unsigned int T,
    float scale
) {
    unsigned int r = lp->r, N = lp->N_out, K = lp->K_in;
    uint64_t dA = lp->d_A, dB = lp->d_B, d_dA = lp->d_dA, d_dB = lp->d_dB;
    void *pdh[7] = { &d_dy, &dB, &d_dh_tmp, &T, &N, &r, &scale };
    check_cu(cuLaunchKernel(lfn_bwd_dh, r, T, 1U, 32U, 1U, 1U, 0U, NULL, pdh, NULL), "lora_bwd_dh");
    void *pdb[7] = { &d_dy, &d_h, &d_dB, &T, &N, &r, &scale };
    check_cu(cuLaunchKernel(lfn_bwd_db, N, r, 1U, 32U, 1U, 1U, 0U, NULL, pdb, NULL), "lora_bwd_db");
    void *pda[6] = { &d_dh_tmp, &d_x, &d_dA, &T, &r, &K };
    check_cu(cuLaunchKernel(lfn_bwd_da, (K + 255U) / 256U, r, 1U, 256U, 1U, 1U, 0U, NULL, pda, NULL), "lora_bwd_da");
    void *pdx[6] = { &d_dh_tmp, &dA, &d_dx, &T, &r, &K };
    check_cu(cuLaunchKernel(lfn_bwd_dx, (K + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, pdx, NULL), "lora_bwd_dx");
    lora_fail_check();
}


static int alloc_lora_proj(LoraProj *lp, unsigned int N_out, unsigned int K_in, unsigned int r,
                           int train_mem, int nonzero_b, uint32_t *rng) {
    memset(lp, 0, sizeof(*lp));
    lp->N_out = N_out;
    lp->K_in = K_in;
    lp->r = r;
    size_t szA = (size_t)r * (size_t)K_in * 4U;
    size_t szB = (size_t)N_out * (size_t)r * 4U;
    check_cu(cuMemAlloc_v2(&lp->d_A, szA), "lora.A");
    check_cu(cuMemAlloc_v2(&lp->d_B, szB), "lora.B");
    float *hA = (float *)malloc(szA);
    float *hB = (float *)malloc(szB);
    if (!hA || !hB) { fprintf(stderr, "OOM initializing LoRA host weights\n"); exit(1); }
    float boundA = (float)sqrt(3.0 / (double)K_in);
    for (size_t i = 0U; i < (size_t)r * (size_t)K_in; i++) hA[i] = lora_rand_sym(rng, nonzero_b ? 0.03f : boundA);
    if (nonzero_b) {
        for (size_t i = 0U; i < (size_t)N_out * (size_t)r; i++) hB[i] = lora_rand_sym(rng, 0.03f);
    } else {
        memset(hB, 0, szB);
    }
    check_cu(cuMemcpyHtoD_v2(lp->d_A, hA, szA), "HtoD(lora.A)");
    check_cu(cuMemcpyHtoD_v2(lp->d_B, hB, szB), "HtoD(lora.B)");
    free(hA); free(hB);
    if (train_mem) {
        check_cu(cuMemAlloc_v2(&lp->d_dA, szA), "lora.dA");
        check_cu(cuMemAlloc_v2(&lp->d_dB, szB), "lora.dB");
        check_cu(cuMemAlloc_v2(&lp->d_mA, szA), "lora.mA");
        check_cu(cuMemAlloc_v2(&lp->d_vA, szA), "lora.vA");
        check_cu(cuMemAlloc_v2(&lp->d_mB, szB), "lora.mB");
        check_cu(cuMemAlloc_v2(&lp->d_vB, szB), "lora.vB");
        check_cu(cuMemsetD8_v2(lp->d_mA, 0, szA), "zero(mA)");
        check_cu(cuMemsetD8_v2(lp->d_vA, 0, szA), "zero(vA)");
        check_cu(cuMemsetD8_v2(lp->d_mB, 0, szB), "zero(mB)");
        check_cu(cuMemsetD8_v2(lp->d_vB, 0, szB), "zero(vB)");
    }
    lora_fail_check();
    return 0;
}

static void free_lora_proj(LoraProj *lp) {
    if (lp->d_A) cuMemFree_v2(lp->d_A);
    if (lp->d_B) cuMemFree_v2(lp->d_B);
    if (lp->d_dA) cuMemFree_v2(lp->d_dA);
    if (lp->d_dB) cuMemFree_v2(lp->d_dB);
    if (lp->d_mA) cuMemFree_v2(lp->d_mA);
    if (lp->d_vA) cuMemFree_v2(lp->d_vA);
    if (lp->d_mB) cuMemFree_v2(lp->d_mB);
    if (lp->d_vB) cuMemFree_v2(lp->d_vB);
    memset(lp, 0, sizeof(*lp));
}

static LoraLayer *alloc_lora_layers(const LlamaModel *m, unsigned int rank, int train_mem, int nonzero_b, uint32_t seed) {
    LoraLayer *ls = (LoraLayer *)calloc((size_t)m->n_layers, sizeof(LoraLayer));
    if (!ls) { fprintf(stderr, "OOM allocating LoraLayer array\n"); exit(1); }
    uint32_t rng = seed ? seed : 20261123U;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        alloc_lora_proj(&ls[l].q, m->dim, m->dim, rank, train_mem, nonzero_b, &rng);
        alloc_lora_proj(&ls[l].k, m->kv_dim, m->dim, rank, train_mem, nonzero_b, &rng);
        alloc_lora_proj(&ls[l].v, m->kv_dim, m->dim, rank, train_mem, nonzero_b, &rng);
        alloc_lora_proj(&ls[l].o, m->dim, m->dim, rank, train_mem, nonzero_b, &rng);
    }
    return ls;
}

static void free_lora_layers(LoraLayer *ls, unsigned int n_layers) {
    if (!ls) return;
    for (unsigned int l = 0U; l < n_layers; l++) {
        free_lora_proj(&ls[l].q);
        free_lora_proj(&ls[l].k);
        free_lora_proj(&ls[l].v);
        free_lora_proj(&ls[l].o);
    }
    free(ls);
}

static void free_lora_proj_train_mem(LoraProj *lp) {
    if (!lp) return;
    if (lp->d_dA) { cuMemFree_v2(lp->d_dA); lp->d_dA = 0ULL; }
    if (lp->d_dB) { cuMemFree_v2(lp->d_dB); lp->d_dB = 0ULL; }
    if (lp->d_mA) { cuMemFree_v2(lp->d_mA); lp->d_mA = 0ULL; }
    if (lp->d_vA) { cuMemFree_v2(lp->d_vA); lp->d_vA = 0ULL; }
    if (lp->d_mB) { cuMemFree_v2(lp->d_mB); lp->d_mB = 0ULL; }
    if (lp->d_vB) { cuMemFree_v2(lp->d_vB); lp->d_vB = 0ULL; }
}

static void free_lora_layers_train_mem(LoraLayer *ls, unsigned int n_layers) {
    if (!ls) return;
    for (unsigned int l = 0U; l < n_layers; l++) {
        free_lora_proj_train_mem(&ls[l].q);
        free_lora_proj_train_mem(&ls[l].k);
        free_lora_proj_train_mem(&ls[l].v);
        free_lora_proj_train_mem(&ls[l].o);
    }
}

static uint32_t g_last_lora_fnv = 0U;

static uint64_t compute_model_struct_fp(const LlamaModel *m) {
    char spec[256];
    snprintf(spec, sizeof(spec), "%s:%u:%u:%u:%u:%u:%u",
             m->arch, m->n_layers, m->dim, m->hidden_dim, m->n_heads, m->n_kv_heads, m->vocab_size);
    return fnv1a_span64(spec, strlen(spec));
}

static uint64_t lora_payload_byte_size(const LlamaModel *m, unsigned int r) {
    uint64_t per_layer_elems =
        (uint64_t)r * m->dim + (uint64_t)m->dim * r +
        (uint64_t)r * m->dim + (uint64_t)m->kv_dim * r +
        (uint64_t)r * m->dim + (uint64_t)m->kv_dim * r +
        (uint64_t)r * m->dim + (uint64_t)m->dim * r;
    return (uint64_t)m->n_layers * per_layer_elems * 4ULL;
}

static void put_le32_b(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void put_le64_b(uint8_t *p, uint64_t v) {
    put_le32_b(p, (uint32_t)(v & 0xFFFFFFFFULL));
    put_le32_b(p + 4, (uint32_t)(v >> 32));
}

static uint8_t *alloc_lora_payload_bytes(
    const LlamaModel *m,
    const LoraLayer *ls,
    unsigned int rank,
    uint64_t *out_pbytes,
    uint32_t *out_fnv
) {
    uint64_t pbytes = lora_payload_byte_size(m, rank);
    uint8_t *payload = (uint8_t *)malloc((size_t)pbytes);
    if (!payload) {
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_IO_ERROR: OOM allocating LoRA payload");
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    size_t off = 0U;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            size_t szA = (size_t)ps[p]->r * (size_t)ps[p]->K_in * 4U;
            size_t szB = (size_t)ps[p]->N_out * (size_t)ps[p]->r * 4U;
            check_cu(cuMemcpyDtoH_v2(payload + off, ps[p]->d_A, szA), "save_DtoH_A");
            off += szA;
            check_cu(cuMemcpyDtoH_v2(payload + off, ps[p]->d_B, szB), "save_DtoH_B");
            off += szB;
        }
    }
    lora_fail_check();
    if (g_zcc_llm_failed) {
        free(payload);
        return NULL;
    }
    if (out_pbytes) *out_pbytes = pbytes;
    if (out_fnv) *out_fnv = fnv1a_bytes32(payload, (size_t)pbytes);
    return payload;
}

static uint32_t compute_lora_layers_fnv(const LlamaModel *m, const LoraLayer *ls, unsigned int rank) {
    uint64_t pbytes = 0ULL;
    uint32_t fnv = 0U;
    uint8_t *payload = alloc_lora_payload_bytes(m, ls, rank, &pbytes, &fnv);
    if (!payload) return 0U;
    free(payload);
    return fnv;
}

static int save_lora_adapter(const char *path, const LlamaModel *m, const LoraLayer *ls, unsigned int rank, float alpha) {
    uint64_t pbytes = 0ULL;
    uint32_t p_fnv = 0U;
    uint8_t *payload = alloc_lora_payload_bytes(m, ls, rank, &pbytes, &p_fnv);
    if (!payload) return -1;

    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "ZCCLORA1", 8U);
    uint32_t alpha_bits = 0U;
    memcpy(&alpha_bits, &alpha, 4U);
    uint64_t mfp = compute_model_struct_fp(m);

    put_le32_b(hdr + 8, 1U);
    put_le32_b(hdr + 12, rank);
    put_le32_b(hdr + 16, alpha_bits);
    put_le32_b(hdr + 20, m->n_layers);
    put_le32_b(hdr + 24, m->dim);
    put_le32_b(hdr + 28, m->kv_dim);
    put_le32_b(hdr + 32, p_fnv);
    put_le64_b(hdr + 36, mfp);
    put_le64_b(hdr + 44, pbytes);
    uint32_t h_fnv = fnv1a_bytes32(hdr, 60U);
    put_le32_b(hdr + 60, h_fnv);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_IO_ERROR: cannot create '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        free(payload);
        return -1;
    }
    size_t w1 = fwrite(hdr, 1U, 64U, fp);
    size_t w2 = fwrite(payload, 1U, (size_t)pbytes, fp);
    fclose(fp);
    free(payload);
    if (w1 != 64U || w2 != (size_t)pbytes) {
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_IO_ERROR: short write to '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return -1;
    }
    g_last_lora_fnv = p_fnv;
    printf("LORA_SAVED path=\"%s\" bytes=%llu rank=%u alpha=%.2f fnv=%08x\n",
           path, (unsigned long long)(64ULL + pbytes), rank, (double)alpha, p_fnv);
    return 0;
}

static LoraLayer *load_lora_adapter(const char *path, const LlamaModel *m, unsigned int *out_rank, float *out_alpha) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_IO_ERROR: cannot open '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long fsz_l = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsz_l < 64L) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_TRUNCATED: file '%s' smaller than 64B header", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    uint8_t hdr[64];
    if (fread(hdr, 1U, 64U, fp) != 64U) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_TRUNCATED: cannot read header from '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    if (memcmp(hdr, "ZCCLORA1", 8U) != 0) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_MAGIC_ERROR: invalid ZCCLORA1 magic in '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    uint32_t calc_hfnv = fnv1a_bytes32(hdr, 60U);
    uint32_t stored_hfnv = read_le32(hdr + 60);
    if (calc_hfnv != stored_hfnv) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_CHECKSUM_MISMATCH: header FNV mismatch in '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    uint32_t ver = read_le32(hdr + 8);
    uint32_t rank = read_le32(hdr + 12);
    uint32_t alpha_bits = read_le32(hdr + 16);
    float alpha = 16.0f;
    memcpy(&alpha, &alpha_bits, 4U);
    uint32_t nl = read_le32(hdr + 20);
    uint32_t dim = read_le32(hdr + 24);
    uint32_t kvd = read_le32(hdr + 28);
    uint32_t p_fnv = read_le32(hdr + 32);
    uint64_t mfp = read_le64(hdr + 36);
    uint64_t pbytes = read_le64(hdr + 44);

    if (ver != 1U || rank == 0U || rank > 64U) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_MAGIC_ERROR: invalid version=%u or rank=%u", ver, rank);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    if (nl != m->n_layers || dim != m->dim || kvd != m->kv_dim || mfp != compute_model_struct_fp(m)) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err),
                 "LORA_ARCH_MISMATCH: adapter (%ux%ux%u fp=%08x) != model (%ux%ux%u fp=%08x)",
                 nl, dim, kvd, (unsigned int)mfp, m->n_layers, m->dim, m->kv_dim,
                 (unsigned int)compute_model_struct_fp(m));
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    uint64_t exp_bytes = lora_payload_byte_size(m, rank);
    if (pbytes != exp_bytes || (uint64_t)fsz_l != 64ULL + exp_bytes) {
        fclose(fp);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err),
                 "LORA_TRUNCATED: expected %lluB total, got %ldB",
                 (unsigned long long)(64ULL + exp_bytes), fsz_l);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    uint8_t *payload = (uint8_t *)malloc((size_t)exp_bytes);
    if (!payload || fread(payload, 1U, (size_t)exp_bytes, fp) != (size_t)exp_bytes) {
        fclose(fp);
        if (payload) free(payload);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_TRUNCATED: short payload read from '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }
    fclose(fp);

    if (fnv1a_bytes32(payload, (size_t)exp_bytes) != p_fnv) {
        free(payload);
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_CHECKSUM_MISMATCH: payload FNV-1a mismatch in '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return NULL;
    }

    LoraLayer *ls = alloc_lora_layers(m, rank, 0, 0, 1U);
    size_t off = 0U;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            size_t szA = (size_t)ps[p]->r * (size_t)ps[p]->K_in * 4U;
            size_t szB = (size_t)ps[p]->N_out * (size_t)ps[p]->r * 4U;
            check_cu(cuMemcpyHtoD_v2(ps[p]->d_A, payload + off, szA), "load_HtoD_A");
            off += szA;
            check_cu(cuMemcpyHtoD_v2(ps[p]->d_B, payload + off, szB), "load_HtoD_B");
            off += szB;
        }
    }
    free(payload);
    lora_fail_check();
    if (g_zcc_llm_failed) {
        free_lora_layers(ls, m->n_layers);
        return NULL;
    }
    g_last_lora_fnv = p_fnv;
    *out_rank = rank;
    *out_alpha = alpha;
    printf("LORA_LOADED path=\"%s\" rank=%u alpha=%.2f params=%llu fnv=%08x\n",
           path, rank, (double)alpha, (unsigned long long)(exp_bytes / 4ULL), p_fnv);
    return ls;
}

/* SFT Dataset Parsing (CRLF-safe Q:/A: blocks -> masked prompt + assistant targets) */
static int parse_lora_dataset_buf(const char *raw, size_t raw_len, const LlamaModel *m, int use_chatml, LoraDataset *ds) {
    memset(ds, 0, sizeof(*ds));
    if (!raw || raw_len == 0U) return -1;

    char *norm = (char *)malloc(raw_len + 2U);
    if (!norm) return -1;
    size_t nlen = 0U;
    for (size_t i = 0U; i < raw_len; i++) {
        if (raw[i] != '\r') norm[nlen++] = raw[i];
    }
    norm[nlen] = '\0';

    size_t cap = 16U;
    ds->items = (LoraExample *)calloc(cap, sizeof(LoraExample));
    int has_im = (tok_lookup(&m->tok, "<|im_start|>", 12U) >= 0);
    int chat_wrap = use_chatml || has_im;

    char *cur_q = NULL;
    char *cur_a = NULL;
    int in_q = 0, in_a = 0;

    size_t pos = 0U;
    while (pos <= nlen) {
        size_t le = pos;
        while (le < nlen && norm[le] != '\n') le++;
        size_t line_len = le - pos;
        char *line = norm + pos;
        int is_eof = (pos >= nlen);
        pos = (le < nlen) ? (le + 1U) : (nlen + 1U);

        int is_q = (!is_eof && line_len >= 2U && line[0] == 'Q' && line[1] == ':');
        int is_a = (!is_eof && line_len >= 2U && line[0] == 'A' && line[1] == ':');
        int is_blank = (!is_eof && line_len == 0U);

        if ((is_q || is_blank || is_eof) && cur_q && cur_a) {
            char *p_str = NULL;
            char *a_str = NULL;
            if (chat_wrap) {
                p_str = format_chat_prompt_alloc(m, cur_q, NULL, 0);
                size_t al = strlen(cur_a) + 32U;
                a_str = (char *)malloc(al);
                if (a_str) snprintf(a_str, al, "%s%s", cur_a, has_im ? "<|im_end|>" : "</s>");
            } else {
                size_t pl = strlen(cur_q) + 16U;
                p_str = (char *)malloc(pl);
                if (p_str) snprintf(p_str, pl, "Q: %s\nA: ", cur_q);
                size_t al = strlen(cur_a) + 8U;
                a_str = (char *)malloc(al);
                if (a_str) snprintf(a_str, al, "%s\n", cur_a);
            }

            uint32_t *p_ids = NULL, p_cnt = 0U;
            uint32_t *a_ids = NULL, a_cnt = 0U;
            if (p_str && a_str &&
                tokenize_prompt(m, p_str, 0, &p_ids, &p_cnt) == 0 && p_cnt >= 1U &&
                tokenize_prompt(m, a_str, 1, &a_ids, &a_cnt) == 0 && a_cnt >= 1U) {
                uint32_t full_len = p_cnt + a_cnt;
                uint32_t T = full_len - 1U;
                if (ds->count == cap) {
                    cap *= 2U;
                    ds->items = (LoraExample *)realloc(ds->items, cap * sizeof(LoraExample));
                }
                LoraExample *ex = &ds->items[ds->count++];
                ex->T = T;
                ex->pref_len = p_cnt;
                ex->toks = (uint32_t *)malloc((size_t)T * sizeof(uint32_t));
                ex->targets = (uint32_t *)malloc((size_t)T * sizeof(uint32_t));
                ex->active_cnt = a_cnt;
                for (uint32_t t = 0U; t < T; t++) {
                    uint32_t cur_id = (t < p_cnt) ? p_ids[t] : a_ids[t - p_cnt];
                    uint32_t nxt_id = ((t + 1U) < p_cnt) ? p_ids[t + 1U] : a_ids[(t + 1U) - p_cnt];
                    if (cur_id >= m->vocab_size) cur_id = 0U;
                    if (nxt_id >= m->vocab_size) nxt_id = 0U;
                    ex->toks[t] = cur_id;
                    ex->targets[t] = (t < p_cnt - 1U) ? 0xFFFFFFFFU : nxt_id;
                }
                if (T > ds->max_seq_len) ds->max_seq_len = T;
            }
            if (p_ids) free(p_ids);
            if (a_ids) free(a_ids);
            if (p_str) free(p_str);
            if (a_str) free(a_str);
            free(cur_q); free(cur_a);
            cur_q = NULL; cur_a = NULL; in_q = 0; in_a = 0;
        }

        if (is_eof) break;
        if (is_q) {
            const char *s = line + 2;
            size_t sl = line_len - 2U;
            if (sl > 0U && *s == ' ') { s++; sl--; }
            if (cur_q) free(cur_q);
            cur_q = (char *)malloc(sl + 1U);
            memcpy(cur_q, s, sl); cur_q[sl] = '\0';
            in_q = 1; in_a = 0;
        } else if (is_a) {
            const char *s = line + 2;
            size_t sl = line_len - 2U;
            if (sl > 0U && *s == ' ') { s++; sl--; }
            if (cur_a) free(cur_a);
            cur_a = (char *)malloc(sl + 1U);
            memcpy(cur_a, s, sl); cur_a[sl] = '\0';
            in_q = 0; in_a = 1;
        } else if (!is_blank) {
            if (in_a && cur_a) {
                size_t ol = strlen(cur_a);
                cur_a = (char *)realloc(cur_a, ol + line_len + 2U);
                cur_a[ol] = '\n';
                memcpy(cur_a + ol + 1U, line, line_len);
                cur_a[ol + 1U + line_len] = '\0';
            } else if (in_q && cur_q) {
                size_t ol = strlen(cur_q);
                cur_q = (char *)realloc(cur_q, ol + line_len + 2U);
                cur_q[ol] = '\n';
                memcpy(cur_q + ol + 1U, line, line_len);
                cur_q[ol + 1U + line_len] = '\0';
            }
        }
    }

    if (cur_q) free(cur_q);
    if (cur_a) free(cur_a);
    free(norm);
    return (ds->count > 0U) ? 0 : -1;
}

static int parse_lora_dataset(const char *path, const LlamaModel *m, int use_chatml, LoraDataset *ds) {
    memset(ds, 0, sizeof(*ds));
    size_t raw_len = 0U;
    char *raw = read_text_file_alloc(path, &raw_len);
    if (!raw) {
        snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "LORA_IO_ERROR: cannot read dataset '%s'", path);
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        return -1;
    }
    int rc = parse_lora_dataset_buf(raw, raw_len, m, use_chatml, ds);
    free(raw);
    return rc;
}

static void free_lora_dataset(LoraDataset *ds) {
    if (!ds) return;
    for (size_t i = 0U; i < ds->count; i++) {
        if (ds->items[i].toks) free(ds->items[i].toks);
        if (ds->items[i].targets) free(ds->items[i].targets);
    }
    if (ds->items) free(ds->items);
    memset(ds, 0, sizeof(*ds));
}

static LoraTrainMem *alloc_lora_train_mem(const LlamaModel *m, unsigned int max_T, unsigned int rank, int fp32_matmul) {
    LoraTrainMem *tm = (LoraTrainMem *)calloc(1U, sizeof(LoraTrainMem));
    if (!tm) exit(1);
    tm->max_T = max_T;
    tm->max_T_pad = ((max_T + 15U) / 16U) * 16U;
    tm->lm_chunk = 4096U;
    tm->fp32_matmul = fp32_matmul;
    tm->layers = (LoraLayerAct *)calloc((size_t)m->n_layers, sizeof(LoraLayerAct));
    if (!tm->layers) exit(1);

    size_t Tp = (size_t)tm->max_T_pad;
    size_t T = (size_t)max_T;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        LoraLayerAct *la = &tm->layers[l];
        check_cu(cuMemAlloc_v2(&la->x_in, T * m->dim * 4U), "la.x_in");
        check_cu(cuMemAlloc_v2(&la->x_norm1, T * m->dim * 4U), "la.x_norm1");
        check_cu(cuMemAlloc_v2(&la->q, T * m->dim * 4U), "la.q");
        check_cu(cuMemAlloc_v2(&la->k, T * m->kv_dim * 4U), "la.k");
        check_cu(cuMemAlloc_v2(&la->v, T * m->kv_dim * 4U), "la.v");
        check_cu(cuMemAlloc_v2(&la->h_q, T * rank * 4U), "la.h_q");
        check_cu(cuMemAlloc_v2(&la->h_k, T * rank * 4U), "la.h_k");
        check_cu(cuMemAlloc_v2(&la->h_v, T * rank * 4U), "la.h_v");
        check_cu(cuMemAlloc_v2(&la->h_o, T * rank * 4U), "la.h_o");
        check_cu(cuMemAlloc_v2(&la->attn_probs, (size_t)m->n_heads * T * T * 4U), "la.attn_probs");
        check_cu(cuMemAlloc_v2(&la->attn_ctx, T * m->dim * 4U), "la.attn_ctx");
        check_cu(cuMemAlloc_v2(&la->attn_out, T * m->dim * 4U), "la.attn_out");
        check_cu(cuMemAlloc_v2(&la->x_mid, T * m->dim * 4U), "la.x_mid");
        check_cu(cuMemAlloc_v2(&la->x_norm2, T * m->dim * 4U), "la.x_norm2");
        check_cu(cuMemAlloc_v2(&la->gate_pre, T * m->hidden_dim * 4U), "la.gate_pre");
        check_cu(cuMemAlloc_v2(&la->up_pre, T * m->hidden_dim * 4U), "la.up_pre");
    }

    size_t max_w_elems = (size_t)m->dim * (size_t)m->dim;
    if ((size_t)m->hidden_dim * (size_t)m->dim > max_w_elems) max_w_elems = (size_t)m->hidden_dim * (size_t)m->dim;
    if ((size_t)tm->lm_chunk * (size_t)m->dim > max_w_elems) max_w_elems = (size_t)tm->lm_chunk * (size_t)m->dim;

    size_t max_out = m->hidden_dim;
    if (m->dim > max_out) max_out = m->dim;
    if (tm->lm_chunk > max_out) max_out = tm->lm_chunk;

    size_t max_in = m->hidden_dim;
    if (m->dim > max_in) max_in = m->dim;
    if (tm->lm_chunk > max_in) max_in = tm->lm_chunk;

    check_cu(cuMemAlloc_v2(&tm->x_final, T * m->dim * 4U), "tm.x_final");
    check_cu(cuMemAlloc_v2(&tm->x_final_norm, T * m->dim * 4U), "tm.x_final_norm");
    check_cu(cuMemAlloc_v2(&tm->logits, T * m->vocab_size * 4U), "tm.logits");
    check_cu(cuMemAlloc_v2(&tm->dlogits, T * m->vocab_size * 4U), "tm.dlogits");
    check_cu(cuMemAlloc_v2(&tm->losses, T * 4U), "tm.losses");
    check_cu(cuMemAlloc_v2(&tm->targets, T * 4U), "tm.targets");
    check_cu(cuMemAlloc_v2(&tm->x_f16, Tp * max_in * 2U), "tm.x_f16");
    check_cu(cuMemAlloc_v2(&tm->w_f16, max_w_elems * 2U), "tm.w_f16");
    check_cu(cuMemAlloc_v2(&tm->w_f16_T, max_w_elems * 2U), "tm.w_f16_T");
    check_cu(cuMemAlloc_v2(&tm->tc_tmp, Tp * max_out * 4U), "tm.tc_tmp");
    check_cu(cuMemAlloc_v2(&tm->chunk_f32, Tp * tm->lm_chunk * 4U), "tm.chunk_f32");
    check_cu(cuMemAlloc_v2(&tm->ffn_act, T * m->hidden_dim * 4U), "tm.ffn_act");
    check_cu(cuMemAlloc_v2(&tm->dx_cur, T * m->dim * 4U), "tm.dx_cur");
    check_cu(cuMemAlloc_v2(&tm->dx_mid, T * m->dim * 4U), "tm.dx_mid");
    check_cu(cuMemAlloc_v2(&tm->dx_norm, T * m->dim * 4U), "tm.dx_norm");
    check_cu(cuMemAlloc_v2(&tm->dffn_act, T * m->hidden_dim * 4U), "tm.dffn_act");
    check_cu(cuMemAlloc_v2(&tm->dgate, T * m->hidden_dim * 4U), "tm.dgate");
    check_cu(cuMemAlloc_v2(&tm->dup, T * m->hidden_dim * 4U), "tm.dup");
    check_cu(cuMemAlloc_v2(&tm->dattn_ctx, T * m->dim * 4U), "tm.dattn_ctx");
    check_cu(cuMemAlloc_v2(&tm->attn_ds, (size_t)m->n_heads * T * T * 4U), "tm.attn_ds");
    check_cu(cuMemAlloc_v2(&tm->dq, T * m->dim * 4U), "tm.dq");
    check_cu(cuMemAlloc_v2(&tm->dk, T * m->kv_dim * 4U), "tm.dk");
    check_cu(cuMemAlloc_v2(&tm->dv, T * m->kv_dim * 4U), "tm.dv");
    check_cu(cuMemAlloc_v2(&tm->dh_tmp, T * rank * 4U), "tm.dh_tmp");
    check_cu(cuMemAlloc_v2(&tm->sq_blk, 256U * 4U), "tm.sq_blk");
    check_cu(cuMemAlloc_v2(&tm->scale_pair, 2U * 4U), "tm.scale_pair");
    lora_fail_check();
    return tm;
}

static void free_lora_train_mem(LoraTrainMem *tm, unsigned int n_layers) {
    if (!tm) return;
    if (tm->layers) {
        for (unsigned int l = 0U; l < n_layers; l++) {
            LoraLayerAct *la = &tm->layers[l];
            if (la->x_in) cuMemFree_v2(la->x_in);
            if (la->x_norm1) cuMemFree_v2(la->x_norm1);
            if (la->q) cuMemFree_v2(la->q);
            if (la->k) cuMemFree_v2(la->k);
            if (la->v) cuMemFree_v2(la->v);
            if (la->h_q) cuMemFree_v2(la->h_q);
            if (la->h_k) cuMemFree_v2(la->h_k);
            if (la->h_v) cuMemFree_v2(la->h_v);
            if (la->h_o) cuMemFree_v2(la->h_o);
            if (la->attn_probs) cuMemFree_v2(la->attn_probs);
            if (la->attn_ctx) cuMemFree_v2(la->attn_ctx);
            if (la->attn_out) cuMemFree_v2(la->attn_out);
            if (la->x_mid) cuMemFree_v2(la->x_mid);
            if (la->x_norm2) cuMemFree_v2(la->x_norm2);
            if (la->gate_pre) cuMemFree_v2(la->gate_pre);
            if (la->up_pre) cuMemFree_v2(la->up_pre);
        }
        free(tm->layers);
    }
    if (tm->x_final) cuMemFree_v2(tm->x_final);
    if (tm->x_final_norm) cuMemFree_v2(tm->x_final_norm);
    if (tm->logits) cuMemFree_v2(tm->logits);
    if (tm->dlogits) cuMemFree_v2(tm->dlogits);
    if (tm->losses) cuMemFree_v2(tm->losses);
    if (tm->targets) cuMemFree_v2(tm->targets);
    if (tm->x_f16) cuMemFree_v2(tm->x_f16);
    if (tm->w_f16) cuMemFree_v2(tm->w_f16);
    if (tm->w_f16_T) cuMemFree_v2(tm->w_f16_T);
    if (tm->tc_tmp) cuMemFree_v2(tm->tc_tmp);
    if (tm->chunk_f32) cuMemFree_v2(tm->chunk_f32);
    if (tm->ffn_act) cuMemFree_v2(tm->ffn_act);
    if (tm->dx_cur) cuMemFree_v2(tm->dx_cur);
    if (tm->dx_mid) cuMemFree_v2(tm->dx_mid);
    if (tm->dx_norm) cuMemFree_v2(tm->dx_norm);
    if (tm->dffn_act) cuMemFree_v2(tm->dffn_act);
    if (tm->dgate) cuMemFree_v2(tm->dgate);
    if (tm->dup) cuMemFree_v2(tm->dup);
    if (tm->dattn_ctx) cuMemFree_v2(tm->dattn_ctx);
    if (tm->attn_ds) cuMemFree_v2(tm->attn_ds);
    if (tm->dq) cuMemFree_v2(tm->dq);
    if (tm->dk) cuMemFree_v2(tm->dk);
    if (tm->dv) cuMemFree_v2(tm->dv);
    if (tm->dh_tmp) cuMemFree_v2(tm->dh_tmp);
    if (tm->sq_blk) cuMemFree_v2(tm->sq_blk);
    if (tm->scale_pair) cuMemFree_v2(tm->scale_pair);
    if (tm->inf_h) cuMemFree_v2(tm->inf_h);
    free(tm);
}

static double lora_forward_loss(
    const LlamaModel *m,
    const LoraLayer *ls,
    LoraTrainMem *tm,
    const LoraExample *ex,
    unsigned int rank,
    float lora_scale
) {
    unsigned int T = ex->T;
    unsigned int T_pad = ((T + 15U) / 16U) * 16U;
    unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
    unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, V = m->vocab_size;
    float eps = m->rms_eps, theta = m->rope_theta;
    int is_neox = (strcmp(m->arch, "qwen2") == 0) ? 1 : 0;

    void *emb_fn = fn_embed_f32;
    if (m->t_tok_emb == ZCC_GGML_F16) emb_fn = fn_embed_f16;
    else if (m->t_tok_emb == ZCC_GGML_Q4_0) emb_fn = fn_embed_q4_0;
    else if (m->t_tok_emb == ZCC_GGML_Q8_0) emb_fn = fn_embed_q8_0;
    else if (m->t_tok_emb == ZCC_GGML_Q4_K) emb_fn = fn_embed_q4_k;
    else if (m->t_tok_emb == ZCC_GGML_Q6_K) emb_fn = fn_embed_q6_k;
    else if (m->t_tok_emb == ZCC_GGML_BF16) emb_fn = fn_embed_bf16;

    uint64_t emb_ptr = m->d_tok_emb;
    for (unsigned int t = 0U; t < T; t++) {
        uint64_t dst_t = tm->layers[0].x_in + (uint64_t)t * (uint64_t)dim * 4ULL;
        unsigned int tok = ex->toks[t];
        void *pe[4] = { &emb_ptr, &dst_t, &tok, &dim };
        check_cu(cuLaunchKernel(emb_fn, (dim + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pe, NULL), "lora_emb");
    }

    unsigned int total_pairs = (nh + nkv) * (hd >> 1);
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LayerWeights *lw = &m->layers[l];
        const LoraLayer *lp = &ls[l];
        LoraLayerAct *la = &tm->layers[l];

        uint64_t an_ptr = lw->d_attn_norm;
        void *prn1[5] = { &la->x_norm1, &la->x_in, &an_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm_batch, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, prn1, NULL), "lora_rn1");

        lora_frozen_linear_fwd(tm, lw->d_wq, lw->t_wq, la->x_norm1, la->q, T, T_pad, dim, dim, 0U);
        lora_frozen_linear_fwd(tm, lw->d_wk, lw->t_wk, la->x_norm1, la->k, T, T_pad, kv_dim, dim, 0U);
        lora_frozen_linear_fwd(tm, lw->d_wv, lw->t_wv, la->x_norm1, la->v, T, T_pad, kv_dim, dim, 0U);

        lora_proj_fwd(&lp->q, la->x_norm1, la->h_q, la->q, T, lora_scale);
        lora_proj_fwd(&lp->k, la->x_norm1, la->h_k, la->k, T, lora_scale);
        lora_proj_fwd(&lp->v, la->x_norm1, la->h_v, la->v, T, lora_scale);

        if (lw->has_bq) {
            uint64_t bq = lw->d_bq;
            void *pbq[3] = { &la->q, &bq, &dim };
            check_cu(cuLaunchKernel(fn_add_bias_batch, (dim + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, pbq, NULL), "lora_bq");
        }
        if (lw->has_bk) {
            uint64_t bk = lw->d_bk;
            void *pbk[3] = { &la->k, &bk, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias_batch, (kv_dim + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, pbk, NULL), "lora_bk");
        }
        if (lw->has_bv) {
            uint64_t bv = lw->d_bv;
            void *pbv[3] = { &la->v, &bv, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias_batch, (kv_dim + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, pbv, NULL), "lora_bv");
        }

        unsigned int pos0 = 0U;
        void *rfn = is_neox ? fn_rope_neox_batch : fn_rope_batch;
        void *prp[7] = { &la->q, &la->k, &pos0, &nh, &nkv, &hd, &theta };
        check_cu(cuLaunchKernel(rfn, (total_pairs + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, prp, NULL), "lora_rope_fwd");

        void *patt[9] = { &la->q, &la->k, &la->v, &la->attn_probs, &la->attn_ctx, &T, &nh, &nkv, &hd };
        check_cu(cuLaunchKernel(lfn_attn_fwd, T, nh, 1U, 32U, 1U, 1U, 0U, NULL, patt, NULL), "lora_attn_fwd");

        lora_frozen_linear_fwd(tm, lw->d_wo, lw->t_wo, la->attn_ctx, la->attn_out, T, T_pad, dim, dim, 0U);
        lora_proj_fwd(&lp->o, la->attn_ctx, la->h_o, la->attn_out, T, lora_scale);
        lora_gpu_add_out(la->x_in, la->attn_out, la->x_mid, T * dim);

        uint64_t fn_ptr = lw->d_ffn_norm;
        void *prn2[5] = { &la->x_norm2, &la->x_mid, &fn_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm_batch, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, prn2, NULL), "lora_rn2");

        lora_frozen_linear_fwd(tm, lw->d_wgate, lw->t_wgate, la->x_norm2, la->gate_pre, T, T_pad, hid, dim, 0U);
        lora_frozen_linear_fwd(tm, lw->d_wup, lw->t_wup, la->x_norm2, la->up_pre, T, T_pad, hid, dim, 0U);

        unsigned int thid = T * hid;
        uint64_t ffact = tm->ffn_act;
        void *psw[4] = { &la->gate_pre, &la->up_pre, &ffact, &thid };
        check_cu(cuLaunchKernel(lfn_swiglu_fwd, (thid + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psw, NULL), "lora_sw_fwd");

        uint64_t x_next = (l + 1U < m->n_layers) ? tm->layers[l + 1U].x_in : tm->x_final;
        lora_frozen_linear_fwd(tm, lw->d_wdown, lw->t_wdown, ffact, x_next, T, T_pad, dim, hid, 0U);
        lora_gpu_add_inplace(x_next, la->x_mid, T * dim);
    }

    uint64_t on_ptr = m->d_out_norm, xf = tm->x_final, xfn = tm->x_final_norm;
    void *pron[5] = { &xfn, &xf, &on_ptr, &dim, &eps };
    check_cu(cuLaunchKernel(fn_rmsnorm_batch, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pron, NULL), "lora_outnorm");

    lora_lmhead_fwd(tm, m->d_wout, m->t_wout, xfn, tm->logits, T, T_pad, V, dim);

    check_cu(cuMemcpyHtoD_v2(tm->targets, ex->targets, (size_t)T * 4U), "HtoD(targets)");
    float inv_act = (ex->active_cnt > 0U) ? (1.0f / (float)ex->active_cnt) : 0.0f;
    uint64_t lg = tm->logits, tg = tm->targets, dlg = tm->dlogits, ls_buf = tm->losses;
    void *pce[7] = { &lg, &tg, &dlg, &ls_buf, &T, &V, &inv_act };
    check_cu(cuLaunchKernel(lfn_ce_masked_bwd, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pce, NULL), "lora_ce_bwd");

    float h_losses[2048];
    if (T > 2048U) {
#ifdef ZCC_GPU_LORA_LIBRARY
        zcc_llm_set_err("Sequence too long");
        return 0.0;
#else
        fprintf(stderr, "Sequence too long\n");
        exit(1);
#endif
    }
    check_cu(cuMemcpyDtoH_v2(h_losses, tm->losses, (size_t)T * 4U), "DtoH(losses)");
    lora_fail_check();
    double sum_l = 0.0;
    for (unsigned int t = 0U; t < T; t++) sum_l += (double)h_losses[t];
    return (ex->active_cnt > 0U) ? (sum_l / (double)ex->active_cnt) : 0.0;
}

static void lora_backward_pass(
    const LlamaModel *m,
    const LoraLayer *ls,
    LoraTrainMem *tm,
    unsigned int T,
    float lora_scale
) {
    unsigned int T_pad = ((T + 15U) / 16U) * 16U;
    unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
    unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, V = m->vocab_size;
    float eps = m->rms_eps, theta = m->rope_theta;
    unsigned int is_neox = (strcmp(m->arch, "qwen2") == 0) ? 1U : 0U;
    unsigned int total_pairs = (nh + nkv) * (hd >> 1);

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            lora_gpu_zero(ps[p]->d_dA, ps[p]->r * ps[p]->K_in);
            lora_gpu_zero(ps[p]->d_dB, ps[p]->N_out * ps[p]->r);
        }
    }

    lora_lmhead_bwd_dx(tm, m->d_wout, m->t_wout, tm->dlogits, tm->dx_norm, T, T_pad, V, dim);
    uint64_t xf = tm->x_final, dxn = tm->dx_norm, on_ptr = m->d_out_norm, dxc = tm->dx_cur;
    unsigned int zero = 0U;
    void *pron_b[8] = { &xf, &dxn, &on_ptr, &dxc, &T, &dim, &eps, &zero };
    check_cu(cuLaunchKernel(lfn_rmsnorm_bwd, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, pron_b, NULL), "bwd_outnorm");

    for (int l = (int)m->n_layers - 1; l >= 0; l--) {
        const LayerWeights *lw = &m->layers[l];
        const LoraLayer *lp = &ls[l];
        const LoraLayerAct *la = &tm->layers[l];

        lora_frozen_linear_bwd(tm, lw->d_wdown, lw->t_wdown, tm->dx_cur, tm->dffn_act, T, T_pad, dim, hid, 0U);
        unsigned int thid = T * hid;
        uint64_t dg = tm->dgate, du = tm->dup, dfact = tm->dffn_act;
        void *pswb[6] = { &la->gate_pre, &la->up_pre, &dfact, &dg, &du, &thid };
        check_cu(cuLaunchKernel(lfn_swiglu_bwd, (thid + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pswb, NULL), "bwd_sw");

        lora_frozen_linear_bwd(tm, lw->d_wgate, lw->t_wgate, dg, tm->dx_norm, T, T_pad, hid, dim, 0U);
        lora_frozen_linear_bwd(tm, lw->d_wup, lw->t_wup, du, tm->dx_norm, T, T_pad, hid, dim, 1U);

        uint64_t fn_ptr = lw->d_ffn_norm, dxm = tm->dx_mid;
        void *prn2_b[8] = { &la->x_mid, &dxn, &fn_ptr, &dxm, &T, &dim, &eps, &zero };
        check_cu(cuLaunchKernel(lfn_rmsnorm_bwd, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, prn2_b, NULL), "bwd_rn2");
        lora_gpu_add_inplace(dxm, dxc, T * dim);

        lora_frozen_linear_bwd(tm, lw->d_wo, lw->t_wo, dxm, tm->dattn_ctx, T, T_pad, dim, dim, 0U);
        lora_proj_bwd(&lp->o, la->attn_ctx, la->h_o, dxm, tm->dattn_ctx, tm->dh_tmp, T, lora_scale);

        uint64_t dctx = tm->dattn_ctx, ads = tm->attn_ds, dq = tm->dq, dk = tm->dk, dv = tm->dv;
        void *pds[8] = { &dctx, &la->v, &la->attn_probs, &ads, &T, &nh, &nkv, &hd };
        check_cu(cuLaunchKernel(lfn_attn_bwd_ds, T, nh, 1U, 32U, 1U, 1U, 0U, NULL, pds, NULL), "bwd_ds");
        void *pbq[7] = { &ads, &la->k, &dq, &T, &nh, &nkv, &hd };
        check_cu(cuLaunchKernel(lfn_attn_bwd_q, T, nh, 1U, 32U, 1U, 1U, 0U, NULL, pbq, NULL), "bwd_q");
        void *pbkv[10] = { &ads, &la->attn_probs, &la->q, &dctx, &dk, &dv, &T, &nh, &nkv, &hd };
        check_cu(cuLaunchKernel(lfn_attn_bwd_kv, T, nkv, 1U, 32U, 1U, 1U, 0U, NULL, pbkv, NULL), "bwd_kv");

        unsigned int pos0 = 0U;
        void *prpb[8] = { &dq, &dk, &pos0, &nh, &nkv, &hd, &theta, &is_neox };
        check_cu(cuLaunchKernel(lfn_rope_bwd, (total_pairs + 255U) / 256U, T, 1U, 256U, 1U, 1U, 0U, NULL, prpb, NULL), "bwd_rope");

        lora_frozen_linear_bwd(tm, lw->d_wq, lw->t_wq, dq, dxn, T, T_pad, dim, dim, 0U);
        lora_proj_bwd(&lp->q, la->x_norm1, la->h_q, dq, dxn, tm->dh_tmp, T, lora_scale);
        lora_frozen_linear_bwd(tm, lw->d_wk, lw->t_wk, dk, dxn, T, T_pad, kv_dim, dim, 1U);
        lora_proj_bwd(&lp->k, la->x_norm1, la->h_k, dk, dxn, tm->dh_tmp, T, lora_scale);
        lora_frozen_linear_bwd(tm, lw->d_wv, lw->t_wv, dv, dxn, T, T_pad, kv_dim, dim, 1U);
        lora_proj_bwd(&lp->v, la->x_norm1, la->h_v, dv, dxn, tm->dh_tmp, T, lora_scale);

        uint64_t an_ptr = lw->d_attn_norm;
        void *prn1_b[8] = { &la->x_in, &dxn, &an_ptr, &dxc, &T, &dim, &eps, &zero };
        check_cu(cuLaunchKernel(lfn_rmsnorm_bwd, T, 1U, 1U, 32U, 1U, 1U, 0U, NULL, prn1_b, NULL), "bwd_rn1");
        lora_gpu_add_inplace(dxc, dxm, T * dim);
    }
    lora_fail_check();
}

static float lora_clip_and_adamw(
    const LlamaModel *m,
    const LoraLayer *ls,
    const LoraTrainMem *tm,
    float lr,
    float wd,
    float grad_clip,
    int step_idx
) {
    float h_blk[256];
    double sum_sq = 0.0;
    uint64_t sb = tm->sq_blk;
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            uint64_t bufs[2] = { ps[p]->d_dA, ps[p]->d_dB };
            unsigned int ns[2] = { ps[p]->r * ps[p]->K_in, ps[p]->N_out * ps[p]->r };
            for (int k = 0; k < 2; k++) {
                void *psq[3] = { &bufs[k], &sb, &ns[k] };
                check_cu(cuLaunchKernel(lfn_sq_sum, 256U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, psq, NULL), "lora_sq");
                check_cu(cuMemcpyDtoH_v2(h_blk, sb, 256U * 4U), "DtoH(sq_blk)");
                for (int bi = 0; bi < 256; bi++) sum_sq += (double)h_blk[bi];
            }
        }
    }
    float gnorm = (float)sqrt(sum_sq);
    if (grad_clip > 0.0f && gnorm > grad_clip) {
        float sc = grad_clip / gnorm;
        for (unsigned int l = 0U; l < m->n_layers; l++) {
            const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
            for (int p = 0; p < 4; p++) {
                uint64_t bufs[2] = { ps[p]->d_dA, ps[p]->d_dB };
                unsigned int ns[2] = { ps[p]->r * ps[p]->K_in, ps[p]->N_out * ps[p]->r };
                for (int k = 0; k < 2; k++) {
                    void *psc[3] = { &bufs[k], &sc, &ns[k] };
                    check_cu(cuLaunchKernel(lfn_scale, (ns[k] + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, psc, NULL), "lora_clip");
                }
            }
        }
    }

    float beta1 = 0.9f, beta2 = 0.999f, eps_a = 1e-8f;
    float bc1_inv = 1.0f / (1.0f - (float)pow((double)beta1, (double)step_idx));
    float bc2_inv = 1.0f / (1.0f - (float)pow((double)beta2, (double)step_idx));
    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            uint64_t w[2] = { ps[p]->d_A, ps[p]->d_B };
            uint64_t g[2] = { ps[p]->d_dA, ps[p]->d_dB };
            uint64_t mm[2] = { ps[p]->d_mA, ps[p]->d_mB };
            uint64_t vv[2] = { ps[p]->d_vA, ps[p]->d_vB };
            unsigned int ns[2] = { ps[p]->r * ps[p]->K_in, ps[p]->N_out * ps[p]->r };
            for (int k = 0; k < 2; k++) {
                void *pad[12] = { &w[k], &g[k], &mm[k], &vv[k], &ns[k], &lr, &beta1, &beta2, &eps_a, &wd, &bc1_inv, &bc2_inv };
                check_cu(cuLaunchKernel(lfn_adamw, (ns[k] + 255U) / 256U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, pad, NULL), "lora_adamw");
            }
        }
    }
    lora_fail_check();
    return gnorm;
}

/* KV-Cached Single-Token Forward with Optional LoRA Adapters */
static void gpu_forward_token_lora(
    const LlamaModel *m,
    const GpuActivations *act,
    const LoraLayer *ls,
    float lora_scale,
    uint64_t d_inf_h,
    unsigned int tok,
    unsigned int pos,
    unsigned int eff_max_seq,
    int use_tensor_core,
    int kv_type
) {
    unsigned int dim = m->dim, hidden = m->hidden_dim, kv_dim = m->kv_dim;
    float eps = m->rms_eps, theta = m->rope_theta;
    unsigned int blk_dim = 256U;
    unsigned int grd_dim = (dim + blk_dim - 1U) / blk_dim;
    unsigned int grd_hid = (hidden + blk_dim - 1U) / blk_dim;
    unsigned int grd_kv = (kv_dim + blk_dim - 1U) / blk_dim;
    unsigned int total_pairs = (m->n_heads + m->n_kv_heads) * (m->head_dim >> 1);
    unsigned int grd_rope = (total_pairs + blk_dim - 1U) / blk_dim;

    void *emb_fn = fn_embed_f32;
    if (m->t_tok_emb == ZCC_GGML_F16) emb_fn = fn_embed_f16;
    else if (m->t_tok_emb == ZCC_GGML_Q4_0) emb_fn = fn_embed_q4_0;
    else if (m->t_tok_emb == ZCC_GGML_Q8_0) emb_fn = fn_embed_q8_0;
    else if (m->t_tok_emb == ZCC_GGML_Q4_K) emb_fn = fn_embed_q4_k;
    else if (m->t_tok_emb == ZCC_GGML_Q6_K) emb_fn = fn_embed_q6_k;
    else if (m->t_tok_emb == ZCC_GGML_BF16) emb_fn = fn_embed_bf16;
    uint64_t emb_ptr = m->d_tok_emb, x_ptr = act->x;
    void *p_emb[4] = { &emb_ptr, &x_ptr, &tok, &dim };
    check_cu(cuLaunchKernel(emb_fn, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_emb, NULL), "inf_embed");

    for (unsigned int l = 0U; l < m->n_layers; l++) {
        const LayerWeights *lw = &m->layers[l];
        uint64_t xb_ptr = act->xb, xb2_ptr = act->xb2, xf16_ptr = act->x_f16;
        uint64_t q_ptr = act->q, k_ptr = act->k, v_ptr = act->v;
        uint64_t kc_ptr = act->k_cache, vc_ptr = act->v_cache, att_ptr = act->att_scratch;
        uint64_t hb_ptr = act->hb, hb2_ptr = act->hb2;

        uint64_t an_ptr = lw->d_attn_norm;
        void *p_rn1[5] = { &xb_ptr, &x_ptr, &an_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_rn1, NULL), "inf_rn1");

        gpu_linear_gen(lw->d_wq, lw->t_wq, xb_ptr, xf16_ptr, q_ptr, dim, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wk, lw->t_wk, xb_ptr, xf16_ptr, k_ptr, kv_dim, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wv, lw->t_wv, xb_ptr, xf16_ptr, v_ptr, kv_dim, dim, use_tensor_core);
        if (ls) {
            lora_proj_fwd(&ls[l].q, xb_ptr, d_inf_h, q_ptr, 1U, lora_scale);
            lora_proj_fwd(&ls[l].k, xb_ptr, d_inf_h, k_ptr, 1U, lora_scale);
            lora_proj_fwd(&ls[l].v, xb_ptr, d_inf_h, v_ptr, 1U, lora_scale);
        }
        if (lw->has_bq) {
            uint64_t bq = lw->d_bq; void *pbq[3] = { &q_ptr, &bq, &dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, pbq, NULL), "inf_bq");
        }
        if (lw->has_bk) {
            uint64_t bk = lw->d_bk; void *pbk[3] = { &k_ptr, &bk, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, pbk, NULL), "inf_bk");
        }
        if (lw->has_bv) {
            uint64_t bv = lw->d_bv; void *pbv[3] = { &v_ptr, &bv, &kv_dim };
            check_cu(cuLaunchKernel(fn_add_bias, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, pbv, NULL), "inf_bv");
        }

        unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, mseq = eff_max_seq;
        void *rfn = (strcmp(m->arch, "qwen2") == 0) ? fn_rope_neox : fn_rope;
        void *p_rope[7] = { &q_ptr, &k_ptr, &pos, &nh, &nkv, &hd, &theta };
        check_cu(cuLaunchKernel(rfn, grd_rope, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_rope, NULL), "inf_rope");

        void *p_kvs[8] = { &k_ptr, &v_ptr, &kc_ptr, &vc_ptr, &l, &pos, &mseq, &kv_dim };
        void *p_fa[10] = { &q_ptr, &kc_ptr, &vc_ptr, &xb_ptr, &l, &pos, &mseq, &nh, &nkv, &hd };
        if (kv_type == ZCC_KV_F16) {
            check_cu(cuLaunchKernel(fn_kv_store_f16, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_kvs, NULL), "inf_kvs16");
            check_cu(cuLaunchKernel(fn_attn_flash_f16, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_fa, NULL), "inf_attn16");
        } else {
            check_cu(cuLaunchKernel(fn_kv_store, grd_kv, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_kvs, NULL), "inf_kvs32");
            if (hd <= 256U) {
                check_cu(cuLaunchKernel(fn_attn_flash, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_fa, NULL), "inf_attn32");
            } else {
                void *p_att[11] = { &q_ptr, &kc_ptr, &vc_ptr, &att_ptr, &xb_ptr, &l, &pos, &mseq, &nh, &nkv, &hd };
                check_cu(cuLaunchKernel(fn_attn_gqa, nh, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_att, NULL), "inf_attngqa");
            }
        }

        gpu_linear_gen(lw->d_wo, lw->t_wo, xb_ptr, xf16_ptr, xb2_ptr, dim, dim, use_tensor_core);
        if (ls) lora_proj_fwd(&ls[l].o, xb_ptr, d_inf_h, xb2_ptr, 1U, lora_scale);

        void *p_add1[3] = { &x_ptr, &xb2_ptr, &dim };
        check_cu(cuLaunchKernel(fn_add, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_add1, NULL), "inf_add1");

        uint64_t fn_ptr = lw->d_ffn_norm;
        void *p_rn2[5] = { &xb_ptr, &x_ptr, &fn_ptr, &dim, &eps };
        check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_rn2, NULL), "inf_rn2");

        gpu_linear_gen(lw->d_wgate, lw->t_wgate, xb_ptr, xf16_ptr, hb_ptr, hidden, dim, use_tensor_core);
        gpu_linear_gen(lw->d_wup, lw->t_wup, xb_ptr, xf16_ptr, hb2_ptr, hidden, dim, use_tensor_core);
        void *p_sw[3] = { &hb_ptr, &hb2_ptr, &hidden };
        check_cu(cuLaunchKernel(fn_swiglu, grd_hid, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_sw, NULL), "inf_sw");
        gpu_linear_gen(lw->d_wdown, lw->t_wdown, hb_ptr, xf16_ptr, xb2_ptr, dim, hidden, use_tensor_core);
        void *p_add2[3] = { &x_ptr, &xb2_ptr, &dim };
        check_cu(cuLaunchKernel(fn_add, grd_dim, 1U, 1U, blk_dim, 1U, 1U, 0U, NULL, p_add2, NULL), "inf_add2");
    }

    uint64_t on_ptr = m->d_out_norm, xb_ptr = act->xb, xf16_ptr = act->x_f16, log_ptr = act->logits;
    void *p_orn[5] = { &xb_ptr, &x_ptr, &on_ptr, &dim, &eps };
    check_cu(cuLaunchKernel(fn_rmsnorm, 1U, 1U, 1U, 32U, 1U, 1U, 0U, NULL, p_orn, NULL), "inf_outnorm");
    gpu_linear_gen(m->d_wout, m->t_wout, xb_ptr, xf16_ptr, log_ptr, m->vocab_size, dim, use_tensor_core);
    lora_fail_check();
}

/* ========================================================================= */
/* Track 23A: Evolved Blackwell sm_120 CUDA-Graph Decoder Plan               */
/* ========================================================================= */

typedef struct {
    void *fn;
    unsigned int gx, gy, gz;
    unsigned int bx, by, bz;
    uint64_t u64_a[10];
    uint32_t u32_a[8];
    float f32_a[2];
    void *kparams[16];
    int deps[4];
    unsigned int n_deps;
} LoraDecodeNode;

typedef struct {
    LoraDecodeNode *body_nodes;
    unsigned int body_count;
    unsigned int decode_logits_count;
    unsigned int decode_argmax_count;
    unsigned int body_cap;
    void *body_exec;
    void *decode_logits_exec;
    void *decode_argmax_exec;
    const LoraLayer *bound_ls;
    unsigned int bound_rank;
    int bound_kv_type;
    unsigned int bound_eff_max_seq;
    int body_ready;
    int use_graph;
} LoraDecodePlan;

static void lora_dec_set_deps(LoraDecodeNode *nd, int n, int d0, int d1, int d2, int d3) {
    if (!nd) return;
    int ds[4];
    ds[0] = d0; ds[1] = d1; ds[2] = d2; ds[3] = d3;
    unsigned int cnt = 0U;
    for (int i = 0; i < n && i < 4; i++) {
        if (ds[i] >= 0) nd->deps[cnt++] = ds[i];
    }
    nd->n_deps = cnt;
}

static LoraDecodeNode *lora_dec_add_node(
    LoraDecodeNode *nodes,
    unsigned int *io_count,
    unsigned int cap,
    void *fn,
    unsigned int gx,
    unsigned int bx
) {
    if (*io_count >= cap) {
        zcc_llm_set_err("LoraDecodePlan node capacity exceeded");
        return NULL;
    }
    LoraDecodeNode *nd = &nodes[(*io_count)++];
    memset(nd, 0, sizeof(*nd));
    nd->fn = fn;
    nd->gx = gx; nd->gy = 1U; nd->gz = 1U;
    nd->bx = bx; nd->by = 1U; nd->bz = 1U;
    for (int k = 0; k < 4; k++) nd->deps[k] = -1;
    return nd;
}

static void lora_dec_instantiate_graph(LoraDecodeNode *nodes, unsigned int num_nodes, void **out_exec) {
    *out_exec = NULL;
    if (num_nodes == 0U || g_zcc_llm_failed) return;
    void **g_nodes = (void **)calloc((size_t)num_nodes, sizeof(void *));
    if (!g_nodes) {
        zcc_llm_set_err("OOM allocating CUDA Graph node table");
        return;
    }
    void *graph = NULL;
    check_cu(cuGraphCreate(&graph, 0U), "lora_cuGraphCreate");
    if (g_zcc_llm_failed || !graph) {
        free(g_nodes);
        return;
    }
    for (unsigned int i = 0U; i < num_nodes; i++) {
        LoraDecodeNode *nd = &nodes[i];
        void *dep_ptrs[4];
        unsigned int ndep = 0U;
        for (unsigned int d = 0U; d < nd->n_deps && d < 4U; d++) {
            int di = nd->deps[d];
            if (di >= 0 && (unsigned int)di < i && g_nodes[di] != NULL) {
                dep_ptrs[ndep++] = g_nodes[di];
            }
        }
        ZccCudaKernelNodeParams kp;
        memset(&kp, 0, sizeof(kp));
        kp.func = nd->fn;
        kp.gridDimX = nd->gx; kp.gridDimY = nd->gy; kp.gridDimZ = nd->gz;
        kp.blockDimX = nd->bx; kp.blockDimY = nd->by; kp.blockDimZ = nd->bz;
        kp.kernelParams = nd->kparams;
        check_cu(cuGraphAddKernelNode(&g_nodes[i], graph, ndep ? dep_ptrs : NULL, ndep, &kp), "lora_cuGraphAddKernelNode");
        if (g_zcc_llm_failed) {
            cuGraphDestroy(graph);
            free(g_nodes);
            return;
        }
    }
    check_cu(cuGraphInstantiateWithFlags(out_exec, graph, 0ULL), "lora_cuGraphInstantiate");
    cuGraphDestroy(graph);
    free(g_nodes);
}

static int lora_dec_emit_rmsnorm(
    LoraDecodeNode *nodes,
    unsigned int *io_count,
    unsigned int cap,
    uint64_t dst_ptr,
    uint64_t src_ptr,
    uint64_t w_ptr,
    uint64_t scratch_ptr,
    unsigned int dim,
    float eps,
    int dep_idx
) {
    LoraDecodeNode *nd = lora_dec_add_node(nodes, io_count, cap, dfn_rmsnorm, 1U, 256U);
    if (!nd) return -1;
    nd->u64_a[0] = dst_ptr; nd->u64_a[1] = src_ptr; nd->u64_a[2] = w_ptr; nd->u64_a[3] = scratch_ptr;
    nd->u32_a[0] = dim; nd->f32_a[0] = eps;
    nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1];
    nd->kparams[2] = &nd->u64_a[2]; nd->kparams[3] = &nd->u64_a[3];
    nd->kparams[4] = &nd->u32_a[0]; nd->kparams[5] = &nd->f32_a[0];
    lora_dec_set_deps(nd, 1, dep_idx, -1, -1, -1);
    return (int)(*io_count - 1U);
}

static int lora_dec_emit_matvec(
    LoraDecodeNode *nodes,
    unsigned int *io_count,
    unsigned int cap,
    uint64_t w_ptr,
    uint32_t w_type,
    uint64_t x_ptr,
    uint64_t xf16_ptr,
    uint64_t y_ptr,
    unsigned int N,
    unsigned int K,
    unsigned int add_res,
    int use_tc,
    int xf16_ready,
    int dep_idx
) {
    if (w_type == ZCC_GGML_F16 && use_tc && (K % 16U) == 0U && add_res == 0U) {
        int tc_dep = dep_idx;
        if (!xf16_ready) {
            LoraDecodeNode *cv = lora_dec_add_node(nodes, io_count, cap, fn_f32_to_f16, (K + 255U) / 256U, 256U);
            if (!cv) return -1;
            cv->u64_a[0] = x_ptr; cv->u64_a[1] = xf16_ptr; cv->u32_a[0] = K;
            cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
            lora_dec_set_deps(cv, 1, dep_idx, -1, -1, -1);
            tc_dep = (int)(*io_count - 1U);
        }
        LoraDecodeNode *tc = lora_dec_add_node(nodes, io_count, cap, fn_gemm_tc, (N + 7U) / 8U, 32U);
        if (!tc) return -1;
        tc->u64_a[0] = xf16_ptr; tc->u64_a[1] = w_ptr; tc->u64_a[2] = y_ptr;
        tc->u32_a[0] = 1U; tc->u32_a[1] = N; tc->u32_a[2] = K;
        tc->kparams[0] = &tc->u64_a[0]; tc->kparams[1] = &tc->u64_a[1]; tc->kparams[2] = &tc->u64_a[2];
        tc->kparams[3] = &tc->u32_a[0]; tc->kparams[4] = &tc->u32_a[1]; tc->kparams[5] = &tc->u32_a[2];
        lora_dec_set_deps(tc, 1, tc_dep, -1, -1, -1);
        return (int)(*io_count - 1U);
    }
    void *fn = dfn_matvec_f32_4w;
    unsigned int rows_per_cta = 4U;
    if (w_type == ZCC_GGML_Q4_K) {
        if (N <= 2048U) { fn = dfn_matvec_q4_k_4w; rows_per_cta = 4U; }
        else { fn = dfn_matvec_q4_k_8r; rows_per_cta = 8U; }
    } else if (w_type == ZCC_GGML_Q6_K) {
        fn = dfn_matvec_q6_k_4w; rows_per_cta = 4U;
    } else if (w_type == ZCC_GGML_F16) fn = dfn_matvec_f16_4w;
    else if (w_type == ZCC_GGML_BF16) fn = dfn_matvec_bf16_4w;
    else if (w_type == ZCC_GGML_Q8_0) fn = dfn_matvec_q8_0_4w;
    else if (w_type == ZCC_GGML_Q4_0) fn = dfn_matvec_q4_0_4w;
    LoraDecodeNode *nd = lora_dec_add_node(nodes, io_count, cap, fn, (N + rows_per_cta - 1U) / rows_per_cta, 128U);
    if (!nd) return -1;
    nd->u64_a[0] = w_ptr; nd->u64_a[1] = x_ptr; nd->u64_a[2] = y_ptr;
    nd->u32_a[0] = N; nd->u32_a[1] = K; nd->u32_a[2] = add_res;
    nd->kparams[0] = &nd->u64_a[0]; nd->kparams[1] = &nd->u64_a[1]; nd->kparams[2] = &nd->u64_a[2];
    nd->kparams[3] = &nd->u32_a[0]; nd->kparams[4] = &nd->u32_a[1]; nd->kparams[5] = &nd->u32_a[2];
    lora_dec_set_deps(nd, 1, dep_idx, -1, -1, -1);
    return (int)(*io_count - 1U);
}

static void lora_decode_plan_init(LoraDecodePlan *p, int use_graph) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->use_graph = use_graph ? 1 : 0;
}

static void lora_decode_plan_invalidate(LoraDecodePlan *p) {
    if (!p) return;
    if (p->body_exec) { cuGraphExecDestroy(p->body_exec); p->body_exec = NULL; }
    if (p->decode_logits_exec) { cuGraphExecDestroy(p->decode_logits_exec); p->decode_logits_exec = NULL; }
    if (p->decode_argmax_exec) { cuGraphExecDestroy(p->decode_argmax_exec); p->decode_argmax_exec = NULL; }
    p->body_count = 0U;
    p->decode_logits_count = 0U;
    p->decode_argmax_count = 0U;
    p->body_ready = 0;
    p->bound_ls = NULL;
    p->bound_rank = 0U;
}

static void lora_decode_plan_free(LoraDecodePlan *p) {
    if (!p) return;
    lora_decode_plan_invalidate(p);
    if (p->body_nodes) { free(p->body_nodes); p->body_nodes = NULL; }
    p->body_cap = 0U;
}

static void lora_decode_plan_ensure(
    LoraDecodePlan *p,
    const LlamaModel *m,
    const GpuActivations *act,
    const LoraLayer *ls,
    uint64_t d_inf_h,
    unsigned int eff_max_seq,
    int use_tc,
    int kv_type
) {
    if (!p || !m || !act) return;
    unsigned int rank = ls ? ls[0].q.r : 0U;
    unsigned int dim = m->dim, hid = m->hidden_dim, kv_dim = m->kv_dim;
    unsigned int nh = m->n_heads, nkv = m->n_kv_heads, hd = m->head_dim, V = m->vocab_size;
    float eps = m->rms_eps, theta = m->rope_theta;
    unsigned int is_neox = (strcmp(m->arch, "qwen2") == 0) ? 1U : 0U;

    if (p->body_ready && p->bound_ls == ls && p->bound_rank == rank &&
        p->bound_kv_type == kv_type && p->bound_eff_max_seq == eff_max_seq) {
        return;
    }

    lora_decode_plan_invalidate(p);
    unsigned int need_cap = 16U + m->n_layers * 28U;
    if (!p->body_nodes || p->body_cap < need_cap) {
        if (p->body_nodes) free(p->body_nodes);
        p->body_cap = need_cap;
        p->body_nodes = (LoraDecodeNode *)calloc((size_t)p->body_cap, sizeof(LoraDecodeNode));
        if (!p->body_nodes) {
            p->body_cap = 0U;
            zcc_llm_set_err("OOM allocating LoraDecodePlan body_nodes");
            return;
        }
    }

    unsigned int total_cnt = 0U;
    void *emb_fn = dfn_embed_ind_f32;
    if (m->t_tok_emb == ZCC_GGML_F16) emb_fn = dfn_embed_ind_f16;
    else if (m->t_tok_emb == ZCC_GGML_BF16) emb_fn = dfn_embed_ind_bf16;
    else if (m->t_tok_emb == ZCC_GGML_Q8_0) emb_fn = dfn_embed_ind_q8_0;
    else if (m->t_tok_emb == ZCC_GGML_Q4_0) emb_fn = dfn_embed_ind_q4_0;
    else if (m->t_tok_emb == ZCC_GGML_Q4_K) emb_fn = dfn_embed_ind_q4_k;
    else if (m->t_tok_emb == ZCC_GGML_Q6_K) emb_fn = dfn_embed_ind_q6_k;
    int tail_x = -1;
    {
        LoraDecodeNode *em = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                               emb_fn, (dim + 255U) / 256U, 256U);
        if (em) {
            em->u64_a[0] = m->d_tok_emb; em->u64_a[1] = act->x; em->u64_a[2] = act->step_args;
            em->u32_a[0] = dim;
            em->kparams[0] = &em->u64_a[0]; em->kparams[1] = &em->u64_a[1];
            em->kparams[2] = &em->u64_a[2]; em->kparams[3] = &em->u32_a[0];
            tail_x = (int)(total_cnt - 1U);
        }
    }

    unsigned int total_pairs = (nh + nkv) * (hd >> 1);
    unsigned int grd_rope = (total_pairs + 255U) / 256U;
    void *rk_fn = (kv_type == ZCC_KV_F16) ? dfn_rope_kv_ind_f16 : dfn_rope_kv_ind_f32;
    void *fa_fn = (kv_type == ZCC_KV_F16) ? dfn_attn_flash_ind_f16 : dfn_attn_flash_ind_f32;

    for (unsigned int l = 0U; l < m->n_layers && !g_zcc_llm_failed; l++) {
        const LayerWeights *lw = &m->layers[l];
        int id_rn1 = lora_dec_emit_rmsnorm(p->body_nodes, &total_cnt, p->body_cap,
                                           act->xb, act->x, lw->d_attn_norm, act->xb2, dim, eps, tail_x);

        int qkv_in = id_rn1;
        int qkv_tc_ready = 0;
        if ((lw->t_wq == ZCC_GGML_F16 || lw->t_wk == ZCC_GGML_F16 || lw->t_wv == ZCC_GGML_F16) && use_tc && (dim % 16U) == 0U) {
            LoraDecodeNode *cv = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                   fn_f32_to_f16, (dim + 255U) / 256U, 256U);
            if (cv) {
                cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = dim;
                cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
                lora_dec_set_deps(cv, 1, id_rn1, -1, -1, -1);
                qkv_in = (int)(total_cnt - 1U);
                qkv_tc_ready = 1;
            }
        }
        int id_wq = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wq, lw->t_wq, act->xb, act->x_f16, act->q, dim, dim, 0U, use_tc, qkv_tc_ready, qkv_in);
        int id_wk = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wk, lw->t_wk, act->xb, act->x_f16, act->k, kv_dim, dim, 0U, use_tc, qkv_tc_ready, qkv_in);
        int id_wv = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wv, lw->t_wv, act->xb, act->x_f16, act->v, kv_dim, dim, 0U, use_tc, qkv_tc_ready, qkv_in);

        int id_rk = -1;
        unsigned int bmask = (lw->has_bq ? 1U : 0U) | (lw->has_bk ? 2U : 0U) | (lw->has_bv ? 4U : 0U);
        if (ls && ls[l].q.r > 0U && d_inf_h) {
            unsigned int r_l = ls[l].q.r;
            int id_lqa = -1;
            LoraDecodeNode *lqa = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                    dfn_lora_qkv_a_ind, 3U * r_l, 32U);
            if (lqa) {
                lqa->u64_a[0] = act->xb; lqa->u64_a[1] = ls[l].q.d_A; lqa->u64_a[2] = ls[l].k.d_A;
                lqa->u64_a[3] = ls[l].v.d_A; lqa->u64_a[4] = d_inf_h; lqa->u64_a[5] = act->step_args;
                lqa->u32_a[0] = r_l; lqa->u32_a[1] = dim;
                for (int k = 0; k < 6; k++) lqa->kparams[k] = &lqa->u64_a[k];
                lqa->kparams[6] = &lqa->u32_a[0]; lqa->kparams[7] = &lqa->u32_a[1];
                lora_dec_set_deps(lqa, 1, id_rn1, -1, -1, -1);
                id_lqa = (int)(total_cnt - 1U);
            }
            int id_lqb = -1;
            LoraDecodeNode *lqb = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                    dfn_lora_qkv_b_add_ind, (dim + 2U * kv_dim + 255U) / 256U, 256U);
            if (lqb) {
                lqb->u64_a[0] = d_inf_h; lqb->u64_a[1] = ls[l].q.d_B; lqb->u64_a[2] = ls[l].k.d_B; lqb->u64_a[3] = ls[l].v.d_B;
                lqb->u64_a[4] = act->q; lqb->u64_a[5] = act->k; lqb->u64_a[6] = act->v; lqb->u64_a[7] = act->step_args;
                lqb->u32_a[0] = dim; lqb->u32_a[1] = kv_dim; lqb->u32_a[2] = r_l;
                for (int k = 0; k < 8; k++) lqb->kparams[k] = &lqb->u64_a[k];
                for (int k = 0; k < 3; k++) lqb->kparams[8 + k] = &lqb->u32_a[k];
                lora_dec_set_deps(lqb, 4, id_wq, id_wk, id_wv, id_lqa);
                id_lqb = (int)(total_cnt - 1U);
            }
            LoraDecodeNode *rk = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap, rk_fn, grd_rope, 256U);
            if (rk) {
                rk->u64_a[0] = act->q; rk->u64_a[1] = act->k; rk->u64_a[2] = act->v;
                rk->u64_a[3] = lw->has_bq ? lw->d_bq : act->q;
                rk->u64_a[4] = lw->has_bk ? lw->d_bk : act->k;
                rk->u64_a[5] = lw->has_bv ? lw->d_bv : act->v;
                rk->u64_a[6] = act->k_cache; rk->u64_a[7] = act->v_cache; rk->u64_a[8] = act->step_args;
                rk->u32_a[0] = l; rk->u32_a[1] = nh; rk->u32_a[2] = nkv; rk->u32_a[3] = hd;
                rk->f32_a[0] = theta; rk->u32_a[4] = is_neox; rk->u32_a[5] = bmask;
                for (int k = 0; k < 9; k++) rk->kparams[k] = &rk->u64_a[k];
                for (int k = 0; k < 4; k++) rk->kparams[9 + k] = &rk->u32_a[k];
                rk->kparams[13] = &rk->f32_a[0]; rk->kparams[14] = &rk->u32_a[4]; rk->kparams[15] = &rk->u32_a[5];
                lora_dec_set_deps(rk, 1, id_lqb, -1, -1, -1);
                id_rk = (int)(total_cnt - 1U);
            }
        } else {
            LoraDecodeNode *rk = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap, rk_fn, grd_rope, 256U);
            if (rk) {
                rk->u64_a[0] = act->q; rk->u64_a[1] = act->k; rk->u64_a[2] = act->v;
                rk->u64_a[3] = lw->has_bq ? lw->d_bq : act->q;
                rk->u64_a[4] = lw->has_bk ? lw->d_bk : act->k;
                rk->u64_a[5] = lw->has_bv ? lw->d_bv : act->v;
                rk->u64_a[6] = act->k_cache; rk->u64_a[7] = act->v_cache; rk->u64_a[8] = act->step_args;
                rk->u32_a[0] = l; rk->u32_a[1] = nh; rk->u32_a[2] = nkv; rk->u32_a[3] = hd;
                rk->f32_a[0] = theta; rk->u32_a[4] = is_neox; rk->u32_a[5] = bmask;
                for (int k = 0; k < 9; k++) rk->kparams[k] = &rk->u64_a[k];
                for (int k = 0; k < 4; k++) rk->kparams[9 + k] = &rk->u32_a[k];
                rk->kparams[13] = &rk->f32_a[0]; rk->kparams[14] = &rk->u32_a[4]; rk->kparams[15] = &rk->u32_a[5];
                lora_dec_set_deps(rk, 3, id_wq, id_wk, id_wv, -1);
                id_rk = (int)(total_cnt - 1U);
            }
        }

        int id_fa = -1;
        {
            LoraDecodeNode *fa = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap, fa_fn, nh, 32U);
            if (fa) {
                fa->u64_a[0] = act->q; fa->u64_a[1] = act->k_cache; fa->u64_a[2] = act->v_cache;
                fa->u64_a[3] = act->xb; fa->u64_a[4] = act->step_args;
                fa->u32_a[0] = l; fa->u32_a[1] = nh; fa->u32_a[2] = nkv; fa->u32_a[3] = hd;
                for (int k = 0; k < 5; k++) fa->kparams[k] = &fa->u64_a[k];
                for (int k = 0; k < 4; k++) fa->kparams[5 + k] = &fa->u32_a[k];
                lora_dec_set_deps(fa, 1, id_rk, -1, -1, -1);
                id_fa = (int)(total_cnt - 1U);
            }
        }

        int id_wo = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wo, lw->t_wo, act->xb, act->x_f16, act->x, dim, dim, 1U, use_tc, 0, id_fa);
        tail_x = id_wo;
        if (ls && ls[l].o.r > 0U && d_inf_h) {
            const LoraProj *lpo = &ls[l].o;
            int id_loa = -1;
            LoraDecodeNode *loa = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                    dfn_lora_fwd_a_ind, lpo->r, 32U);
            if (loa) {
                loa->u64_a[0] = act->xb; loa->u64_a[1] = lpo->d_A; loa->u64_a[2] = d_inf_h + 768ULL; loa->u64_a[3] = act->step_args;
                loa->u32_a[0] = lpo->r; loa->u32_a[1] = lpo->K_in;
                for (int k = 0; k < 4; k++) loa->kparams[k] = &loa->u64_a[k];
                loa->kparams[4] = &loa->u32_a[0]; loa->kparams[5] = &loa->u32_a[1];
                lora_dec_set_deps(loa, 1, id_fa, -1, -1, -1);
                id_loa = (int)(total_cnt - 1U);
            }
            LoraDecodeNode *lob = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                    dfn_lora_fwd_b_add_ind, (lpo->N_out + 255U) / 256U, 256U);
            if (lob) {
                lob->u64_a[0] = d_inf_h + 768ULL; lob->u64_a[1] = lpo->d_B; lob->u64_a[2] = act->x; lob->u64_a[3] = act->step_args;
                lob->u32_a[0] = lpo->N_out; lob->u32_a[1] = lpo->r;
                for (int k = 0; k < 4; k++) lob->kparams[k] = &lob->u64_a[k];
                lob->kparams[4] = &lob->u32_a[0]; lob->kparams[5] = &lob->u32_a[1];
                lora_dec_set_deps(lob, 2, id_wo, id_loa, -1, -1);
                tail_x = (int)(total_cnt - 1U);
            }
        }

        int id_rn2 = lora_dec_emit_rmsnorm(p->body_nodes, &total_cnt, p->body_cap,
                                           act->xb, act->x, lw->d_ffn_norm, act->xb2, dim, eps, tail_x);

        int ffn_in = id_rn2;
        int ffn_tc_ready = 0;
        if ((lw->t_wgate == ZCC_GGML_F16 || lw->t_wup == ZCC_GGML_F16) && use_tc && (dim % 16U) == 0U) {
            LoraDecodeNode *cv = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                   fn_f32_to_f16, (dim + 255U) / 256U, 256U);
            if (cv) {
                cv->u64_a[0] = act->xb; cv->u64_a[1] = act->x_f16; cv->u32_a[0] = dim;
                cv->kparams[0] = &cv->u64_a[0]; cv->kparams[1] = &cv->u64_a[1]; cv->kparams[2] = &cv->u32_a[0];
                lora_dec_set_deps(cv, 1, id_rn2, -1, -1, -1);
                ffn_in = (int)(total_cnt - 1U);
                ffn_tc_ready = 1;
            }
        }
        int id_wg = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wgate, lw->t_wgate, act->xb, act->x_f16, act->hb, hid, dim, 0U, use_tc, ffn_tc_ready, ffn_in);
        int id_wu = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                         lw->d_wup, lw->t_wup, act->xb, act->x_f16, act->hb2, hid, dim, 0U, use_tc, ffn_tc_ready, ffn_in);
        int id_sw = -1;
        {
            LoraDecodeNode *sw = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap,
                                                   fn_swiglu, (hid + 255U) / 256U, 256U);
            if (sw) {
                sw->u64_a[0] = act->hb; sw->u64_a[1] = act->hb2; sw->u32_a[0] = hid;
                sw->kparams[0] = &sw->u64_a[0]; sw->kparams[1] = &sw->u64_a[1]; sw->kparams[2] = &sw->u32_a[0];
                lora_dec_set_deps(sw, 2, id_wg, id_wu, -1, -1);
                id_sw = (int)(total_cnt - 1U);
            }
        }
        tail_x = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                      lw->d_wdown, lw->t_wdown, act->hb, act->x_f16, act->x, dim, hid, 1U, use_tc, 0, id_sw);
    }

    p->body_count = total_cnt;

    int id_on = lora_dec_emit_rmsnorm(p->body_nodes, &total_cnt, p->body_cap,
                                      act->xb, act->x, m->d_out_norm, act->xb2, dim, eps, tail_x);
    int id_wout = lora_dec_emit_matvec(p->body_nodes, &total_cnt, p->body_cap,
                                       m->d_wout, m->t_wout, act->xb, act->x_f16, act->logits,
                                       V, dim, 0U, use_tc, 0, id_on);
    p->decode_logits_count = total_cnt;

    int id_ab = -1;
    LoraDecodeNode *ab = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap, dfn_argmax_blocks, 64U, 32U);
    if (ab) {
        ab->u64_a[0] = act->logits; ab->u64_a[1] = act->argmax_ids;
        ab->u64_a[2] = act->argmax_vals; ab->u64_a[3] = act->draft_ids;
        ab->u32_a[0] = V;
        for (int k = 0; k < 4; k++) ab->kparams[k] = &ab->u64_a[k];
        ab->kparams[4] = &ab->u32_a[0];
        lora_dec_set_deps(ab, 1, id_wout, -1, -1, -1);
        id_ab = (int)(total_cnt - 1U);
    }
    LoraDecodeNode *af = lora_dec_add_node(p->body_nodes, &total_cnt, p->body_cap, dfn_argmax_finalize, 1U, 32U);
    if (af) {
        af->u64_a[0] = act->argmax_ids; af->u64_a[1] = act->argmax_vals; af->u64_a[2] = act->draft_ids;
        af->u64_a[3] = act->argmax_ids + 2048ULL * 4ULL;
        af->u64_a[4] = act->argmax_vals + 2048ULL * 4ULL;
        af->u32_a[0] = V;
        for (int k = 0; k < 5; k++) af->kparams[k] = &af->u64_a[k];
        af->kparams[5] = &af->u32_a[0];
        lora_dec_set_deps(af, 1, id_ab, -1, -1, -1);
    }
    p->decode_argmax_count = total_cnt;

    if (p->use_graph && !g_zcc_llm_failed) {
        lora_dec_instantiate_graph(p->body_nodes, p->body_count, &p->body_exec);
        lora_dec_instantiate_graph(p->body_nodes, p->decode_logits_count, &p->decode_logits_exec);
        lora_dec_instantiate_graph(p->body_nodes, p->decode_argmax_count, &p->decode_argmax_exec);
    }
    if (!g_zcc_llm_failed) {
        p->bound_ls = ls;
        p->bound_rank = rank;
        p->bound_kv_type = kv_type;
        p->bound_eff_max_seq = eff_max_seq;
        p->body_ready = 1;
    } else {
        lora_decode_plan_invalidate(p);
    }
}

static void lora_write_step_args(
    uint64_t d_step_args,
    uint32_t tok,
    uint32_t pos,
    uint32_t eff_max_seq,
    float lora_scale
) {
    uint32_t sa[4];
    sa[0] = tok;
    sa[1] = pos;
    sa[2] = eff_max_seq;
    sa[3] = 0U;
    memcpy(&sa[3], &lora_scale, 4U);
    check_cu(cuMemcpyHtoD_v2(d_step_args, sa, 16U), "st_step_args");
}

static void lora_dec_launch(const LoraDecodeNode *nodes, unsigned int n, void *graph_exec) {
    if (graph_exec) {
        check_cu(cuGraphLaunch(graph_exec, NULL), "lora_cuGraphLaunch");
        return;
    }
    for (unsigned int i = 0U; i < n; i++) {
        const LoraDecodeNode *nd = &nodes[i];
        check_cu(cuLaunchKernel(nd->fn, nd->gx, nd->gy, nd->gz, nd->bx, nd->by, nd->bz, 0U, NULL, (void **)nd->kparams, NULL), "lora_dec_direct");
        if (g_zcc_llm_failed) return;
    }
}

static void lora_decode_step_body(
    const LoraDecodePlan *p,
    const GpuActivations *act,
    uint32_t tok,
    uint32_t pos,
    uint32_t eff_max_seq,
    float lora_scale
) {
    lora_write_step_args(act->step_args, tok, pos, eff_max_seq, lora_scale);
    if (g_zcc_llm_failed) return;
    lora_dec_launch(p->body_nodes, p->body_count, p->body_exec);
}

static void lora_decode_step_logits(
    const LoraDecodePlan *p,
    const GpuActivations *act,
    uint32_t tok,
    uint32_t pos,
    uint32_t eff_max_seq,
    float lora_scale
) {
    lora_write_step_args(act->step_args, tok, pos, eff_max_seq, lora_scale);
    if (g_zcc_llm_failed) return;
    lora_dec_launch(p->body_nodes, p->decode_logits_count, p->decode_logits_exec);
}

static int lora_decode_step_argmax(
    const LoraDecodePlan *p,
    const LlamaModel *m,
    const GpuActivations *act,
    uint32_t tok,
    uint32_t pos,
    uint32_t eff_max_seq,
    float lora_scale,
    int step_idx
) {
    lora_write_step_args(act->step_args, tok, pos, eff_max_seq, lora_scale);
    if (g_zcc_llm_failed) return 0;
    lora_dec_launch(p->body_nodes, p->decode_argmax_count, p->decode_argmax_exec);
    if (g_zcc_llm_failed) return 0;
    uint32_t res[2];
    res[0] = 0U; res[1] = 0U;
    check_cu(cuMemcpyDtoH_v2(res, act->argmax_ids + 2048ULL * 4ULL, 8U), "st_argmax_dtoh");
    if (g_zcc_llm_failed) return 0;
    if (res[1] != 0U) {
        char msg[160];
        if (step_idx < 0) snprintf(msg, sizeof(msg), "Non-finite GPU logit detected");
        else snprintf(msg, sizeof(msg), "Non-finite GPU logit at step %d", step_idx);
        zcc_llm_set_err(msg);
        return 0;
    }
    if (res[0] >= m->vocab_size) return 0;
    return (int)res[0];
}

static int run_grad_check(
    const LlamaModel *m,
    unsigned int rank,
    float alpha,
    float eps_fd,
    int fp32_matmul,
    const char *sft_path,
    int chat_mode
) {
    LoraExample ex;
    memset(&ex, 0, sizeof(ex));
    LoraDataset tmp_ds;
    memset(&tmp_ds, 0, sizeof(tmp_ds));
    if (sft_path && parse_lora_dataset(sft_path, m, chat_mode, &tmp_ds) == 0 && tmp_ds.count > 0U) {
        ex = tmp_ds.items[0];
        if (ex.T > 48U) {
            ex.T = 48U;
            ex.active_cnt = 0U;
            for (unsigned int t = 0U; t < ex.T; t++) {
                if (ex.targets[t] < m->vocab_size) ex.active_cnt++;
            }
            if (ex.active_cnt == 0U) {
                ex.targets[ex.T - 1U] = 1U % m->vocab_size;
                ex.active_cnt = 1U;
            }
        }
    } else {
        ex.T = 6U;
        ex.active_cnt = 3U;
        ex.toks = (uint32_t *)malloc(ex.T * sizeof(uint32_t));
        ex.targets = (uint32_t *)malloc(ex.T * sizeof(uint32_t));
        for (unsigned int t = 0U; t < ex.T; t++) {
            ex.toks[t] = (uint32_t)((17U * (t + 1U)) % m->vocab_size);
            ex.targets[t] = (t >= 3U) ? (uint32_t)((31U * (t + 2U)) % m->vocab_size) : 0xFFFFFFFFU;
        }
    }

    LoraLayer *ls = alloc_lora_layers(m, rank, 1, 1, 777U);
    LoraTrainMem *tm = alloc_lora_train_mem(m, ex.T, rank, fp32_matmul);
    float scale = alpha / (float)rank;

    double loss0 = lora_forward_loss(m, ls, tm, &ex, rank, scale);
    lora_backward_pass(m, ls, tm, ex.T, scale);

    /* Always evaluate the finite-difference oracle in exact FP32 so Tensor Core checks measure analytical vs exact truth */
    tm->fp32_matmul = 1;

    unsigned int test_layers[2] = { 0U, (m->n_layers > 1U) ? (m->n_layers - 1U) : 0U };
    double max_rel = 0.0;
    double dot_ab = 0.0, norm_a = 0.0, norm_b = 0.0;
    int checked = 0;

    for (int li = 0; li < ((m->n_layers > 1U) ? 2 : 1); li++) {
        unsigned int l = test_layers[li];
        const LoraProj *ps[4] = { &ls[l].q, &ls[l].k, &ls[l].v, &ls[l].o };
        for (int p = 0; p < 4; p++) {
            for (int is_b = 0; is_b < 2; is_b++) {
                uint64_t dw = is_b ? ps[p]->d_B : ps[p]->d_A;
                uint64_t dg = is_b ? ps[p]->d_dB : ps[p]->d_dA;
                unsigned int n_el = is_b ? (ps[p]->N_out * ps[p]->r) : (ps[p]->r * ps[p]->K_in);

                float *h_g = (float *)malloc((size_t)n_el * sizeof(float));
                if (!h_g) { fprintf(stderr, "OOM in run_grad_check\n"); exit(1); }
                check_cu(cuMemcpyDtoH_v2(h_g, dg, (size_t)n_el * sizeof(float)), "gc_DtoH_gall");
                unsigned int idx = 0U;
                double best_abs = -1.0;
                for (unsigned int ei = 0U; ei < n_el; ei++) {
                    double av = fabs((double)h_g[ei]);
                    if (av > best_abs) { best_abs = av; idx = ei; }
                }
                if (best_abs < 1e-12) {
                    idx = ((l * 13U + (unsigned int)p * 7U + (unsigned int)is_b * 3U) * 19U) % n_el;
                }
                float g_ana = h_g[idx];
                free(h_g);

                float w_orig = 0.0f;
                check_cu(cuMemcpyDtoH_v2(&w_orig, dw + (uint64_t)idx * 4ULL, 4U), "gc_DtoH_w");

                float wp = w_orig + eps_fd, wm = w_orig - eps_fd;
                check_cu(cuMemcpyHtoD_v2(dw + (uint64_t)idx * 4ULL, &wp, 4U), "gc_plus");
                double lp_v = lora_forward_loss(m, ls, tm, &ex, rank, scale);
                check_cu(cuMemcpyHtoD_v2(dw + (uint64_t)idx * 4ULL, &wm, 4U), "gc_minus");
                double lm_v = lora_forward_loss(m, ls, tm, &ex, rank, scale);
                check_cu(cuMemcpyHtoD_v2(dw + (uint64_t)idx * 4ULL, &w_orig, 4U), "gc_restore");

                double g_num = (lp_v - lm_v) / (2.0 * (double)eps_fd);
                double denom = fabs((double)g_ana) + fabs(g_num) + 1e-6;
                double rel = fabs((double)g_ana - g_num) / denom;
                if (rel > max_rel) max_rel = rel;
                dot_ab += (double)g_ana * g_num;
                norm_a += (double)g_ana * (double)g_ana;
                norm_b += g_num * g_num;
                checked++;
            }
        }
    }
    tm->fp32_matmul = fp32_matmul;

    double cos_sim = (norm_a > 0.0 && norm_b > 0.0) ? (dot_ab / (sqrt(norm_a) * sqrt(norm_b))) : 0.0;
    printf("LORA_GRAD_CHECK_OK samples=%d base_loss=%.6f fp32_matmul=%d eps=%.6f max_rel_err=%.6f cos_sim=%.8f\n",
           checked, loss0, fp32_matmul, (double)eps_fd, max_rel, cos_sim);

    if (sft_path && tmp_ds.count > 0U) free_lora_dataset(&tmp_ds);
    else { free(ex.toks); free(ex.targets); }
    free_lora_train_mem(tm, m->n_layers);
    free_lora_layers(ls, m->n_layers);
    return (cos_sim > 0.99) ? 0 : 1;
}

#ifndef ZCC_GPU_LORA_LIBRARY
int main(int argc, char **argv) {
    const char *model_path = NULL;
    const char *sft_path = NULL;
    const char *save_lora_path = NULL;
    const char *load_lora_path = NULL;
    const char *prompt = NULL;
    int rank = 8;
    float alpha = 16.0f;
    float lr = 5e-4f;
    float wd = 0.0f;
    float grad_clip = 1.0f;
    float eps_fd = -1.0f;
    int train_steps = 80;
    int grad_check = 0;
    int fp32_matmul = 0;
    int chat_mode = 0;
    int interactive = 0;
    int clean_mode = 0;
    int kv_type = ZCC_KV_F32;
    int ctx_override = 0;
    int inf_steps = 48;
    float temp = 0.0f;
    int top_k = 1;
    float top_p = 1.0f;
    float min_p = 0.0f;
    float rep_penalty = 1.0f;
    uint32_t seed = 42U;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) { model_path = argv[++i]; }
        else if (strcmp(argv[i], "--train") == 0 && i + 1 < argc) { sft_path = argv[++i]; }
        else if (strcmp(argv[i], "--sft") == 0 && i + 1 < argc) { sft_path = argv[++i]; }
        else if (strcmp(argv[i], "--save-lora") == 0 && i + 1 < argc) { save_lora_path = argv[++i]; }
        else if (strcmp(argv[i], "--load-lora") == 0 && i + 1 < argc) { load_lora_path = argv[++i]; }
        else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) { prompt = argv[++i]; }
        else if (strcmp(argv[i], "--rank") == 0 && i + 1 < argc) { if (parse_int_arg(argv[++i], "--rank", 1, 64, &rank) != 0) return 1; }
        else if (strcmp(argv[i], "--alpha") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--alpha", 0.01f, 1, 1024.0f, 1, &alpha) != 0) return 1; }
        else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--lr", 1e-7f, 1, 1.0f, 1, &lr) != 0) return 1; }
        else if (strcmp(argv[i], "--wd") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--wd", 0.0f, 1, 1.0f, 1, &wd) != 0) return 1; }
        else if (strcmp(argv[i], "--grad-clip") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--grad-clip", 0.0f, 1, 100.0f, 1, &grad_clip) != 0) return 1; }
        else if (strcmp(argv[i], "--eps") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--eps", 1e-5f, 1, 0.5f, 1, &eps_fd) != 0) return 1; }
        else if (strcmp(argv[i], "--train-steps") == 0 && i + 1 < argc) { if (parse_int_arg(argv[++i], "--train-steps", 1, 100000, &train_steps) != 0) return 1; }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) { if (parse_int_arg(argv[++i], "--steps", 1, 4096, &inf_steps) != 0) return 1; }
        else if (strcmp(argv[i], "--temp") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--temp", 0.0f, 1, 5.0f, 1, &temp) != 0) return 1; }
        else if (strcmp(argv[i], "--top-k") == 0 && i + 1 < argc) { if (parse_int_arg(argv[++i], "--top-k", 1, 50000, &top_k) != 0) return 1; }
        else if (strcmp(argv[i], "--top-p") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--top-p", 0.01f, 1, 1.0f, 1, &top_p) != 0) return 1; }
        else if (strcmp(argv[i], "--min-p") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--min-p", 0.0f, 1, 1.0f, 1, &min_p) != 0) return 1; }
        else if (strcmp(argv[i], "--rep-penalty") == 0 && i + 1 < argc) { if (parse_finite_float_arg(argv[++i], "--rep-penalty", 0.5f, 1, 10.0f, 1, &rep_penalty) != 0) return 1; }
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) { seed = (uint32_t)strtol(argv[++i], NULL, 10); }
        else if (strcmp(argv[i], "--ctx") == 0 && i + 1 < argc) { if (parse_int_arg(argv[++i], "--ctx", 16, 32768, &ctx_override) != 0) return 1; }
        else if (strcmp(argv[i], "--kv-type") == 0 && i + 1 < argc) {
            const char *kv_arg = argv[++i];
            if (strcmp(kv_arg, "f32") == 0 || strcmp(kv_arg, "0") == 0) kv_type = ZCC_KV_F32;
            else if (strcmp(kv_arg, "f16") == 0 || strcmp(kv_arg, "1") == 0) kv_type = ZCC_KV_F16;
            else { fprintf(stderr, "INVALID_ARG: unsupported --kv-type '%s' (use f32 or f16)\n", kv_arg); return 1; }
        }
        else if (strcmp(argv[i], "--grad-check") == 0) { grad_check = 1; }
        else if (strcmp(argv[i], "--fp32-matmul") == 0) { fp32_matmul = 1; }
        else if (strcmp(argv[i], "--chat") == 0) { chat_mode = 1; }
        else if (strcmp(argv[i], "--interactive") == 0) { interactive = 1; }
        else if (strcmp(argv[i], "--clean") == 0) { clean_mode = 1; }
        else { fprintf(stderr, "INVALID_ARG: unknown flag '%s'\n", argv[i]); return 1; }
    }

    if (!model_path) {
        fprintf(stderr, "INVALID_ARG: --model <base.gguf> is required\n");
        return 1;
    }

    if (cuInit(0U) != 0) { fprintf(stderr, "cuInit failed\n"); return 1; }
    int dev_cnt = 0, dev = 0;
    if (cuDeviceGetCount(&dev_cnt) != 0 || dev_cnt <= 0 || cuDeviceGet(&dev, 0) != 0) {
        fprintf(stderr, "No CUDA device\n");
        return 1;
    }
    void *ctx = NULL;
    if (cuCtxCreate_v2(&ctx, 0U, dev) != 0) { fprintf(stderr, "cuCtxCreate_v2 failed\n"); return 1; }

    void *mod_llm = NULL, *mod_lora = NULL;
    int aot_t = 0;
    uint64_t meta[16];
    unsigned int ptx_b = 0U;
    if (load_ptx_module(NULL, g_gpu_llm_kernels_ptx, (unsigned int)(sizeof(g_gpu_llm_kernels_ptx) - 1U), 0,
                        &aot_t, &mod_llm, meta, &ptx_b) != 0 || load_generic_kernels(mod_llm) != 0) {
        cuCtxDestroy_v2(ctx); return 1;
    }
    if (load_ptx_module(NULL, g_gpu_lora_kernels_ptx, (unsigned int)(sizeof(g_gpu_lora_kernels_ptx) - 1U), 0,
                        &aot_t, &mod_lora, meta, &ptx_b) != 0 || load_lora_kernels(mod_lora) != 0) {
        cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx); return 1;
    }

    LlamaModel *model = (LlamaModel *)calloc(1U, sizeof(LlamaModel));
    if (!model || parse_and_stream_gguf_ex(model_path, NULL, 0U, 0ULL, 0ULL, NULL, model, 0, NULL, 0, 0ULL) != 0) {
        if (model) { free_model_gpu_mem(model); free_llama_model(model); free(model); }
        cuModuleUnload(mod_lora); cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx);
        return 1;
    }

    if (!clean_mode) {
        printf("GPU_LORA_MODEL path=\"%s\" arch=%s layers=%u dim=%u hidden=%u heads=%u kv_heads=%u vocab=%u wtype=%s\n",
               model_path, model->arch, model->n_layers, model->dim, model->hidden_dim,
               model->n_heads, model->n_kv_heads, model->vocab_size, wtype_name_str(model->weight_type_primary));
    }

    if (grad_check) {
        if (eps_fd < 0.0f) eps_fd = fp32_matmul ? 2e-3f : 5e-3f;
        int rc = run_grad_check(model, (unsigned int)rank, alpha, eps_fd, fp32_matmul, sft_path, chat_mode);
        free_model_gpu_mem(model); free_llama_model(model); free(model);
        cuModuleUnload(mod_lora); cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx);
        return rc;
    }

    LoraLayer *lora_ls = NULL;
    unsigned int active_rank = (unsigned int)rank;
    float active_alpha = alpha;

    if (load_lora_path) {
        lora_ls = load_lora_adapter(load_lora_path, model, &active_rank, &active_alpha);
        if (!lora_ls) {
            free_model_gpu_mem(model); free_llama_model(model); free(model);
            cuModuleUnload(mod_lora); cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx);
            return 1;
        }
    }

    if (sft_path && !load_lora_path) {
        LoraDataset ds;
        if (parse_lora_dataset(sft_path, model, chat_mode, &ds) != 0) {
            fprintf(stderr, "LORA_DATASET_ERROR: zero valid Q:/A: examples in '%s'\n", sft_path);
            free_model_gpu_mem(model); free_llama_model(model); free(model);
            cuModuleUnload(mod_lora); cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx);
            return 1;
        }
        unsigned int train_T_max = ds.max_seq_len;
        if (train_T_max < 16U) train_T_max = 16U;
        lora_ls = alloc_lora_layers(model, active_rank, 1, 0, seed);
        LoraTrainMem *tm = alloc_lora_train_mem(model, train_T_max, active_rank, fp32_matmul);
        float scale = active_alpha / (float)active_rank;
        uint64_t params = lora_payload_byte_size(model, active_rank) / 4ULL;

        printf("LORA_TRAIN_START examples=%zu max_T=%u rank=%u alpha=%.1f params=%llu steps=%d lr=%.6f fp32_matmul=%d\n",
               ds.count, ds.max_seq_len, active_rank, (double)active_alpha, (unsigned long long)params,
               train_steps, (double)lr, fp32_matmul);
        fflush(stdout);

        void *ev0 = NULL, *ev1 = NULL;
        check_cu(cuEventCreate(&ev0, 0U), "tr_ev0");
        check_cu(cuEventCreate(&ev1, 0U), "tr_ev1");
        check_cu(cuEventRecord(ev0, NULL), "tr_start");

        double first_loss = 0.0, last_loss = 0.0;
        double total_toks = 0.0;
        for (int st = 1; st <= train_steps; st++) {
            const LoraExample *ex = &ds.items[(size_t)(st - 1) % ds.count];
            double step_loss = lora_forward_loss(model, lora_ls, tm, ex, active_rank, scale);
            lora_backward_pass(model, lora_ls, tm, ex->T, scale);
            float lr_t = lr * (0.1f + 0.9f * 0.5f * (1.0f + (float)cos(3.14159265358979323846 * (double)(st - 1) / (double)train_steps)));
            float gn = lora_clip_and_adamw(model, lora_ls, tm, lr_t, wd, grad_clip, st);
            total_toks += (double)ex->T;
            if (st == 1) first_loss = step_loss;
            last_loss = step_loss;
            if (st == 1 || st == train_steps || (st % 10 == 0)) {
                check_cu(cuEventRecord(ev1, NULL), "tr_tick");
                check_cu(cuEventSynchronize(ev1), "tr_sync");
                float ms = 0.0f;
                check_cu(cuEventElapsedTime(&ms, ev0, ev1), "tr_ms");
                double tps = (ms > 0.0f) ? (total_toks * 1000.0 / (double)ms) : 0.0;
                printf("LORA_STEP step=%d/%d ex=%zu/%zu loss=%.6f gnorm=%.4f lr=%.6f tok_per_sec=%.2f\n",
                       st, train_steps, ((size_t)(st - 1) % ds.count) + 1U, ds.count,
                       step_loss, (double)gn, (double)lr_t, tps);
                fflush(stdout);
            }
        }
        check_cu(cuEventDestroy_v2(ev0), "tr_ev0_d");
        check_cu(cuEventDestroy_v2(ev1), "tr_ev1_d");
        printf("LORA_TRAIN_OK initial_loss=%.6f final_loss=%.6f\n", first_loss, last_loss);
        free_lora_train_mem(tm, model->n_layers);
        free_lora_dataset(&ds);

        if (save_lora_path) {
            if (save_lora_adapter(save_lora_path, model, lora_ls, active_rank, active_alpha) != 0) {
                free_lora_layers(lora_ls, model->n_layers);
                free_model_gpu_mem(model); free_llama_model(model); free(model);
                cuModuleUnload(mod_lora); cuModuleUnload(mod_llm); cuCtxDestroy_v2(ctx);
                return 1;
            }
        }
    }

    if (prompt || interactive) {
        unsigned int eff_max_seq = ctx_override > 0 ? (unsigned int)ctx_override : 512U;
        if (eff_max_seq > model->max_seq_len) eff_max_seq = model->max_seq_len;
        uint64_t row_kv = (kv_type == ZCC_KV_F16) ? ((uint64_t)model->kv_dim * 2ULL) : ((uint64_t)model->kv_dim * 4ULL);
        size_t kv_bytes = (size_t)((uint64_t)model->n_layers * (uint64_t)eff_max_seq * row_kv);

        GpuActivations act;
        memset(&act, 0, sizeof(act));
        check_cu(cuMemAlloc_v2(&act.k_cache, kv_bytes), "inf.kc");
        check_cu(cuMemAlloc_v2(&act.v_cache, kv_bytes), "inf.vc");
        check_cu(cuMemAlloc_v2(&act.x, (size_t)model->dim * 4U), "inf.x");
        check_cu(cuMemAlloc_v2(&act.xb, (size_t)model->dim * 4U), "inf.xb");
        check_cu(cuMemAlloc_v2(&act.xb2, (size_t)model->dim * 4U), "inf.xb2");
        check_cu(cuMemAlloc_v2(&act.x_f16, (size_t)(model->hidden_dim > model->dim ? model->hidden_dim : model->dim) * 2U), "inf.xf16");
        check_cu(cuMemAlloc_v2(&act.q, (size_t)model->dim * 4U), "inf.q");
        check_cu(cuMemAlloc_v2(&act.k, (size_t)model->kv_dim * 4U), "inf.k");
        check_cu(cuMemAlloc_v2(&act.v, (size_t)model->kv_dim * 4U), "inf.v");
        if (model->head_dim > 256U) check_cu(cuMemAlloc_v2(&act.att_scratch, (size_t)model->n_heads * eff_max_seq * 4U), "inf.att");
        check_cu(cuMemAlloc_v2(&act.hb, (size_t)model->hidden_dim * 4U), "inf.hb");
        check_cu(cuMemAlloc_v2(&act.hb2, (size_t)model->hidden_dim * 4U), "inf.hb2");
        check_cu(cuMemAlloc_v2(&act.logits, (size_t)model->vocab_size * 4U), "inf.logits");
        uint64_t d_inf_h = 0ULL;
        check_cu(cuMemAlloc_v2(&d_inf_h, (size_t)active_rank * 4U), "inf.lora_h");
        lora_fail_check();

        float *h_logits = (float *)malloc((size_t)model->vocab_size * 4U);
        float *work_logits = (float *)malloc((size_t)model->vocab_size * 4U);
        float *h_probs = (float *)malloc((size_t)model->vocab_size * 4U);
        SampleCand *cands = (SampleCand *)malloc((size_t)model->vocab_size * sizeof(SampleCand));
        uint32_t *hist = (uint32_t *)malloc(((size_t)eff_max_seq + 32U) * sizeof(uint32_t));
        int *gen_ids = (int *)malloc(((size_t)inf_steps + 16U) * sizeof(int));
        size_t gcap = 16384U;
        char *gen_text = (char *)malloc(gcap);
        float scale = active_alpha / (float)active_rank;
        int use_tc = (model->weight_type_primary == ZCC_GGML_F16 && (model->dim % 16U) == 0U) ? 1 : 0;

        char line_buf[4096];
        int pending = prompt ? 1 : 0;
        if (interactive && clean_mode) {
            fprintf(stderr, "[LoRA Interactive ready: rank=%u alpha=%.1f ctx=%u | type /quit to exit]\n",
                    active_rank, (double)active_alpha, eff_max_seq);
        }
        for (;;) {
            const char *cur_p = NULL;
            if (pending) { cur_p = prompt; pending = 0; }
            else if (interactive) {
                if (clean_mode) fprintf(stderr, "\nYou> ");
                fflush(stderr);
                if (!fgets(line_buf, (int)sizeof(line_buf), stdin)) break;
                if ((unsigned char)line_buf[0] == 0xEFU && (unsigned char)line_buf[1] == 0xBBU && (unsigned char)line_buf[2] == 0xBFU) {
                    memmove(line_buf, line_buf + 3, strlen(line_buf + 3) + 1U);
                }
                size_t ll = strlen(line_buf);
                while (ll > 0U && (line_buf[ll - 1U] == '\n' || line_buf[ll - 1U] == '\r')) line_buf[--ll] = '\0';
                if (ll == 0U) continue;
                if (strcmp(line_buf, "/quit") == 0 || strcmp(line_buf, "/exit") == 0) break;
                cur_p = line_buf;
            } else break;

            check_cu(cuMemsetD8_v2(act.k_cache, 0, kv_bytes), "rst_kc");
            check_cu(cuMemsetD8_v2(act.v_cache, 0, kv_bytes), "rst_vc");

            char *fmt = chat_mode ? format_chat_prompt_alloc(model, cur_p, NULL, 0) : NULL;
            const char *tin = fmt ? fmt : cur_p;
            uint32_t *pids = NULL, plen = 0U;
            if (tokenize_prompt(model, tin, 0, &pids, &plen) != 0 || plen == 0U) {
                if (fmt) free(fmt);
                continue;
            }
            if (fmt) free(fmt);
            unsigned int pos = 0U, hlen = 0U;
            for (uint32_t pi = 0U; pi < plen && pos + 1U < eff_max_seq; pi++) {
                uint32_t tk = (pids[pi] < model->vocab_size) ? pids[pi] : 0U;
                hist[hlen++] = tk;
                gpu_forward_token_lora(model, &act, lora_ls, scale, d_inf_h, tk, pos, eff_max_seq, use_tc, kv_type);
                pos++;
            }
            free(pids);
            check_cu(cuCtxSynchronize(), "inf_pf_sync");
            check_cu(cuMemcpyDtoH_v2(h_logits, act.logits, (size_t)model->vocab_size * 4U), "inf_pf_logits");

            int gcnt = 0; size_t glen = 0U; gen_text[0] = '\0';
            if (clean_mode) { fprintf(stderr, "Assistant> "); fflush(stderr); }
            for (int s = 0; s < inf_steps && pos < eff_max_seq; s++) {
                int nxt = sample_next_token(h_logits, work_logits, h_probs, cands, model->vocab_size,
                                            hist, hlen, temp, top_k, top_p, min_p, rep_penalty, &seed);
                if (is_stop_token(model, 0, (uint32_t)nxt)) break;
                record_generated_token_ex(model, (uint32_t)nxt, gen_ids, &gcnt, hist, &hlen,
                                          &gen_text, &gcap, &glen, clean_mode, NULL, NULL);
                gpu_forward_token_lora(model, &act, lora_ls, scale, d_inf_h, (uint32_t)nxt, pos, eff_max_seq, use_tc, kv_type);
                pos++;
                check_cu(cuMemcpyDtoH_v2(h_logits, act.logits, (size_t)model->vocab_size * 4U), "inf_step_logits");
            }
            if (clean_mode) { fprintf(stderr, "\n"); fflush(stderr); }
            else {
                printf("LORA_TEXT lora=%d prompt=\"%s\" generated=\"", lora_ls ? 1 : 0, cur_p);
                print_sanitized(gen_text);
                printf("\" steps=%d\n", gcnt);
                fflush(stdout);
            }
            if (!interactive) break;
        }

        if (d_inf_h) cuMemFree_v2(d_inf_h);
        cuMemFree_v2(act.k_cache); cuMemFree_v2(act.v_cache);
        cuMemFree_v2(act.x); cuMemFree_v2(act.xb); cuMemFree_v2(act.xb2); cuMemFree_v2(act.x_f16);
        cuMemFree_v2(act.q); cuMemFree_v2(act.k); cuMemFree_v2(act.v);
        if (act.att_scratch) cuMemFree_v2(act.att_scratch);
        cuMemFree_v2(act.hb); cuMemFree_v2(act.hb2); cuMemFree_v2(act.logits);
        free(h_logits); free(work_logits); free(h_probs); free(cands);
        free(hist); free(gen_ids); free(gen_text);
    }

    if (lora_ls) free_lora_layers(lora_ls, model->n_layers);
    free_model_gpu_mem(model);
    free_llama_model(model);
    free(model);
    cuModuleUnload(mod_lora);
    cuModuleUnload(mod_llm);
    cuCtxDestroy_v2(ctx);
    return 0;
}
#endif
