#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* ================================================================ */
/* PART 6C: RISC-V 64-BIT TARGET BACKEND (RV64GC / psABI)           */
/* ================================================================ */

static void riscv_emit_prologue(Compiler *cc, Node *func) {
    int stack_size = func->stack_size + 64;
    if (stack_size < 256) stack_size = 256;
    stack_size = (stack_size + 15) & ~15;

    fprintf(cc->out, "    .text\n");
    fprintf(cc->out, "    .option pic\n");
    if (!func->is_static) {
        fprintf(cc->out, "    .globl %s\n", func->func_def_name);
    }
    fprintf(cc->out, "    .type %s, @function\n", func->func_def_name);
    fprintf(cc->out, "%s:\n", func->func_def_name);
    
    /* Save ra and s0/fp in dedicated 16-byte header, then set s0 and allocate locals below s0 */
    fprintf(cc->out, "    addi sp, sp, -16\n");
    fprintf(cc->out, "    sd ra, 8(sp)\n");
    fprintf(cc->out, "    sd s0, 0(sp)\n");
    fprintf(cc->out, "    mv s0, sp\n");
    if (stack_size <= 2048) {
        fprintf(cc->out, "    addi sp, sp, -%d\n", stack_size);
    } else {
        fprintf(cc->out, "    li t0, -%d\n", stack_size);
        fprintf(cc->out, "    add sp, sp, t0\n");
    }

    /* Store incoming register arguments into local stack frame below s0 */
    int i;
    for (i = 0; i < func->num_params && i < 8; i++) {
        int off = -8 * (i + 1);
        fprintf(cc->out, "    sd a%d, %d(s0)\n", i, off);
    }
}

static void riscv_emit_epilogue(Compiler *cc, Node *func) {
    (void)func;
    fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
    fprintf(cc->out, "    mv sp, s0\n");
    fprintf(cc->out, "    ld ra, 8(sp)\n");
    fprintf(cc->out, "    ld s0, 0(sp)\n");
    fprintf(cc->out, "    addi sp, sp, 16\n");
    fprintf(cc->out, "    ret\n");
}

static void riscv_emit_call(Compiler *cc, Node *func) {
    fprintf(cc->out, "    call %s\n", func->func_name);
}

static void riscv_emit_binary_op(Compiler *cc, int op) {
    int uns = (op >> 8) & 1;
    op &= 0xFF;
    switch (op) {
        case ND_ADD: fprintf(cc->out, "    add a0, a0, a1\n"); break;
        case ND_SUB: fprintf(cc->out, "    sub a0, a0, a1\n"); break;
        case ND_MUL: fprintf(cc->out, "    mul a0, a0, a1\n"); break;
        case ND_DIV: fprintf(cc->out, "    div a0, a0, a1\n"); break;
        case ND_MOD: fprintf(cc->out, "    rem a0, a0, a1\n"); break;
        case ND_BAND: fprintf(cc->out, "    and a0, a0, a1\n"); break;
        case ND_BOR: fprintf(cc->out, "    or a0, a0, a1\n"); break;
        case ND_BXOR: fprintf(cc->out, "    xor a0, a0, a1\n"); break;
        case ND_SHL: fprintf(cc->out, "    sll a0, a0, a1\n"); break;
        case ND_SHR: fprintf(cc->out, "    sra a0, a0, a1\n"); break;
        case ND_EQ:
            fprintf(cc->out, "    sub a0, a0, a1\n    seqz a0, a0\n");
            break;
        case ND_NE:
            fprintf(cc->out, "    sub a0, a0, a1\n    snez a0, a0\n");
            break;
        case ND_LT:
            fprintf(cc->out, uns ? "    sltu a0, a0, a1\n" : "    slt a0, a0, a1\n");
            break;
        case ND_LE:
            fprintf(cc->out, uns ? "    sltu a0, a1, a0\n    xori a0, a0, 1\n" : "    slt a0, a1, a0\n    xori a0, a0, 1\n");
            break;
        case ND_GT:
            fprintf(cc->out, uns ? "    sltu a0, a1, a0\n" : "    slt a0, a1, a0\n");
            break;
        case ND_GE:
            fprintf(cc->out, uns ? "    sltu a0, a0, a1\n    xori a0, a0, 1\n" : "    slt a0, a0, a1\n    xori a0, a0, 1\n");
            break;
    }
}

