/*
 * ir_to_x86.c — ZCC IR-to-x86_64 Lowering Backend
 *
 * Walks the ir_module_t and generates System V x86-64 assembly,
 * completely replacing the AST-based codegen in part4.c.
 *
 * v2: Linear-scan register allocator (regalloc.c) assigns physical
 *     registers to %tN temporaries where possible, eliminating
 *     redundant load/store pairs.  %stack_* locals are unchanged.
 */

#include "ir.h"
#include "regalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *preg_name32(PhysReg r) {
    if (r == PREG_RBX) return "ebx";
    if (r == PREG_R12) return "r12d";
    if (r == PREG_R13) return "r13d";
    if (r == PREG_R14) return "r14d";
    if (r == PREG_R15) return "r15d";
    if (r == PREG_R10) return "r10d";
    if (r == PREG_R11) return "r11d";
    return preg_name(r);
}
typedef struct {
    char name[IR_NAME_MAX];
    int offset;
} ir_var_t;

static ir_var_t *vars = 0;
static int max_vars = 0;
static int num_vars = 0;
static int next_offset_from_rbp = -8;

static int get_or_create_var(const char *name) {
    int i;
    if (!name || name[0] == '\0' || name[0] == '-') return 0;
    
    if (strncmp(name, "%stack_", 7) == 0) {
        return atoi(name + 7);
    }
    
    for (i = 0; i < num_vars; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].offset;
    }
    
    if (num_vars >= max_vars) {
        max_vars = (max_vars == 0) ? 2048 : max_vars * 2;
        ir_var_t *new_vars = (ir_var_t *)realloc(vars, max_vars * sizeof(ir_var_t));
        if (!new_vars) {
            fprintf(stderr, "\n[FATAL] IR Backend: Variable limit exceeded and out of memory!\n");
            fprintf(stderr, "Tried to allocate %d variables.\n", max_vars);
            exit(1);
        }
        vars = new_vars;
    }
    
    int off;
    strncpy(vars[num_vars].name, name, IR_NAME_MAX - 1);
    vars[num_vars].name[IR_NAME_MAX - 1] = '\0';
    vars[num_vars].offset = next_offset_from_rbp;
    off = next_offset_from_rbp;
    next_offset_from_rbp -= 8;
    num_vars++;
    return off;
}

/* ── Register-aware operand helpers ──────────────────────────────────── */

/*
 * Emit code to load operand `src` into x86 register `dst_reg`.
 * If `src` has a physical register assigned by the allocator, emit a
 * register-to-register move.  Otherwise fall back to the stack slot.
 *
 * `ra` may be NULL (disabled / non-IR path).
 */
static void load_operand(FILE *out, const char *src, const char *dst_reg,
                         const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, src);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, dst_reg + 1) != 0) /* avoid "movq %rbx, %rbx" */
                fprintf(out, "    movq %%%s, %s\n", pname, dst_reg);
            return;
        }
    }
    /* Stack slot fallback */
    int off = get_or_create_var(src);
    fprintf(out, "    movq %d(%%rbp), %s\n", off, dst_reg);
}

/*
 * Emit code to store x86 register `src_reg` into the location for `dst`.
 * If `dst` has a physical register, emit reg→reg.  Otherwise stack slot.
 */
static void store_result(FILE *out, const char *dst, const char *src_reg,
                         const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, dst);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, src_reg + 1) != 0)
                fprintf(out, "    movq %s, %%%s\n", src_reg, pname);
            return;
        }
    }
    int off = get_or_create_var(dst);
    fprintf(out, "    movq %s, %d(%%rbp)\n", src_reg, off);
}

static void load_operand_xmm(FILE *out, const char *src, const char *dst_reg, const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, src);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, dst_reg + 1) != 0) {
                if (pr >= PREG_XMM0) fprintf(out, "    movsd %%%s, %s\n", pname, dst_reg);
                else                 fprintf(out, "    movq %%%s, %s\n", pname, dst_reg);
            }
            return;
        }
    }
    int off = get_or_create_var(src);
    fprintf(out, "    movsd %d(%%rbp), %s\n", off, dst_reg);
}

