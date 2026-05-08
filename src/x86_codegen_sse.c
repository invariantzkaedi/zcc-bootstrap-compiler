/*
 * x86_codegen_sse.c — SSE2 floating-point code generation
 * =======================================================
 * Emits double-precision scalar instructions (addsd, subsd, mulsd, divsd)
 * and type conversion instructions.
 */

#include <stdio.h>
#include <string.h>
#include "../ir.h"
#include "../regalloc.h"

void sse_emit_float_binop(FILE *out, const char *op, const char *dst, const char *src1, const char *src2) {
    /* If src1 is the same as dst, we can do op src2, dst */
    if (strcmp(dst, src1) == 0) {
        fprintf(out, "    %s %s, %s\n", op, src2, dst);
    } else if (strcmp(dst, src2) == 0) {
        /* Since floating point subtraction/division are not commutative, 
           if dst == src2, we must use a scratch register or swap.
           But usually regalloc ensures dst == src1 for binary ops if we enforce 2-address format, 
           or we just move src1 to dst then op src2.
           ZCC usually does: mov src1, dst; op src2, dst. */
        fprintf(out, "    movsd %s, %%xmm7\n", src1);
        fprintf(out, "    %s %s, %%xmm7\n", op, src2);
        fprintf(out, "    movsd %%xmm7, %s\n", dst);
    } else {
        fprintf(out, "    movsd %s, %s\n", src1, dst);
        fprintf(out, "    %s %s, %s\n", op, src2, dst);
    }
}

void sse_emit_cvtsi2sd(FILE *out, const char *dst, const char *src) {
    fprintf(out, "    cvtsi2sd %s, %s\n", src, dst);
}

void sse_emit_cvttsd2si(FILE *out, const char *dst, const char *src) {
    fprintf(out, "    cvttsd2si %s, %s\n", src, dst);
}