static const char *riscv_map_reg(const char *reg) {
    if (!reg) return "a0";
    if (reg[0] == '%') reg++;
    if (strcmp(reg, "rax") == 0 || strcmp(reg, "eax") == 0 || strcmp(reg, "ax") == 0 || strcmp(reg, "al") == 0 || strcmp(reg, "r0") == 0 || strcmp(reg, "x0") == 0) return "a0";
    if (strcmp(reg, "rdi") == 0 || strcmp(reg, "edi") == 0 || strcmp(reg, "di") == 0 || strcmp(reg, "dil") == 0 || strcmp(reg, "r1") == 0 || strcmp(reg, "x1") == 0 || strcmp(reg, "r11") == 0) return "a1";
    if (strcmp(reg, "rsi") == 0 || strcmp(reg, "esi") == 0 || strcmp(reg, "si") == 0 || strcmp(reg, "sil") == 0 || strcmp(reg, "r2") == 0 || strcmp(reg, "x2") == 0) return "a2";
    if (strcmp(reg, "rdx") == 0 || strcmp(reg, "edx") == 0 || strcmp(reg, "dx") == 0 || strcmp(reg, "dl") == 0 || strcmp(reg, "r3") == 0 || strcmp(reg, "x3") == 0) return "a3";
    if (strcmp(reg, "rcx") == 0 || strcmp(reg, "ecx") == 0 || strcmp(reg, "cx") == 0 || strcmp(reg, "cl") == 0 || strcmp(reg, "r4") == 0 || strcmp(reg, "x4") == 0) return "a4";
    if (strcmp(reg, "r8") == 0 || strcmp(reg, "r8d") == 0 || strcmp(reg, "r5") == 0 || strcmp(reg, "x5") == 0) return "a5";
    if (strcmp(reg, "r9") == 0 || strcmp(reg, "r9d") == 0 || strcmp(reg, "r6") == 0 || strcmp(reg, "x6") == 0) return "a6";
    return reg;
}

static void riscv_emit_load_stack(Compiler *cc, int offset, const char *reg) {
    const char *r = riscv_map_reg(reg);
    if (offset >= -2048 && offset <= 2047) {
        fprintf(cc->out, "    ld %s, %d(s0)\n", r, offset);
    } else {
        fprintf(cc->out, "    li t0, %d\n", offset);
        fprintf(cc->out, "    add t0, s0, t0\n");
        fprintf(cc->out, "    ld %s, 0(t0)\n", r);
    }
}

static void riscv_emit_store_stack(Compiler *cc, int offset, const char *reg) {
    const char *r = riscv_map_reg(reg);
    if (offset >= -2048 && offset <= 2047) {
        fprintf(cc->out, "    sd %s, %d(s0)\n", r, offset);
    } else {
        fprintf(cc->out, "    li t0, %d\n", offset);
        fprintf(cc->out, "    add t0, s0, t0\n");
        fprintf(cc->out, "    sd %s, 0(t0)\n", r);
    }
}

static void riscv_emit_float_binop(Compiler *cc, int op) {
    switch (op) {
        case ND_FADD: fprintf(cc->out, "    fadd.d fa0, fa0, fa1\n"); break;
        case ND_FSUB: fprintf(cc->out, "    fsub.d fa0, fa0, fa1\n"); break;
        case ND_FMUL: fprintf(cc->out, "    fmul.d fa0, fa0, fa1\n"); break;
        case ND_FDIV: fprintf(cc->out, "    fdiv.d fa0, fa0, fa1\n"); break;
    }
}

static void riscv_emit_push(Compiler *cc, const char *reg) {
    const char *r = riscv_map_reg(reg);
    fprintf(cc->out, "    addi sp, sp, -16\n");
    fprintf(cc->out, "    sd %s, 0(sp)\n", r);
}

