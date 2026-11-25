/*
 * ZKAEDI ZCC // Sovereign Win64 GPU GGUF Inference DLL Implementation (Track 17)
 * ============================================================================
 * Wraps src/win64_gpu_llm.c as an in-process Win64 PE32+ DLL (zcc_gpu_llm.dll).
 * Compiled natively by ZCC with ZERO MSVC, MinGW, or CUDA Toolkit.
 * Only non-static symbols in this TU are the 6 public zcc_llm_* exports.
 */

#define ZCC_GPU_LLM_LIBRARY 1
#include "win64_gpu_llm.c"
#include "zcc_gpu_llm_api.h"

typedef struct {
    void *ctx;
    void *mod;
    void *fn_aot_fwd;
    void *ev0;
    void *ev1;
    LlamaModel model;
    GpuActivations act;
    McPlan mc_plan;
    McPrefillFns mc_pf;
    int mc_plan_inited;
    int model_inited;
    uint64_t aot_meta[16];
    uint64_t params;
    size_t kv_bytes;
    unsigned int aot_block_dim;
    unsigned int eff_max_seq;
    unsigned int pos;
    unsigned int hist_len;
    unsigned int hist_cap;
    unsigned int prefill_batch;
    int aot_tier;
    int kv_type;
    int turn;
    int last_gen_tokens;
    double last_tps;
    float *h_logits;
    float *work_logits;
    float *h_probs;
    SampleCand *cands;
    uint32_t *hist;
    int *gen_ids;
    size_t gen_ids_cap;
    char *gen_text;
    size_t gen_text_cap;
} ZccLlmHandle;

static uint64_t zcc_dll_count_params(const LlamaModel *m) {
    uint64_t total = 0ULL;
    if (!m || !m->tensors) return 0ULL;
    for (unsigned int i = 0U; i < m->num_tensors; i++) {
        const GgufTensor *gt = &m->tensors[i];
        uint64_t ne = (gt->n_dims > 0U) ? 1ULL : 0ULL;
        for (uint32_t d = 0U; d < gt->n_dims && d < 4U; d++) {
            ne *= gt->dims[d];
        }
        total += ne;
    }
    return total;
}

static void zcc_dll_free_activations(GpuActivations *act) {
    if (!act) return;
    if (act->x) cuMemFree_v2(act->x);
    if (act->xb) cuMemFree_v2(act->xb);
    if (act->xb2) cuMemFree_v2(act->xb2);
    if (act->x_f16) cuMemFree_v2(act->x_f16);
    if (act->q) cuMemFree_v2(act->q);
    if (act->k) cuMemFree_v2(act->k);
    if (act->v) cuMemFree_v2(act->v);
    if (act->att_scratch) cuMemFree_v2(act->att_scratch);
    if (act->hb) cuMemFree_v2(act->hb);
    if (act->hb2) cuMemFree_v2(act->hb2);
    if (act->probs) cuMemFree_v2(act->probs);
    if (act->step_args) cuMemFree_v2(act->step_args);
    if (act->argmax_ids) cuMemFree_v2(act->argmax_ids);
    if (act->argmax_vals) cuMemFree_v2(act->argmax_vals);
    if (act->draft_ids) cuMemFree_v2(act->draft_ids);
    if (act->w_f16_tmp) cuMemFree_v2(act->w_f16_tmp);
    if (act->prefill_tmp) cuMemFree_v2(act->prefill_tmp);
    if (act->batch_toks) cuMemFree_v2(act->batch_toks);
    if (act->spill_stage[0]) cuMemFree_v2(act->spill_stage[0]);
    if (act->spill_stage[1]) cuMemFree_v2(act->spill_stage[1]);
    if (act->dma_ev[0]) cuEventDestroy_v2(act->dma_ev[0]);
    if (act->dma_ev[1]) cuEventDestroy_v2(act->dma_ev[1]);
    if (act->comp_ev[0]) cuEventDestroy_v2(act->comp_ev[0]);
    if (act->comp_ev[1]) cuEventDestroy_v2(act->comp_ev[1]);
    if (act->dma_stream) cuStreamDestroy_v2(act->dma_stream);
    if (act->k_cache) cuMemFree_v2(act->k_cache);
    if (act->v_cache) cuMemFree_v2(act->v_cache);
    if (act->logits) cuMemFree_v2(act->logits);
    memset(act, 0, sizeof(*act));
}