static void store_result_xmm(FILE *out, const char *dst, const char *src_reg, const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, dst);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, src_reg + 1) != 0) {
                if (pr >= PREG_XMM0) fprintf(out, "    movsd %s, %%%s\n", src_reg, pname);
                else                 fprintf(out, "    movq %s, %%%s\n", src_reg, pname);
            }
            return;
        }
    }
    int off = get_or_create_var(dst);
    fprintf(out, "    movsd %s, %d(%%rbp)\n", src_reg, off);
}

/*
 * Emit a binary arithmetic operand for src2: either a direct memory
 * operand (e.g. "addq -16(%rbp), %rax") or a register-to-register op.
 * Returns non-zero if the op was emitted inline; the caller does NOT
 * need to do anything else for src2.
 *
 * Only used for commutative/simple binary ops (ADD, SUB, MUL, AND, OR, XOR).
 */
static void emit_src2(FILE *out, const char *mnemonic, const char *src2,
                      const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, src2);
        if (pr != PREG_NONE) {
            fprintf(out, "    %s %%%s, %%rax\n", mnemonic, preg_name(pr));
            return;
        }
    }
    int off2 = get_or_create_var(src2);
    fprintf(out, "    %s %d(%%rbp), %%rax\n", mnemonic, off2);
}

/* ── Address-load helper (unchanged from v1) ─────────────────────────── */

static void load_address(FILE *out, const char *src, const char *reg) {
    if (strncmp(src, "%stack_", 7) == 0) {
        int off = get_or_create_var(src);
        fprintf(out, "    leaq %d(%%rbp), %s\n", off, reg);
    } else if (strncmp(src, "%t", 2) == 0) {
        /* A temp holding an address: load its value (the pointer) */
        int off = get_or_create_var(src);
        fprintf(out, "    movq %d(%%rbp), %s\n", off, reg);
    } else {
        const char *name = src;
        if (name[0] == '%') name++; /* remove '%' prefix for globals */
        fprintf(out, "    leaq %s(%%rip), %s\n", name, reg);
    }
}

/* Address-load that is register-aware for %t temporaries holding pointers */
static void load_address_ra(FILE *out, const char *src, const char *reg,
                             const RegAllocator *ra) {
    if (strncmp(src, "%t", 2) == 0 && ra) {
        PhysReg pr = ra_get(ra, src);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, reg + 1) != 0)
                fprintf(out, "    movq %%%s, %s\n", pname, reg);
            return;
        }
    }
    load_address(out, src, reg);
}

/* Callee-save register tables — match get_callee_reg() order in part4.c
 * r12=0, r13=1, r14=2, r15=3, rbx=4 */
static const char *callee_regs[5] = {"%r12", "%r13", "%r14", "%r15", "%rbx"};
static const int   callee_flag[5] = {PREG_R12, PREG_R13, PREG_R14, PREG_R15, PREG_RBX};

static int is_unsigned_comparison(ir_func_t *fn, ir_node_t *n) {
    if (ir_type_unsigned(n->type)) return 1;
    ir_node_t *def1 = ir_find_def(fn, n, n->src1);
    ir_node_t *def2 = ir_find_def(fn, n, n->src2);
    if (def1 && ir_type_unsigned(def1->type)) return 1;
    if (def2 && ir_type_unsigned(def2->type)) return 1;
    return 0;
}

/* ── Main lowering entry point ───────────────────────────────────────── */

