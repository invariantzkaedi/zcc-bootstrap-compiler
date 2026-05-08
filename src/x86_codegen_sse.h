#ifndef ZCC_X86_CODEGEN_SSE_H
#define ZCC_X86_CODEGEN_SSE_H

#include <stdio.h>

void sse_emit_float_binop(FILE *out, const char *op, const char *dst, const char *src1, const char *src2);
void sse_emit_cvtsi2sd(FILE *out, const char *dst, const char *src);
void sse_emit_cvttsd2si(FILE *out, const char *dst, const char *src);

#endif