static void riscv_emit_pop(Compiler *cc, const char *reg) {
    const char *r = riscv_map_reg(reg);
    fprintf(cc->out, "    ld %s, 0(sp)\n", r);
    fprintf(cc->out, "    addi sp, sp, 16\n");
}

static void riscv_emit_load(Compiler *cc, Type *type) {
    if (!type || type->size >= 8 || type->kind == TY_PTR) fprintf(cc->out, "    ld a0, 0(a0)\n");
    else if (type->size == 4) fprintf(cc->out, is_unsigned_type(type) ? "    lwu a0, 0(a0)\n" : "    lw a0, 0(a0)\n");
    else if (type->size == 1) fprintf(cc->out, is_unsigned_type(type) ? "    lbu a0, 0(a0)\n" : "    lb a0, 0(a0)\n");
    else if (type->size == 2) fprintf(cc->out, is_unsigned_type(type) ? "    lhu a0, 0(a0)\n" : "    lh a0, 0(a0)\n");
}

static void riscv_emit_store(Compiler *cc, Type *type) {
    if (!type || type->size >= 8 || type->kind == TY_PTR) fprintf(cc->out, "    sd a1, 0(a0)\n");
    else if (type->size == 4) fprintf(cc->out, "    sw a1, 0(a0)\n");
    else if (type->size == 1) fprintf(cc->out, "    sb a1, 0(a0)\n");
    else if (type->size == 2) fprintf(cc->out, "    sh a1, 0(a0)\n");
    fprintf(cc->out, "    mv a0, a1\n");
}

static void riscv_emit_addr(Compiler *cc, int folded_off) {
    if (folded_off >= -2048 && folded_off <= 2047) {
        fprintf(cc->out, "    addi a0, s0, %d\n", folded_off);
    } else {
        fprintf(cc->out, "    li a0, %d\n", folded_off);
        fprintf(cc->out, "    add a0, s0, a0\n");
    }
}

static void riscv_emit_num(Compiler *cc, long long val) {
    fprintf(cc->out, "    li a0, %lld\n", val);
}

static void riscv_emit_prep_rhs(Compiler *cc) {
    fprintf(cc->out, "    mv a1, a0\n");
}

static void riscv_emit_cmp_zero(Compiler *cc) {
    fprintf(cc->out, "    snez a0, a0\n");
}

static void riscv_emit_div(Compiler *cc, int is_unsigned) {
    fprintf(cc->out, is_unsigned ? "    divu a0, a0, a1\n" : "    div a0, a0, a1\n");
}

static void riscv_emit_mod(Compiler *cc, int is_unsigned) {
    fprintf(cc->out, is_unsigned ? "    remu a0, a0, a1\n" : "    rem a0, a0, a1\n");
}

static void riscv_emit_shr(Compiler *cc, int is_unsigned) {
    fprintf(cc->out, is_unsigned ? "    srl a0, a0, a1\n" : "    sra a0, a0, a1\n");
}

TargetBackend backend_riscv = {
    8, /* ptr_size = 8 */
    riscv_emit_prologue,
    riscv_emit_epilogue,
    riscv_emit_call,
    riscv_emit_binary_op,
    riscv_emit_load_stack,
    riscv_emit_store_stack,
    riscv_emit_float_binop,
    riscv_emit_push,
    riscv_emit_pop,
    riscv_emit_load,
    riscv_emit_store,
    riscv_emit_addr,
    riscv_emit_num,
    riscv_emit_prep_rhs,
    riscv_emit_cmp_zero,
    riscv_emit_div,
    riscv_emit_mod,
    riscv_emit_shr,
    "j" /* jump_mnemonic */
};

/* ================================================================ */
/* PART 6D: CROSS-BACKEND RESIDUAL ARM32 INSTRUCTION NORMALIZER     */
/* ================================================================ */

