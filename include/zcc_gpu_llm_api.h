/*
 * ZKAEDI ZCC // Sovereign Win64 GPU GGUF Inference C ABI (Track 17)
 * ==================================================================
 * Pure C99 public API for zcc_gpu_llm.dll (compiled natively by ZCC
 * with ZERO MSVC, ZERO MinGW, ZERO CUDA Toolkit; System32 msvcrt+nvcuda ONLY).
 */

#ifndef ZCC_GPU_LLM_API_H
#define ZCC_GPU_LLM_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KV cache quantization modes passed to zcc_llm_create(..., kv_type, ...) */
#define ZCC_LLM_KV_F32   0
#define ZCC_LLM_KV_F16   1
#define ZCC_LLM_KV_Q8_0  2

/* Streaming token callback invoked on each decoded token piece. */
typedef void (*zcc_llm_token_cb)(const char *piece_utf8, int token_id, void *userdata);

/*
 * Exact 64-byte Win64 / C99 / .NET P/Invoke sequential configuration layout.
 * Zero hidden compiler padding between fields.
 */
typedef struct {
    int32_t max_steps;          /* offset 0:  max tokens to generate (<=0 -> 64) */
    int32_t top_k;              /* offset 4:  top-k cutoff (<=0 -> 40) */
    int32_t chat_mode;          /* offset 8:  1 = format with ChatML/Llama template */
    int32_t clean_stream;       /* offset 12: 1 = clean streaming callback only */
    int32_t ignore_eos;         /* offset 16: 1 = keep decoding past EOS */
    float temp;                 /* offset 20: sampling temperature (0.0 = greedy) */
    float top_p;                /* offset 24: nucleus cutoff (<=0 -> 0.9f) */
    float min_p;                /* offset 28: min-p threshold (>=0.0f) */
    float repeat_penalty;       /* offset 32: repetition penalty (<=0 -> 1.1f) */
    uint32_t seed;              /* offset 36: PRNG seed (0 -> 42U) */
    uint32_t out_cap;           /* offset 40: byte capacity of out_buf in zcc_llm_generate */
    uint32_t flags;             /* offset 44: reserved (0) */
    zcc_llm_token_cb stream_cb; /* offset 48: optional per-token callback */
    void *userdata;             /* offset 56: opaque pointer passed to stream_cb */
} ZccLlmGenConfig;              /* sizeof == 64 */

/*
 * Exact 48-byte Win64 / C99 / .NET P/Invoke telemetry struct.
 */
typedef struct {
    double tok_per_sec;         /* offset 0:  last generate() throughput */
    uint64_t params;            /* offset 8:  total loaded parameter count */
    uint32_t n_layers;          /* offset 16: transformer blocks */
    uint32_t dim;               /* offset 20: embedding width */
    uint32_t vocab_size;        /* offset 24: vocabulary size */
    uint32_t eff_max_seq;       /* offset 28: allocated KV context length */
    uint32_t pos;               /* offset 32: current KV cache token position */
    int32_t last_gen_tokens;    /* offset 36: tokens emitted in last generate() */
    uint32_t aot_tier;          /* offset 40: 0=generic PTX, 1=TierA, 2=TierB */
    uint32_t kv_type;           /* offset 44: 0=F32, 1=F16, 2=Q8_0 */
} ZccLlmStats;                  /* sizeof == 48 */

/* Create an in-process GPU LLM handle (NULL on error; inspect zcc_llm_last_error()). */
void *zcc_llm_create(const char *model_path, const char *aot_ptx_path, int kv_type, int ctx_len);

/* Run prefill + decode for prompt; returns bytes written to out_buf (<0 on error). */
int zcc_llm_generate(void *handle, const char *prompt, const ZccLlmGenConfig *cfg, char *out_buf);

/* Reset KV cache and conversation turn state to pos=0 (0 on success, -1 on error). */
int zcc_llm_reset(void *handle);

/* Query model geometry and last-turn telemetry (0 on success, -1 on error). */
int zcc_llm_get_stats(void *handle, ZccLlmStats *out_stats);

/* Return last error message in static storage (never NULL). */
const char *zcc_llm_last_error(void);

/* Unload CUDA module/context, free VRAM and host allocations. */
void zcc_llm_destroy(void *handle);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_GPU_LLM_API_H */
