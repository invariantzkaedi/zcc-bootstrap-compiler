/* ========================================================================= */
/* ZCC AVX-512 HARDWARE-VECTORIZED LATTICE PHYSICS JIT COMPILER              */
/* ========================================================================= */
/* File: src/codegen/avxzkd_jit_x86.h                                       */
/* Description: Native x86-64 / AVX-512 FMA lattice JIT engine emitting      */
/*              zero-split-load, 48-cell interleaved kernel assembly.        */
/* ========================================================================= */

#ifndef ZCC_AVXZKD_JIT_X86_H
#define ZCC_AVXZKD_JIT_X86_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float eta;
    float gamma;
    float beta;
    float eps;
    float kick;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint64_t seed[4];
} AvxzkdJitParams;

typedef struct {
    uint8_t *code_buffer;
    size_t   code_size;
    size_t   capacity;
    bool     executable;
} AvxzkdJitContext;

typedef void (*AvxzkdJitKernelFn)(float *current, const float *base, float *scars, 
                                  uint32_t width, uint32_t height, uint32_t stride, 
                                  float eta, float gamma, float eps);

/* JIT Compiler lifecycle */
AvxzkdJitContext *avxzkd_jit_create(size_t capacity);
void avxzkd_jit_destroy(AvxzkdJitContext *ctx);

/* Emitters */
int avxzkd_jit_emit_48cell_fma_kernel(AvxzkdJitContext *ctx);
AvxzkdJitKernelFn avxzkd_jit_compile(AvxzkdJitContext *ctx);

/* Assembly text emitter for ZCC codegen pipeline */
int avxzkd_emit_assembly_text(char *out_buf, size_t buf_size, const AvxzkdJitParams *params);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_AVXZKD_JIT_X86_H */