void zcc_llm_destroy(void *handle) {
    ZccLlmHandle *h = (ZccLlmHandle *)handle;
    if (!h) return;
    if (h->mc_plan_inited && h->aot_tier == 2) {
        mc_plan_free(&h->mc_plan);
        h->mc_plan_inited = 0;
    }
    if (h->model_inited) {
        free_model_gpu_mem(&h->model);
    }
    zcc_dll_free_activations(&h->act);
    if (h->model_inited) {
        free_llama_model(&h->model);
        h->model_inited = 0;
    }
    if (h->ev0) { cuEventDestroy_v2(h->ev0); h->ev0 = NULL; }
    if (h->ev1) { cuEventDestroy_v2(h->ev1); h->ev1 = NULL; }
    if (h->mod) { cuModuleUnload(h->mod); h->mod = NULL; }
    if (h->ctx) { cuCtxDestroy_v2(h->ctx); h->ctx = NULL; }
    if (h->h_logits) free(h->h_logits);
    if (h->work_logits) free(h->work_logits);
    if (h->h_probs) free(h->h_probs);
    if (h->cands) free(h->cands);
    if (h->hist) free(h->hist);
    if (h->gen_ids) free(h->gen_ids);
    if (h->gen_text) free(h->gen_text);
    free(h);
}

const char *zcc_llm_last_error(void) {
    return g_zcc_llm_err;
}

