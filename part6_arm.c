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
        fprintf(cc->out, "    ldr r5, =%d\n", stack_size);
        fprintf(cc->out, "    mov r4, sp\n");
        fprintf(cc->out, "    subs r4, r4, r5\n");
        fprintf(cc->out, "    mov sp, r4\n");
    }

    int i;
    for (i = 0; i < func->num_params && i < 4; i++) {
        fprintf(cc->out, "    ldr r4, =%d\n", -(i * 4 + 8));
        fprintf(cc->out, "    adds r4, r7, r4\n");
        fprintf(cc->out, "    str r%d, [r4]\n", i);
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
        case ND_MOD:
            fprintf(cc->out, "    bl __aeabi_idivmod\n"); /* software divmod: r0=quot, r1=rem */
            fprintf(cc->out, "    movs r0, r1\n");        /* return remainder in r0 */
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

/* ================================================================ */
/* PART 6B: AARCH64 / ARM64 TARGET BACKEND (System V / AAPCS64)     */
/* ================================================================ */

static void aarch64_emit_prologue(Compiler *cc, Node *func) {
    int raw_stack = func->stack_size + 64;
    if (raw_stack < 256) raw_stack = 256;
    int aligned_stack = (raw_stack + 15) & ~15;

    fprintf(cc->out, "    .text\n");
    fprintf(cc->out, "    .arch armv8-a\n");
    if (!func->is_static) {
        fprintf(cc->out, "    .global %s\n", func->func_def_name);
    }
    fprintf(cc->out, "    .type %s, %%function\n", func->func_def_name);
    fprintf(cc->out, "%s:\n", func->func_def_name);
    fprintf(cc->out, "    stp x29, x30, [sp, #-%d]!\n", aligned_stack);
    fprintf(cc->out, "    mov x29, sp\n");

    /* AAPCS64: Parameter passing in x0-x7 spilled to local frame slots */
    int i;
    for (i = 0; i < func->num_params && i < 8; i++) {
        int off = -(i * 8 + 8);
        fprintf(cc->out, "    stur x%d, [x29, #%d]\n", i, off);
    }
}

static void aarch64_emit_epilogue(Compiler *cc, Node *func) {
    int raw_stack = func->stack_size + 64;
    if (raw_stack < 256) raw_stack = 256;
    int aligned_stack = (raw_stack + 15) & ~15;

    fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
    fprintf(cc->out, "    ldp x29, x30, [sp], #%d\n", aligned_stack);
    fprintf(cc->out, "    ret\n");
}

static void aarch64_emit_call(Compiler *cc, Node *func) {
    fprintf(cc->out, "    bl %s\n", func->func_name);
}

static void aarch64_emit_binary_op(Compiler *cc, int op) {
    switch (op) {
        case ND_ADD:
            fprintf(cc->out, "    add x0, x0, x1\n");
            break;
        case ND_SUB:
            fprintf(cc->out, "    sub x0, x0, x1\n");
            break;
        case ND_MUL:
            fprintf(cc->out, "    mul x0, x0, x1\n");
            break;
        case ND_DIV:
            fprintf(cc->out, "    sdiv x0, x0, x1\n");
            break;
        case ND_MOD:
            fprintf(cc->out, "    sdiv x2, x0, x1\n");
            fprintf(cc->out, "    msub x0, x2, x1, x0\n");
            break;
        case ND_BAND:
            fprintf(cc->out, "    and x0, x0, x1\n");
            break;
        case ND_BOR:
            fprintf(cc->out, "    orr x0, x0, x1\n");
            break;
        case ND_BXOR:
            fprintf(cc->out, "    eor x0, x0, x1\n");
            break;
        case ND_SHL:
            fprintf(cc->out, "    lsl x0, x0, x1\n");
            break;
        case ND_SHR:
            fprintf(cc->out, "    asr x0, x0, x1\n");
            break;
    }
}

static const char *aarch64_map_reg(const char *reg) {
    if (!reg) return "x0";
    if (strcmp(reg, "rax") == 0 || strcmp(reg, "eax") == 0 || strcmp(reg, "r0") == 0) return "x0";
    if (strcmp(reg, "rdi") == 0 || strcmp(reg, "edi") == 0 || strcmp(reg, "r1") == 0 || strcmp(reg, "r11") == 0) return "x1";
    if (strcmp(reg, "rsi") == 0 || strcmp(reg, "esi") == 0 || strcmp(reg, "r2") == 0) return "x2";
    if (strcmp(reg, "rdx") == 0 || strcmp(reg, "edx") == 0 || strcmp(reg, "r3") == 0) return "x3";
    if (strcmp(reg, "rcx") == 0 || strcmp(reg, "ecx") == 0 || strcmp(reg, "r4") == 0) return "x4";
    if (strcmp(reg, "r8") == 0 || strcmp(reg, "r5") == 0) return "x5";
    if (strcmp(reg, "r9") == 0 || strcmp(reg, "r6") == 0) return "x6";
    return reg;
}

static void aarch64_emit_load_stack(Compiler *cc, int offset, const char *reg) {
    const char *r = aarch64_map_reg(reg);
    if (offset >= -256 && offset < 0) {
        fprintf(cc->out, "    ldur %s, [x29, #%d]\n", r, offset);
    } else if (offset >= 0 && offset <= 4095) {
        fprintf(cc->out, "    ldr %s, [x29, #%d]\n", r, offset);
    } else {
        fprintf(cc->out, "    mov x16, #%d\n", offset);
        fprintf(cc->out, "    ldr %s, [x29, x16]\n", r);
    }
}

static void aarch64_emit_store_stack(Compiler *cc, int offset, const char *reg) {
    const char *r = aarch64_map_reg(reg);
    if (offset >= -256 && offset < 0) {
        fprintf(cc->out, "    stur %s, [x29, #%d]\n", r, offset);
    } else if (offset >= 0 && offset <= 4095) {
        fprintf(cc->out, "    str %s, [x29, #%d]\n", r, offset);
    } else {
        fprintf(cc->out, "    mov x16, #%d\n", offset);
        fprintf(cc->out, "    str %s, [x29, x16]\n", r);
    }
}

static void aarch64_emit_float_binop(Compiler *cc, int op) {
    switch (op) {
        case ND_FADD: fprintf(cc->out, "    fadd d0, d0, d1\n"); break;
        case ND_FSUB: fprintf(cc->out, "    fsub d0, d0, d1\n"); break;
        case ND_FMUL: fprintf(cc->out, "    fmul d0, d0, d1\n"); break;
        case ND_FDIV: fprintf(cc->out, "    fdiv d0, d0, d1\n"); break;
    }
}

static void aarch64_emit_push(Compiler *cc, const char *reg) {
    const char *r = aarch64_map_reg(reg);
    fprintf(cc->out, "    str %s, [sp, #-16]!\n", r);
}

static void aarch64_emit_pop(Compiler *cc, const char *reg) {
    const char *r = aarch64_map_reg(reg);
    fprintf(cc->out, "    ldr %s, [sp], #16\n", r);
}

static void aarch64_emit_load(Compiler *cc, Type *type) {
    if (!type || type->size >= 4 || type->kind == TY_PTR) fprintf(cc->out, "    ldr x0, [x0]\n");
    else if (type->size == 1) fprintf(cc->out, is_unsigned_type(type) ? "    ldrb w0, [x0]\n" : "    ldrsb x0, [x0]\n");
    else if (type->size == 2) fprintf(cc->out, is_unsigned_type(type) ? "    ldrh w0, [x0]\n" : "    ldrsh x0, [x0]\n");
}

static void aarch64_emit_store(Compiler *cc, Type *type) {
    if (!type || type->size >= 4 || type->kind == TY_PTR) fprintf(cc->out, "    str x1, [x0]\n");
    else if (type->size == 1) fprintf(cc->out, "    strb w1, [x0]\n");
    else if (type->size == 2) fprintf(cc->out, "    strh w1, [x0]\n");
    fprintf(cc->out, "    mov x0, x1\n");
}

static void aarch64_emit_addr(Compiler *cc, int folded_off) {
    if (folded_off < 0 && -folded_off <= 4095) fprintf(cc->out, "    sub x0, x29, #%d\n", -folded_off);
    else if (folded_off >= 0 && folded_off <= 4095) fprintf(cc->out, "    add x0, x29, #%d\n", folded_off);
    else {
        fprintf(cc->out, "    mov x16, #%d\n", folded_off);
        fprintf(cc->out, "    add x0, x29, x16\n");
    }
}

static void aarch64_emit_num(Compiler *cc, long long val) {
    if (val >= 0 && val <= 65535) fprintf(cc->out, "    mov x0, #%lld\n", val);
    else fprintf(cc->out, "    ldr x0, =%lld\n", val);
}

static void aarch64_emit_prep_rhs(Compiler *cc) {
    fprintf(cc->out, "    mov x1, x0\n");
}

static void aarch64_emit_cmp_zero(Compiler *cc) {
    fprintf(cc->out, "    cmp x0, #0\n");
}

TargetBackend backend_aarch64 = {
    8, /* ptr_size = 8 */
    aarch64_emit_prologue,
    aarch64_emit_epilogue,
    aarch64_emit_call,
    aarch64_emit_binary_op,
    aarch64_emit_load_stack,
    aarch64_emit_store_stack,
    aarch64_emit_float_binop,
    aarch64_emit_push,
    aarch64_emit_pop,
    aarch64_emit_load,
    aarch64_emit_store,
    aarch64_emit_addr,
    aarch64_emit_num,
    aarch64_emit_prep_rhs,
    aarch64_emit_cmp_zero
};