void ir_module_lower_x86(const ir_module_t *mod, FILE *out) {
    int i;
    const char *arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

    fprintf(out, "    .text\n");
    
    for (i = 0; i < mod->func_count; i++) {
        ir_func_t *fn = mod->funcs[i];
        ir_node_t *n;
        int stack_size;
        int arg_idx;
        int call_padding = 0;

        num_vars = 0;
        {
            int min_stack_off = -8 * fn->num_params;
            ir_node_t *scan = fn->head;
            while (scan) {
                if (scan->flags & IRF_DEAD) {
                    scan = scan->next;
                    continue;
                }
                if (strncmp(scan->dst, "%stack_", 7) == 0) {
                    int off = atoi(scan->dst + 7);
                    if (off < min_stack_off) min_stack_off = off;
                }
                if (strncmp(scan->src1, "%stack_", 7) == 0) {
                    int off = atoi(scan->src1 + 7);
                    if (off < min_stack_off) min_stack_off = off;
                }
                if (strncmp(scan->src2, "%stack_", 7) == 0) {
                    int off = atoi(scan->src2 + 7);
                    if (off < min_stack_off) min_stack_off = off;
                }
                scan = scan->next;
            }
            next_offset_from_rbp = min_stack_off - 8;
        }

        /* ── Pass 0: Run linear scan register allocator ── */
        RegAllocator *ra = ra_create();
        ra_run(ra, fn);

        /* ── Pass 1: Allocate stack slots for ALL vars/temps ── */
        /* We still allocate stack slots for ALL temps: allocated temps
         * use their physical register during computation but the slot
         * remains as a spill target if needed by loads/stores.         */
        n = fn->head;
        while (n) {
            if (n->flags & IRF_DEAD) {
                n = n->next;
                continue;
            }
            if (n->dst[0]  != '\0' && n->dst[0]  != '-') get_or_create_var(n->dst);
            if (n->src1[0] != '\0' && n->src1[0] != '-') get_or_create_var(n->src1);
            if (n->src2[0] != '\0' && n->src2[0] != '-') get_or_create_var(n->src2);
            if (n->op == IR_ALLOCA) {
                next_offset_from_rbp -= n->imm;
            }
            n = n->next;
        }
        
        /* ── Task 1: Unified stack frame sizing (matches AST codegen_func) ── */
        /* raw slot bytes consumed so far (next_offset_from_rbp started at -8) */
        int stack_raw = -next_offset_from_rbp - 8; /* bytes for locals/temps */
        stack_size = stack_raw + 40 + 16; /* +40: 5 push slots, +16: ABI scratch CG-IR-019 */
        /* NOTE: variadic +176 is NOT applied here — ir_func_t has no is_variadic
         * field and the IR lowering path never handles variadic functions directly.
         * Variadic call-site setup is handled at the call instruction level. */
        if (stack_size < 256) stack_size = 256; /* minimum frame size */
        stack_size = (stack_size + 15) & ~15;   /* 16-byte align */

        /* ── Prologue ── */
        fprintf(out, "    .globl %s\n", fn->name);
        fprintf(out, "%s:\n", fn->name);
        fprintf(out, "    pushq %%rbp\n");
        fprintf(out, "    movq %%rsp, %%rbp\n");
        fprintf(out, "    subq $%d, %%rsp\n", stack_size);

        /* ── Task 2: Callee-saves via movq at %rbp-relative offsets ──
         * Mirrors: movq %r12, -(stack_raw + 8*(i+1))(%rbp)
         * (matching get_callee_reg() + codegen_func() in part4.c) */
        for (int ci = 0; ci < 5; ci++) {
            if (ra->used[callee_flag[ci]]) {
                int save_off = -(stack_raw + 8 * (ci + 1));
                fprintf(out, "    movq %s, %d(%%rbp)\n", callee_regs[ci], save_off);
            }
        }
        
        /* Store incoming parameters into their stack slots */
        for (int pnum = 0; pnum < fn->num_params; pnum++) {
            char param_name[32];
            sprintf(param_name, "%%stack_%d", -8 * (pnum + 1));
            int stack_off = get_or_create_var(param_name);
            if (pnum < 6) {
                fprintf(out, "    movq %s, %d(%%rbp)\n", arg_regs[pnum], stack_off);
            }
        }

        arg_idx = 0;
        int has_ret = 0; /* tracks whether an explicit IR_RET was emitted */

        /* ── Pass 2: Emit assembly ── */
        n = fn->head;
        while (n) {
            /* CG-IR-OPT-001: skip dead nodes */
            if (n->flags & IRF_DEAD) { n = n->next; continue; }
            fprintf(out, "    # %s\n", ir_op_name(n->op));
            switch (n->op) {
                case IR_CONST: {
                    /* Task 3: use movq (not movabsq) to match AST codegen */
                    fprintf(out, "    movq $%ld, %%rax\n", n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_CONST_STR: {
                    fprintf(out, "    leaq %s(%%rip), %%rax\n", n->src1);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_ADD:
                case IR_SUB:
                case IR_MUL:
                case IR_AND:
                case IR_OR:
                case IR_XOR:
                case IR_SHL:
                case IR_SHR:
                case IR_EQ:
                case IR_NE:
                case IR_LT:
                case IR_LE:
                case IR_GT:
                case IR_GE: {
                    load_operand(out, n->src1, "%rax", ra);

                    if (n->op == IR_ADD)        emit_src2(out, "addq",  n->src2, ra);
                    else if (n->op == IR_SUB)   emit_src2(out, "subq",  n->src2, ra);
                    else if (n->op == IR_MUL)   emit_src2(out, "imulq", n->src2, ra);
                    else if (n->op == IR_AND)   emit_src2(out, "andq",  n->src2, ra);
                    else if (n->op == IR_OR)    emit_src2(out, "orq",   n->src2, ra);
                    else if (n->op == IR_XOR)   emit_src2(out, "xorq",  n->src2, ra);
                    else if (n->op == IR_SHL) {
                        /* shift count must be in %cl */
                        load_operand(out, n->src2, "%rcx", ra);
                        if (n->type == IR_TY_I32 || n->type == IR_TY_U32) fprintf(out, "    shll %%cl, %%eax\n");
                        else if (n->type == IR_TY_I16 || n->type == IR_TY_U16) fprintf(out, "    shlw %%cl, %%ax\n");
                        else if (n->type == IR_TY_I8 || n->type == IR_TY_U8) fprintf(out, "    shlb %%cl, %%al\n");
                        else fprintf(out, "    shlq %%cl, %%rax\n");
                    } else if (n->op == IR_SHR) {
                        load_operand(out, n->src2, "%rcx", ra);
                        if (ir_type_unsigned(n->type)) {
                            if (n->type == IR_TY_U32) fprintf(out, "    shrl %%cl, %%eax\n");
                            else if (n->type == IR_TY_U16) fprintf(out, "    shrw %%cl, %%ax\n");
                            else if (n->type == IR_TY_U8) fprintf(out, "    shrb %%cl, %%al\n");
                            else fprintf(out, "    shrq %%cl, %%rax\n");
                        } else {
                            if (n->type == IR_TY_I32) fprintf(out, "    sarl %%cl, %%eax\n");
                            else if (n->type == IR_TY_I16) fprintf(out, "    sarw %%cl, %%ax\n");
                            else if (n->type == IR_TY_I8) fprintf(out, "    sarb %%cl, %%al\n");
                            else fprintf(out, "    sarq %%cl, %%rax\n");
                        }
                    } else {
                        /* comparison: src2 as memory operand or reg */
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE)
                                fprintf(out, "    cmpq %%%s, %%rax\n", preg_name(pr2));
                            else {
                                int off2 = get_or_create_var(n->src2);
                                fprintf(out, "    cmpq %d(%%rbp), %%rax\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            fprintf(out, "    cmpq %d(%%rbp), %%rax\n", off2);
                        }
                        int is_uns = is_unsigned_comparison(fn, n);
                        if      (n->op == IR_EQ) fprintf(out, "    sete %%al\n");
                        else if (n->op == IR_NE) fprintf(out, "    setne %%al\n");
                        else if (n->op == IR_LT) fprintf(out, is_uns ? "    setb %%al\n" : "    setl %%al\n");
                        else if (n->op == IR_LE) fprintf(out, is_uns ? "    setbe %%al\n" : "    setle %%al\n");
                        else if (n->op == IR_GT) fprintf(out, is_uns ? "    seta %%al\n" : "    setg %%al\n");
                        else if (n->op == IR_GE) fprintf(out, is_uns ? "    setae %%al\n" : "    setge %%al\n");
                        fprintf(out, "    movzbq %%al, %%rax\n");
                    }
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_DIV:
                case IR_MOD: {
                    load_operand(out, n->src1, "%rax", ra);
                    /* divisor: must be in a register or memory for idivq/divq */
                    int is_32 = (n->type == IR_TY_I32 || n->type == IR_TY_U32);
                    if (ir_type_unsigned(n->type)) {
                        if (is_32) fprintf(out, "    xorl %%edx, %%edx\n");
                        else       fprintf(out, "    xorq %%rdx, %%rdx\n");
                        
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE) {
                                if (is_32) fprintf(out, "    divl %%%s\n", preg_name32(pr2));
                                else       fprintf(out, "    divq %%%s\n", preg_name(pr2));
                            } else {
                                int off2 = get_or_create_var(n->src2);
                                if (is_32) fprintf(out, "    divl %d(%%rbp)\n", off2);
                                else       fprintf(out, "    divq %d(%%rbp)\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            if (is_32) fprintf(out, "    divl %d(%%rbp)\n", off2);
                            else       fprintf(out, "    divq %d(%%rbp)\n", off2);
                        }
                    } else {
                        if (is_32) fprintf(out, "    cltd\n");
                        else       fprintf(out, "    cqo\n");
                        
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE) {
                                if (is_32) fprintf(out, "    idivl %%%s\n", preg_name32(pr2));
                                else       fprintf(out, "    idivq %%%s\n", preg_name(pr2));
                            } else {
                                int off2 = get_or_create_var(n->src2);
                                if (is_32) fprintf(out, "    idivl %d(%%rbp)\n", off2);
                                else       fprintf(out, "    idivq %d(%%rbp)\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            if (is_32) fprintf(out, "    idivl %d(%%rbp)\n", off2);
                            else       fprintf(out, "    idivq %d(%%rbp)\n", off2);
                        }
                    }
                    if (n->op == IR_MOD) store_result(out, n->dst, "%rdx", ra);
                    else                 store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_NOT:
                case IR_NEG: {
                    load_operand(out, n->src1, "%rax", ra);
                    if (n->op == IR_NOT) fprintf(out, "    notq %%rax\n");
                    else                 fprintf(out, "    negq %%rax\n");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_LOAD: {
                    load_address_ra(out, n->src1, "%rax", ra);
                    if (n->type == IR_TY_I32 || n->type == IR_TY_U32) {
                        fprintf(out, "    movl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movslq %%eax, %%rax\n");
                    } else if (n->type == IR_TY_I8 || n->type == IR_TY_U8) {
                        fprintf(out, "    movzbl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movsbq %%al, %%rax\n");
                    } else if (n->type == IR_TY_I16 || n->type == IR_TY_U16) {
                        fprintf(out, "    movzwl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movswq %%ax, %%rax\n");
                    } else {
                        fprintf(out, "    movq (%%rax), %%rax\n");
                    }
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_VLOAD: {
                    load_address_ra(out, n->src1, "%rax", ra);
                    fprintf(out, "    movq (%%rax), %%rax\n");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_VEXTRACT: {
                    load_operand(out, n->src1, "%rax", ra);
                    fprintf(out, "    shrq $32, %%rax\n");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_ADDR: {
                    load_address_ra(out, n->src1, "%rax", ra);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_STORE: {
                    /* src1 = value, dst = address */
                    load_operand(out, n->src1, "%rcx", ra);
                    load_address_ra(out, n->dst, "%rax", ra);
                    if (n->type == IR_TY_I32 || n->type == IR_TY_U32)
                        fprintf(out, "    movl %%ecx, (%%rax)\n");
                    else if (n->type == IR_TY_I16 || n->type == IR_TY_U16)
                        fprintf(out, "    movw %%cx, (%%rax)\n");
                    else if (n->type == IR_TY_I8 || n->type == IR_TY_U8)
                        fprintf(out, "    movb %%cl, (%%rax)\n");
                    else
                        fprintf(out, "    movq %%rcx, (%%rax)\n");
                    break;
                }
                case IR_ALLOCA: {
                    int offd = get_or_create_var(n->dst);
                    fprintf(out, "    leaq %d(%%rbp), %%rax\n", offd - (int)n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_COPY:
                case IR_CAST: {
                    load_operand(out, n->src1, "%rax", ra);
                    if (n->op == IR_CAST) {
                        if (n->type == IR_TY_I8) fprintf(out, "    movsbl %%al, %%eax\n    movsbq %%al, %%rax\n");
                        else if (n->type == IR_TY_U8) fprintf(out, "    movzbl %%al, %%eax\n");
                        else if (n->type == IR_TY_I16) fprintf(out, "    movswl %%ax, %%eax\n    movswq %%ax, %%rax\n");
                        else if (n->type == IR_TY_U16) fprintf(out, "    movzwl %%ax, %%eax\n");
                        else if (n->type == IR_TY_I32) fprintf(out, "    movslq %%eax, %%rax\n");
                        else if (n->type == IR_TY_U32) fprintf(out, "    movl %%eax, %%eax\n");
                    }
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_LABEL: {
                    fprintf(out, "%s:\n", n->label);
                    break;
                }
                case IR_BR: {
                    fprintf(out, "    jmp %s\n", n->label);
                    break;
                }
                case IR_BR_IF: {
                    load_operand(out, n->src1, "%rax", ra);
                    fprintf(out, "    cmpq $0, %%rax\n");
                    fprintf(out, "    je %s\n", n->label);
                    break;
                }
                case IR_ARG: {
                    if (arg_idx == 0) {
                        int total_args = 0;
                        ir_node_t *scan = n;
                        while (scan && scan->op == IR_ARG) {
                            total_args++;
                            scan = scan->next;
                        }
                        if (total_args > 6 && (total_args - 6) % 2 != 0) {
                            call_padding = 8;
                        } else {
                            call_padding = 0;
                        }
                        // [FIX]: Pre-allocate exact stack frame for all args > 6
                        int alloc_size = (total_args > 6 ? (total_args - 6) * 8 : 0) + call_padding;
                        if (alloc_size > 0) {
                            fprintf(out, "    subq $%d, %%rsp\n", alloc_size);
                        }
                    }
                    
                    load_operand(out, n->src1, "%rax", ra);
                    
                    if (arg_idx < 6) {
                        fprintf(out, "    movq %%rax, %s\n", arg_regs[arg_idx]);
                    } else {
                        // [FIX]: Map stack args to precise offsets (0, 8, 16...)
                        int offset = (arg_idx - 6) * 8;
                        fprintf(out, "    movq %%rax, %d(%%rsp)\n", offset);
                    }
                    arg_idx++;
                    break;
                }
                case IR_CALL: {
                    fprintf(out, "    movb $0, %%al\n");
                    if (n->label[0] == '%') {
                        load_operand(out, n->label, "%r10", ra);
                        fprintf(out, "    call *%%r10\n"); /* Task 3: call not callq */
                    } else {
                        fprintf(out, "    call %s\n", n->label); /* Task 3: call not callq */
                    }
                    if (arg_idx > 6 || call_padding > 0) {
                        int cleanup_size = (arg_idx > 6 ? (arg_idx - 6) * 8 : 0) + call_padding;
                        fprintf(out, "    addq $%d, %%rsp\n", cleanup_size);
                    }
                    if (n->dst[0] != '\0' && n->dst[0] != '-') {
                        store_result(out, n->dst, "%rax", ra);
                    }
                    arg_idx = 0;
                    call_padding = 0;
                    break;
                }
                case IR_RET: {
                    /* CG-IR-OPT-001: IRF_RET_IMM — direct constant return, bypass regalloc */
                    if (n->flags & IRF_RET_IMM) {
                        fprintf(out, "    movq $%ld, %%rax\n", n->imm);
                    } else if (n->src1[0] != '\0' && n->src1[0] != '-') {
                        load_operand(out, n->src1, "%rax", ra);
                        if (n->type == IR_TY_I8) fprintf(out, "    movsbl %%al, %%eax\n    movsbq %%al, %%rax\n");
                        else if (n->type == IR_TY_U8) fprintf(out, "    movzbl %%al, %%eax\n");
                        else if (n->type == IR_TY_I16) fprintf(out, "    movswl %%ax, %%eax\n    movswq %%ax, %%rax\n");
                        else if (n->type == IR_TY_U16) fprintf(out, "    movzwl %%ax, %%eax\n");
                    }
                    fprintf(out, "    jmp %s\n", fn->end_label);
                    has_ret = 1;
                    break;
                }
                case IR_FCONST: {
                    fprintf(out, "    movabsq $%ld, %%rax\n", n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_FADD:
                case IR_FSUB:
                case IR_FMUL:
                case IR_FDIV: {
                    load_operand_xmm(out, n->src1, "%xmm0", ra);
                    if (ra) {
                        PhysReg pr2 = ra_get(ra, n->src2);
                        if (pr2 != PREG_NONE && pr2 >= PREG_XMM0) {
                            if      (n->op == IR_FADD) fprintf(out, "    addsd %%%s, %%xmm0\n", preg_name(pr2));
                            else if (n->op == IR_FSUB) fprintf(out, "    subsd %%%s, %%xmm0\n", preg_name(pr2));
                            else if (n->op == IR_FMUL) fprintf(out, "    mulsd %%%s, %%xmm0\n", preg_name(pr2));
                            else                       fprintf(out, "    divsd %%%s, %%xmm0\n", preg_name(pr2));
                            store_result_xmm(out, n->dst, "%xmm0", ra);
                            break;
                        }
                    }
                    /* memory operand fallback */
                    int off2 = get_or_create_var(n->src2);
                    if      (n->op == IR_FADD) fprintf(out, "    addsd %d(%%rbp), %%xmm0\n", off2);
                    else if (n->op == IR_FSUB) fprintf(out, "    subsd %d(%%rbp), %%xmm0\n", off2);
                    else if (n->op == IR_FMUL) fprintf(out, "    mulsd %d(%%rbp), %%xmm0\n", off2);
                    else                       fprintf(out, "    divsd %d(%%rbp), %%xmm0\n", off2);
                    store_result_xmm(out, n->dst, "%xmm0", ra);
                    break;
                }
                case IR_ITOF: {
                    load_operand(out, n->src1, "%rax", ra);
                    fprintf(out, "    cvtsi2sdq %%rax, %%xmm0\n");
                    store_result_xmm(out, n->dst, "%xmm0", ra);
                    break;
                }
                case IR_FTOI: {
                    load_operand_xmm(out, n->src1, "%xmm0", ra);
                    fprintf(out, "    cvttsd2si %%xmm0, %%rax\n");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                default: break;
            }
            n = n->next;
        }
        
        /* Convergence epilogue — all IR_RET paths jmp here */
        fprintf(out, "%s:\n", fn->end_label);
        for (int ci = 4; ci >= 0; ci--) {
            if (ra->used[callee_flag[ci]]) {
                int save_off = -(stack_raw + 8 * (ci + 1));
                fprintf(out, "    movq %d(%%rbp), %s\n", save_off, callee_regs[ci]);
            }
        }
        fprintf(out, "    movq %%rbp, %%rsp\n");
        fprintf(out, "    popq %%rbp\n");
        fprintf(out, "    ret\n");


        ra_free(ra);
    }
}
