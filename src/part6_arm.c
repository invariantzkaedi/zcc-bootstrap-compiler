#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* ================================================================ */
/* PART 6: ARM TARGET BACKEND (thumbv6m)                             */
/* ================================================================ */


TargetBackend *backend_ops = 0;
int ZCC_POINTER_WIDTH = 8;
int ZCC_INT_WIDTH = 4;

static void thumb_emit_prologue(Compiler *cc, Node *func) {
    int stack_size = func->stack_size + 40;
    if (stack_size < 256) stack_size = 256;
    stack_size = (stack_size + 7) & ~7;

    fprintf(cc->out, "    .text\n");
    fprintf(cc->out, "    .syntax unified\n");
    fprintf(cc->out, "    .cpu cortex-m0plus\n");
    fprintf(cc->out, "    .thumb\n");
    if (!func->is_static) {
        fprintf(cc->out, "    .global %s\n", func->func_def_name);
    }
    fprintf(cc->out, "    .type %s, %%function\n", func->func_def_name);
    fprintf(cc->out, "%s:\n", func->func_def_name);
    
    fprintf(cc->out, "    push {r4, r5, r6, r7, lr}\n");
    fprintf(cc->out, "    mov r7, sp\n");

    if (stack_size <= 508 && (stack_size % 4 == 0)) {
        fprintf(cc->out, "    sub sp, #%d\n", stack_size);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", stack_size);
        fprintf(cc->out, "    mov r4, sp\n");
        fprintf(cc->out, "    subs r4, r4, r3\n");
        fprintf(cc->out, "    mov sp, r4\n");
    }

    int i;
    for (i = 0; i < func->num_params && i < 4; i++) {
        fprintf(cc->out, "    ldr r3, =%d\n", -(i * 4 + 8));
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    str r%d, [r3]\n", i);
    }
}

static void thumb_emit_epilogue(Compiler *cc, Node *func) {
    fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
    fprintf(cc->out, "    mov sp, r7\n");
    fprintf(cc->out, "    pop {r4, r5, r6, r7, pc}\n");
}

static void thumb_emit_call(Compiler *cc, Node *func) {
    fprintf(cc->out, "    bl %s\n", func->func_name);
}

static void thumb_emit_binary_op(Compiler *cc, int op) {
    /* op matches ND_ADD, ND_SUB, etc.
       r0 = lhs, r1 = rhs
       output -> r0 */
    switch (op) {
        case ND_ADD:
            fprintf(cc->out, "    adds r0, r0, r1\n");
            break;
        case ND_SUB:
            fprintf(cc->out, "    subs r0, r0, r1\n");
            break;
        case ND_MUL:
            fprintf(cc->out, "    muls r0, r1, r0\n"); /* thumb-1 allows only dest=lhs */
            break;
        case ND_DIV:
            fprintf(cc->out, "    bl __aeabi_idiv\n"); /* software divide */
            break;
        case ND_BAND:
            fprintf(cc->out, "    ands r0, r0, r1\n");
            break;
        case ND_BOR:
            fprintf(cc->out, "    orrs r0, r0, r1\n");
            break;
        case ND_BXOR:
            fprintf(cc->out, "    eors r0, r0, r1\n");
            break;
        case ND_SHL:
            fprintf(cc->out, "    lsls r0, r0, r1\n");
            break;
        case ND_SHR:
            fprintf(cc->out, "    asrs r0, r0, r1\n"); /* arithmetic shift right */
            break;
    }
}

static void thumb_emit_load_stack(Compiler *cc, int offset, const char *reg) {
    if (offset >= 0 && offset <= 1020 && (offset % 4 == 0)) {
        fprintf(cc->out, "    ldr %s, [r7, #%d]\n", reg, offset);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", offset);
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    ldr %s, [r3]\n", reg);
    }
}

static void thumb_emit_store_stack(Compiler *cc, int offset, const char *reg) {
    if (offset >= 0 && offset <= 1020 && (offset % 4 == 0)) {
        fprintf(cc->out, "    str %s, [r7, #%d]\n", reg, offset);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", offset);
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    str %s, [r3]\n", reg);
    }
}

static void thumb_emit_float_binop(Compiler *cc, int op) {
    const char *fn = 0;
    switch (op) {
        case ND_FADD: fn = "__aeabi_fadd"; break;
        case ND_FSUB: fn = "__aeabi_fsub"; break;
        case ND_FMUL: fn = "__aeabi_fmul"; break;
        case ND_FDIV: fn = "__aeabi_fdiv"; break;
    }
    if (fn) {
        fprintf(cc->out, "    bl %s\n", fn);
    }
}

TargetBackend backend_thumbv6m = {
    4, /* ptr_size */
    thumb_emit_prologue,
    thumb_emit_epilogue,
    thumb_emit_call,
    thumb_emit_binary_op,
    thumb_emit_load_stack,
    thumb_emit_store_stack,
    thumb_emit_float_binop
};