void backend_normalize_asm(char **line_ptrs, int nlines) {
    if (!backend_ops || !line_ptrs) return;
    int is_a64 = (backend_ops == &backend_aarch64);
    int is_rv64 = (backend_ops == &backend_riscv);
    if (!is_a64 && !is_rv64) return;

    int i;
    for (i = 0; i < nlines; i++) {
        char *l = line_ptrs[i];
        if (!l || l[0] == 0) continue;
        int v = 0;

        /* 1. Eliminate spurious 8-byte stack alignment pads on 16-byte push backends */
        if (strcmp(l, "    sub sp, sp, #8\n") == 0 || strcmp(l, "    add sp, sp, #8\n") == 0) {
            l[0] = 0;
            continue;
        }

        if (is_a64) {
            if (strcmp(l, "    mov r1, r0\n") == 0 || strcmp(l, "    movs r1, r0\n") == 0) {
                strcpy(l, "    mov x1, x0\n");
            } else if (strcmp(l, "    mov r0, r1\n") == 0 || strcmp(l, "    movs r0, r1\n") == 0) {
                strcpy(l, "    mov x0, x1\n");
            } else if (strcmp(l, "    mov r0, r3\n") == 0 || strcmp(l, "    movs r0, r3\n") == 0) {
                strcpy(l, "    mov x0, x3\n");
            } else if (strcmp(l, "    mov r3, r0\n") == 0 || strcmp(l, "    movs r3, r0\n") == 0) {
                strcpy(l, "    mov x3, x0\n");
            } else if (strcmp(l, "    cmp r0, #0\n") == 0) {
                strcpy(l, "    cmp x0, #0\n");
            } else if (strcmp(l, "    cmp r0, r1\n") == 0) {
                strcpy(l, "    cmp x0, x1\n");
            } else if (strcmp(l, "    adds r0, r0, r3\n") == 0 || strcmp(l, "    add r0, r0, r3\n") == 0) {
                strcpy(l, "    add x0, x0, x3\n");
            } else if (strcmp(l, "    subs r0, r0, r3\n") == 0 || strcmp(l, "    sub r0, r0, r3\n") == 0) {
                strcpy(l, "    sub x0, x0, x3\n");
            } else if (strcmp(l, "    muls r1, r3, r1\n") == 0) {
                strcpy(l, "    mul x1, x3, x1\n");
            } else if (strcmp(l, "    muls r0, r3, r0\n") == 0) {
                strcpy(l, "    mul x0, x3, x0\n");
            } else if (strcmp(l, "    str r1, [r0]\n") == 0) {
                strcpy(l, "    str x1, [x0]\n");
            } else if (strcmp(l, "    strb r1, [r0]\n") == 0) {
                strcpy(l, "    strb w1, [x0]\n");
            } else if (strcmp(l, "    strh r1, [r0]\n") == 0) {
                strcpy(l, "    strh w1, [x0]\n");
            } else if (strcmp(l, "    uxtb r0, r0\n") == 0) {
                strcpy(l, "    uxtb w0, w0\n");
            } else if (strcmp(l, "    sxtb r0, r0\n") == 0) {
                strcpy(l, "    sxtb x0, w0\n");
            } else if (strcmp(l, "    uxth r0, r0\n") == 0) {
                strcpy(l, "    uxth w0, w0\n");
            } else if (strcmp(l, "    sxth r0, r0\n") == 0) {
                strcpy(l, "    sxth x0, w0\n");
            } else if (sscanf(l, "    lsl r1, r1, #%d", &v) == 1) {
                sprintf(l, "    lsl x1, x1, #%d\n", v);
            } else if (sscanf(l, "    lsl r0, r0, #%d", &v) == 1) {
                sprintf(l, "    lsl x0, x0, #%d\n", v);
            } else if (sscanf(l, "    ldr r3, =%d", &v) == 1) {
                sprintf(l, "    mov x3, #%d\n", v);
            } else if (sscanf(l, "    ldr r1, =%d", &v) == 1) {
                sprintf(l, "    mov x1, #%d\n", v);
            } else if (sscanf(l, "    ldr r0, =%d", &v) == 1) {
                sprintf(l, "    mov x0, #%d\n", v);
            } else if (sscanf(l, "    movs r0, #%d", &v) == 1) {
                sprintf(l, "    mov x0, #%d\n", v);
            } else if (sscanf(l, "    movs r3, #%d", &v) == 1) {
                sprintf(l, "    mov x3, #%d\n", v);
            }
        } else if (is_rv64) {
            if (strcmp(l, "    mov r1, r0\n") == 0 || strcmp(l, "    movs r1, r0\n") == 0) {
                strcpy(l, "    mv a1, a0\n");
            } else if (strcmp(l, "    mov r0, r1\n") == 0 || strcmp(l, "    movs r0, r1\n") == 0) {
                strcpy(l, "    mv a0, a1\n");
            } else if (strcmp(l, "    mov r0, r3\n") == 0 || strcmp(l, "    movs r0, r3\n") == 0) {
                strcpy(l, "    mv a0, a3\n");
            } else if (strcmp(l, "    mov r3, r0\n") == 0 || strcmp(l, "    movs r3, r0\n") == 0) {
                strcpy(l, "    mv a3, a0\n");
            } else if (strcmp(l, "    cmp r0, #0\n") == 0 || strcmp(l, "    cmp x0, #0\n") == 0) {
                strcpy(l, "    snez a0, a0\n");
            } else if (strcmp(l, "    adds r0, r0, r3\n") == 0 || strcmp(l, "    add r0, r0, r3\n") == 0) {
                strcpy(l, "    add a0, a0, a3\n");
            } else if (strcmp(l, "    subs r0, r0, r3\n") == 0 || strcmp(l, "    sub r0, r0, r3\n") == 0) {
                strcpy(l, "    sub a0, a0, a3\n");
            } else if (strcmp(l, "    muls r1, r3, r1\n") == 0) {
                strcpy(l, "    mul a1, a3, a1\n");
            } else if (strcmp(l, "    muls r0, r3, r0\n") == 0) {
                strcpy(l, "    mul a0, a3, a0\n");
            } else if (strcmp(l, "    str r1, [r0]\n") == 0) {
                strcpy(l, "    sd a1, 0(a0)\n");
            } else if (strcmp(l, "    strb r1, [r0]\n") == 0) {
                strcpy(l, "    sb a1, 0(a0)\n");
            } else if (strcmp(l, "    strh r1, [r0]\n") == 0) {
                strcpy(l, "    sh a1, 0(a0)\n");
            } else if (strcmp(l, "    uxtb r0, r0\n") == 0) {
                strcpy(l, "    andi a0, a0, 255\n");
            } else if (strcmp(l, "    sxtb r0, r0\n") == 0) {
                strcpy(l, "    slli a0, a0, 56\n");
            } else if (strcmp(l, "    uxth r0, r0\n") == 0) {
                strcpy(l, "    slli a0, a0, 48\n");
            } else if (strcmp(l, "    sxth r0, r0\n") == 0) {
                strcpy(l, "    slli a0, a0, 48\n");
            } else if (sscanf(l, "    lsl r1, r1, #%d", &v) == 1) {
                sprintf(l, "    slli a1, a1, %d\n", v);
            } else if (sscanf(l, "    lsl r0, r0, #%d", &v) == 1) {
                sprintf(l, "    slli a0, a0, %d\n", v);
            } else if (sscanf(l, "    ldr r3, =%d", &v) == 1) {
                sprintf(l, "    li a3, %d\n", v);
            } else if (sscanf(l, "    ldr r1, =%d", &v) == 1) {
                sprintf(l, "    li a1, %d\n", v);
            } else if (sscanf(l, "    ldr r0, =%d", &v) == 1) {
                sprintf(l, "    li a0, %d\n", v);
            } else if (sscanf(l, "    movs r0, #%d", &v) == 1 || sscanf(l, "    mov x0, #%d", &v) == 1) {
                sprintf(l, "    li a0, %d\n", v);
            } else if (sscanf(l, "    movs r3, #%d", &v) == 1 || sscanf(l, "    mov x3, #%d", &v) == 1) {
                sprintf(l, "    li a3, %d\n", v);
            } else if (sscanf(l, "    beq .L%d", &v) == 1) {
                sprintf(l, "    beqz a0, .L%d\n", v);
            } else if (sscanf(l, "    bne .L%d", &v) == 1) {
                sprintf(l, "    bnez a0, .L%d\n", v);
            } else if (sscanf(l, "    b .L%d", &v) == 1) {
                sprintf(l, "    j .L%d\n", v);
            }
        }
    }
}
