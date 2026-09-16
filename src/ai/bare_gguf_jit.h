/* ========================================================================= */
/* ZCC BARE-GGUF: ZERO-DEPENDENCY BARE-METAL TRANSFORMER JIT EMITTER          */
/* ========================================================================= */
/* File: src/ai/bare_gguf_jit.h                                              */
/* Description: Direct compilation of GGUF quantized model weights into     */
/*              freestanding AVX2/AVX-512 FlashAttention & GEMV kernels.     */
/* ========================================================================= */

#ifndef ZCC_BARE_GGUF_JIT_H
#define ZCC_BARE_GGUF_JIT_H

#include "src/gguf_emit.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Quantized Q4_0 Block (32 weights per block: 16-bit float scale + 16 bytes) */
typedef struct {
    uint16_t d;        /* fp16 scale factor */
    uint8_t  qs[16];   /* 32x 4-bit nibbles */
} BareQ4Block;

/* Quantized Q8_0 Block (32 weights per block: 16-bit float scale + 32 bytes) */
typedef struct {
    uint16_t d;        /* fp16 scale factor */
    int8_t   qs[32];   /* 32x 8-bit quantized weights */
} BareQ8Block;

typedef struct {
    uint32_t dim;
    uint32_t hidden_dim;
    uint32_t n_heads;
    uint32_t n_layers;
    BareQ4Block *q4_weights;
    size_t       n_blocks;
} BareTransformerContext;

/* Vectorized Q4_0 Matrix-Vector Multiply Kernel (AVX2/FMA) */
void bare_gguf_gemv_q4_0(
    const BareQ4Block *weights,
    const float       *input_vec,
    float             *output_vec,
    uint32_t           rows,
    uint32_t           cols
);

/* Vectorized Fused FlashAttention-2 Head Step */
void bare_gguf_flash_attention_step(
    const float *q,       /* [head_dim] */
    const float *k_cache, /* [seq_len, head_dim] */
    const float *v_cache, /* [seq_len, head_dim] */
    float       *out,     /* [head_dim] */
    uint32_t     seq_len,
    uint32_t     head_dim
);

/* Generate native assembly emitter string for standalone deployment */
int bare_gguf_emit_native_assembly(const BareTransformerContext *ctx, char *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_BARE_GGUF_JIT_H */