void *zcc_llm_create(const char *model_path, const char *aot_ptx_path, int kv_type, int ctx_len) {
    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';

    if (!model_path || model_path[0] == '\0') {
        zcc_llm_set_err("INVALID_ARG: model_path is NULL or empty");
        return NULL;
    }
    if (kv_type < ZCC_LLM_KV_F32 || kv_type > ZCC_LLM_KV_Q8_0) {
        zcc_llm_set_err("INVALID_ARG: kv_type must be 0 (F32), 1 (F16), or 2 (Q8_0)");
        return NULL;
    }
    if (ctx_len < 0) {
        zcc_llm_set_err("INVALID_ARG: ctx_len must be >= 0");
        return NULL;
    }

    ZccLlmHandle *h = (ZccLlmHandle *)calloc(1U, sizeof(ZccLlmHandle));
    if (!h) {
        zcc_llm_set_err("OOM allocating ZccLlmHandle");
        return NULL;
    }
    h->kv_type = kv_type;

    check_cu(cuInit(0U), "cuInit");
    int dev_count = 0;
    if (!g_zcc_llm_failed) check_cu(cuDeviceGetCount(&dev_count), "cuDeviceGetCount");
    if (!g_zcc_llm_failed && dev_count <= 0) {
        zcc_llm_set_err("NO_CUDA_DEVICE: cuDeviceGetCount returned 0");
    }
    int dev = 0;
    if (!g_zcc_llm_failed) check_cu(cuDeviceGet(&dev, 0), "cuDeviceGet");
    if (!g_zcc_llm_failed) check_cu(cuCtxCreate_v2(&h->ctx, 0U, dev), "cuCtxCreate_v2");
    if (g_zcc_llm_failed) {
        zcc_llm_destroy(h);
        return NULL;
    }

    int use_aot = (aot_ptx_path != NULL && aot_ptx_path[0] != '\0') ? 1 : 0;
    unsigned int ptx_bytes = 0U;
    if (load_ptx_module(use_aot ? aot_ptx_path : NULL,
                        use_aot ? NULL : g_gpu_llm_kernels_ptx,
                        use_aot ? 0U : (unsigned int)(sizeof(g_gpu_llm_kernels_ptx) - 1U),
                        use_aot,
                        &h->aot_tier, &h->mod, h->aot_meta, &ptx_bytes) != 0) {
        if (!g_zcc_llm_failed) zcc_llm_set_err("AOT_PTX_ERROR: failed to load CUDA PTX module");
        zcc_llm_destroy(h);
        return NULL;
    }
    (void)ptx_bytes;

    if (use_aot) {
        h->aot_block_dim = (unsigned int)h->aot_meta[14];
        if (h->aot_tier == 1) {
            check_cu(cuModuleGetFunction(&h->fn_aot_fwd, h->mod, "zcc_aot_forward_token"),
                     "get(zcc_aot_forward_token)");
        }
    } else {
        load_generic_kernels(h->mod);
    }
    if (g_zcc_llm_failed) {
        zcc_llm_destroy(h);
        return NULL;
    }

    if (parse_and_stream_gguf(model_path, NULL, 0U, &h->model, use_aot, h->aot_meta, 0, 0ULL) != 0) {
        h->model_inited = 1;
        if (!g_zcc_llm_failed) zcc_llm_set_err("GGUF_LOAD_ERROR: failed to parse or stream GGUF model");
        zcc_llm_destroy(h);
        return NULL;
    }
    h->model_inited = 1;
    h->params = zcc_dll_count_params(&h->model);

    if (kv_type != ZCC_KV_F32 && h->aot_tier == 1) {
        zcc_llm_set_err("INVALID_ARG: kv_type != F32 not supported in Tier A aot_fused mode");
        zcc_llm_destroy(h);
        return NULL;
    }
    if (kv_type == ZCC_KV_Q8_0 && ((h->model.head_dim & 31U) != 0U || (h->model.kv_dim & 31U) != 0U)) {
        zcc_llm_set_err("INVALID_ARG: kv_type Q8_0 requires head_dim and kv_dim divisible by 32");
        zcc_llm_destroy(h);
        return NULL;
    }
    if (kv_type != ZCC_KV_F32 && h->model.head_dim > 256U) {
        zcc_llm_set_err("INVALID_ARG: quantized KV cache requires head_dim <= 256");
        zcc_llm_destroy(h);
        return NULL;
    }

    h->eff_max_seq = h->model.max_seq_len;
    if (ctx_len > 0) {
        if (h->aot_tier == 1 && (unsigned int)ctx_len > h->model.max_seq_len) {
            zcc_llm_set_err("INVALID_ARG: ctx_len exceeds compiled Tier A max_seq_len");
            zcc_llm_destroy(h);
            return NULL;
        }
        h->eff_max_seq = (unsigned int)ctx_len;
    }

    uint64_t row_bytes_kv = (uint64_t)h->model.kv_dim * 4ULL;
    if (kv_type == ZCC_KV_F16) row_bytes_kv = (uint64_t)h->model.kv_dim * 2ULL;
    else if (kv_type == ZCC_KV_Q8_0) row_bytes_kv = (uint64_t)(h->model.kv_dim >> 5) * 34ULL;

    h->prefill_batch = (h->aot_tier == 1) ? 1U : 32U;
    int alloc_w_f16_tmp = (h->aot_tier != 1) ? 1 : 0;

    size_t free_vram = 0U, total_vram = 0U;
    check_cu(cuMemGetInfo_v2(&free_vram, &total_vram), "cuMemGetInfo_v2");
    if (g_zcc_llm_failed) {
        zcc_llm_destroy(h);
        return NULL;
    }

    uint64_t max_dim = (uint64_t)h->model.dim;
    uint64_t max_hid = (uint64_t)h->model.hidden_dim;
    uint64_t max_kv_dim = (uint64_t)h->model.kv_dim;
    uint64_t max_vocab = (uint64_t)h->model.vocab_size;
    uint64_t ws_est = (uint64_t)h->prefill_batch * (max_dim * 24ULL + max_hid * 12ULL + max_kv_dim * 8ULL) +
                      max_vocab * 4ULL + 65536ULL;
    if (alloc_w_f16_tmp) {
        uint64_t max_mat = (max_hid > max_dim) ? (max_hid * max_dim) : (max_dim * max_dim);
        ws_est += max_mat * 2ULL;
    }

    uint64_t kv_each64 = 0ULL, kv_total64 = 0ULL;
    for (;;) {
        int ov = (mul_u64_checked((uint64_t)h->model.n_layers, (uint64_t)h->eff_max_seq, &kv_each64) != 0 ||
                  mul_u64_checked(kv_each64, row_bytes_kv, &kv_each64) != 0);
        if (!ov) kv_total64 = kv_each64 * 2ULL;
        if (ctx_len == 0 && h->aot_tier != 1 && h->eff_max_seq > 256U && free_vram > 0U &&
            (ov || kv_total64 > (uint64_t)free_vram || kv_total64 + ws_est > ((uint64_t)free_vram * 85ULL) / 100ULL)) {
            h->eff_max_seq >>= 1;
            continue;
        }
        if (ov || kv_total64 + ws_est > (uint64_t)free_vram) {
            zcc_llm_set_err("VRAM_BUDGET_EXCEEDED: insufficient free VRAM for KV cache");
            zcc_llm_destroy(h);
            return NULL;
        }
        break;
    }
    h->kv_bytes = (size_t)kv_each64;

    check_cu(cuMemAlloc_v2(&h->act.k_cache, h->kv_bytes), "act.k_cache");
    check_cu(cuMemAlloc_v2(&h->act.v_cache, h->kv_bytes), "act.v_cache");
    check_cu(cuMemAlloc_v2(&h->act.logits, (size_t)h->model.vocab_size * 4U), "act.logits");
    if (h->aot_tier != 1) {
        unsigned int max_k = (h->model.hidden_dim > h->model.dim) ? h->model.hidden_dim : h->model.dim;
        size_t pb = (size_t)h->prefill_batch;
        check_cu(cuMemAlloc_v2(&h->act.x, pb * (size_t)h->model.dim * 4U), "act.x");
        check_cu(cuMemAlloc_v2(&h->act.xb, pb * (size_t)h->model.dim * 4U), "act.xb");
        check_cu(cuMemAlloc_v2(&h->act.xb2, pb * (size_t)h->model.dim * 4U), "act.xb2");
        check_cu(cuMemAlloc_v2(&h->act.x_f16, pb * (size_t)max_k * 2U), "act.x_f16");
        check_cu(cuMemAlloc_v2(&h->act.q, pb * (size_t)h->model.dim * 4U), "act.q");
        check_cu(cuMemAlloc_v2(&h->act.k, pb * (size_t)h->model.kv_dim * 4U), "act.k");
        check_cu(cuMemAlloc_v2(&h->act.v, pb * (size_t)h->model.kv_dim * 4U), "act.v");
        if (!use_aot && h->model.head_dim > 256U) {
            check_cu(cuMemAlloc_v2(&h->act.att_scratch, (size_t)h->model.n_heads * h->eff_max_seq * 4U), "act.att");
        }
        check_cu(cuMemAlloc_v2(&h->act.hb, pb * (size_t)h->model.hidden_dim * 4U), "act.hb");
        check_cu(cuMemAlloc_v2(&h->act.hb2, pb * (size_t)h->model.hidden_dim * 4U), "act.hb2");
        if (!use_aot) check_cu(cuMemAlloc_v2(&h->act.probs, (size_t)h->model.vocab_size * 4U), "act.probs");
        if (h->aot_tier == 2) {
            check_cu(cuMemAlloc_v2(&h->act.step_args, 16U), "act.step_args");
            check_cu(cuMemAlloc_v2(&h->act.argmax_ids, 4U), "act.argmax_ids");
            check_cu(cuMemAlloc_v2(&h->act.argmax_vals, 4U), "act.argmax_vals");
            check_cu(cuMemAlloc_v2(&h->act.batch_toks, pb * 4U), "act.batch_toks");
        }
        if (alloc_w_f16_tmp) {
            size_t max_mat = ((size_t)h->model.hidden_dim > (size_t)h->model.dim)
                             ? (size_t)h->model.hidden_dim * (size_t)h->model.dim
                             : (size_t)h->model.dim * (size_t)h->model.dim;
            check_cu(cuMemAlloc_v2(&h->act.w_f16_tmp, max_mat * 2U), "act.w_f16_tmp");
            if (h->aot_tier == 0) check_cu(cuMemAlloc_v2(&h->act.prefill_tmp, pb * (size_t)h->model.dim * 4U), "act.prefill_tmp");
        }
    }
    if (g_zcc_llm_failed) {
        zcc_llm_destroy(h);
        return NULL;
    }

    if (h->aot_tier == 2) {
        mc_plan_init(&h->mc_plan, h->mod, &h->model, &h->act, 0, kv_type, 1);
        h->mc_plan_inited = 1;
    }

    check_cu(cuEventCreate(&h->ev0, 0U), "ev0");
    check_cu(cuEventCreate(&h->ev1, 0U), "ev1");
    if (g_zcc_llm_failed) {
        zcc_llm_destroy(h);
        return NULL;
    }

    h->h_logits = (float *)malloc((size_t)h->model.vocab_size * 4U);
    h->work_logits = (float *)malloc((size_t)h->model.vocab_size * 4U);
    h->h_probs = (float *)malloc((size_t)h->model.vocab_size * 4U);
    h->cands = (SampleCand *)malloc((size_t)h->model.vocab_size * sizeof(SampleCand));
    h->hist_cap = h->eff_max_seq + 128U;
    h->hist = (uint32_t *)malloc((size_t)h->hist_cap * sizeof(uint32_t));
    h->gen_ids_cap = (size_t)h->eff_max_seq + 8U;
    h->gen_ids = (int *)malloc(h->gen_ids_cap * sizeof(int));
    h->gen_text_cap = 4096U;
    h->gen_text = (char *)malloc(h->gen_text_cap);
    if (!h->h_logits || !h->work_logits || !h->h_probs || !h->cands ||
        !h->hist || !h->gen_ids || !h->gen_text) {
        zcc_llm_set_err("OOM allocating host inference scratch buffers");
        zcc_llm_destroy(h);
        return NULL;
    }
    h->gen_text[0] = '\0';
    return (void *)h;
}

int zcc_llm_reset(void *handle) {
    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';
    ZccLlmHandle *h = (ZccLlmHandle *)handle;
    if (!h || !h->model_inited) {
        zcc_llm_set_err("INVALID_HANDLE: NULL or uninitialized handle in zcc_llm_reset");
        return -1;
    }
    check_cu(cuMemsetD8_v2(h->act.k_cache, 0, h->kv_bytes), "reset_k");
    check_cu(cuMemsetD8_v2(h->act.v_cache, 0, h->kv_bytes), "reset_v");
    if (g_zcc_llm_failed) return -1;
    h->pos = 0U;
    h->hist_len = 0U;
    h->turn = 0;
    h->last_gen_tokens = 0;
    h->last_tps = 0.0;
    return 0;
}

int zcc_llm_get_stats(void *handle, ZccLlmStats *out_stats) {
    ZccLlmHandle *h = (ZccLlmHandle *)handle;
    if (!h || !h->model_inited || !out_stats) {
        zcc_llm_set_err("INVALID_ARG: NULL handle or out_stats in zcc_llm_get_stats");
        return -1;
    }
    out_stats->tok_per_sec = h->last_tps;
    out_stats->params = h->params;
    out_stats->n_layers = h->model.n_layers;
    out_stats->dim = h->model.dim;
    out_stats->vocab_size = h->model.vocab_size;
    out_stats->eff_max_seq = h->eff_max_seq;
    out_stats->pos = h->pos;
    out_stats->last_gen_tokens = h->last_gen_tokens;
    out_stats->aot_tier = (uint32_t)h->aot_tier;
    out_stats->kv_type = (uint32_t)h->kv_type;
    return 0;
}

int zcc_llm_generate(void *handle, const char *prompt, const ZccLlmGenConfig *cfg, char *out_buf) {
    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';
    ZccLlmHandle *h = (ZccLlmHandle *)handle;
    if (!h || !h->model_inited) {
        zcc_llm_set_err("INVALID_HANDLE: NULL handle passed to zcc_llm_generate");
        return -1;
    }
    if (!prompt || prompt[0] == '\0') {
        zcc_llm_set_err("INVALID_ARG: prompt is NULL or empty");
        return -1;
    }

    int max_steps = (cfg && cfg->max_steps > 0) ? cfg->max_steps : 64;
    int top_k = (cfg && cfg->top_k > 0) ? cfg->top_k : 40;
    int chat_mode = cfg ? cfg->chat_mode : 1;
    int ignore_eos = cfg ? cfg->ignore_eos : 0;
    float temp = cfg ? cfg->temp : 0.7f;
    float top_p = (cfg && cfg->top_p > 0.0f) ? cfg->top_p : 0.9f;
    float min_p = (cfg && cfg->min_p >= 0.0f) ? cfg->min_p : 0.0f;
    float repeat_penalty = (cfg && cfg->repeat_penalty > 0.0f) ? cfg->repeat_penalty : 1.1f;
    uint32_t seed = (cfg && cfg->seed != 0U) ? cfg->seed : 42U;
    uint32_t out_cap = cfg ? cfg->out_cap : 0U;
    zcc_llm_token_cb stream_cb = cfg ? cfg->stream_cb : NULL;
    void *userdata = cfg ? cfg->userdata : NULL;

    if (!(temp >= 0.0f && temp <= 100.0f) || !(top_p > 0.0f && top_p <= 1.0f) ||
        !(min_p >= 0.0f && min_p <= 1.0f) || !(repeat_penalty > 0.0f && repeat_penalty <= 100.0f)) {
        zcc_llm_set_err("INVALID_ARG: sampling hyperparameter out of bounds");
        return -1;
    }

    char *fmt_prompt = NULL;
    const char *tok_in = prompt;
    if (chat_mode != 0) {
        fmt_prompt = format_chat_prompt_alloc(&h->model, prompt, NULL, (h->turn > 0) ? 1 : 0);
        if (!fmt_prompt) return -1;
        tok_in = fmt_prompt;
    }

    uint32_t *t_ids = NULL;
    uint32_t t_len = 0U;
    int no_bos = (h->pos > 0U) ? 1 : 0;
    if (tokenize_prompt(&h->model, tok_in, no_bos, &t_ids, &t_len) != 0 || t_len == 0U) {
        if (fmt_prompt) free(fmt_prompt);
        zcc_llm_set_err("TOKENIZE_ERROR: empty or unencodable prompt");
        return -1;
    }
    if (fmt_prompt) { free(fmt_prompt); fmt_prompt = NULL; }

    if ((uint64_t)h->pos + (uint64_t)t_len >= (uint64_t)h->eff_max_seq) {
        free(t_ids);
        zcc_llm_set_err("CONTEXT_LIMIT_EXCEEDED: prompt overflows remaining KV context (call zcc_llm_reset)");
        return -1;
    }

    for (uint32_t i = 0U; i < t_len; i++) {
        uint32_t tok = t_ids[i];
        if (tok >= h->model.vocab_size) tok = 0U;
        if (h->hist_len < h->hist_cap) h->hist[h->hist_len++] = tok;
    }

    int room = (int)(h->eff_max_seq - h->pos - t_len);
    int turn_steps = (max_steps > room) ? room : max_steps;
    if (turn_steps < 0) turn_steps = 0;

    if ((size_t)turn_steps + 4U > h->gen_ids_cap) {
        size_t new_cap = (size_t)turn_steps + 16U;
        int *ng = (int *)realloc(h->gen_ids, new_cap * sizeof(int));
        if (!ng) {
            free(t_ids);
            zcc_llm_set_err("OOM resizing gen_ids");
            return -1;
        }
        h->gen_ids = ng;
        h->gen_ids_cap = new_cap;
    }

    check_cu(cuEventRecord(h->ev0, NULL), "dll_ev0");
    if (!g_zcc_llm_failed) {
        h->pos = run_prefill_active(h->aot_tier, h->fn_aot_fwd, h->aot_block_dim, h->mod,
                                    &h->mc_plan, &h->mc_pf, &h->model, &h->act,
                                    0, NULL, 0U, NULL, NULL, NULL, NULL,
                                    t_ids, t_len, h->pos, h->eff_max_seq,
                                    1, h->kv_type, (int)h->prefill_batch, NULL, NULL);
    }
    free(t_ids);
    if (!g_zcc_llm_failed) check_cu(cuCtxSynchronize(), "dll_prefill_sync");
    if (!g_zcc_llm_failed) check_cu(cuMemcpyDtoH_v2(h->h_logits, h->act.logits, (size_t)h->model.vocab_size * 4U), "dll_dtoh_logits");
    if (!g_zcc_llm_failed) check_finite_logits(h->h_logits, h->model.vocab_size, -1);
    if (g_zcc_llm_failed) return -1;

    int gen_count = 0;
    h->pos = run_decode_sequence_ex(h->aot_tier, h->fn_aot_fwd, h->aot_block_dim,
                                    &h->mc_plan, &h->model, &h->act,
                                    1, h->kv_type, turn_steps, h->pos, h->eff_max_seq,
                                    temp, top_k, top_p, min_p, repeat_penalty, ignore_eos,
                                    0, NULL, stream_cb, userdata, &seed,
                                    h->h_logits, h->work_logits, h->h_probs, h->cands,
                                    h->hist, &h->hist_len, h->gen_ids,
                                    &h->gen_text, &h->gen_text_cap, &gen_count);
    if (g_zcc_llm_failed) return -1;

    check_cu(cuEventRecord(h->ev1, NULL), "dll_ev1");
    check_cu(cuEventSynchronize(h->ev1), "dll_ev1_sync");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, h->ev0, h->ev1), "dll_elapsed");
    if (g_zcc_llm_failed) return -1;

    h->last_gen_tokens = gen_count;
    h->last_tps = (ms > 0.0f) ? (((double)(t_len + (uint32_t)gen_count) * 1000.0) / (double)ms) : 0.0;
    h->turn++;

    size_t text_len = h->gen_text ? strlen(h->gen_text) : 0U;
    if (out_buf && out_cap > 0U) {
        size_t n_copy = (text_len < (size_t)(out_cap - 1U)) ? text_len : (size_t)(out_cap - 1U);
        if (n_copy > 0U) memcpy(out_buf, h->gen_text, n_copy);
        out_buf[n_copy] = '\0';
        return (int)n_copy;
    }
    return (int)text_len;
}
