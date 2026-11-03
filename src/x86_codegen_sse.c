/*
 * x86_codegen_sse.c — SSE2 floating-point code generation
 * =======================================================
 * Emits double-precision scalar instructions (addsd, subsd, mulsd, divsd)
 * and type conversion instructions.
 */

#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    char val_line[48];
    char label[32];
} FlitPoolEntry;

static int is_loc_hoistable_prev(const char *m) {
    if (!m || strncmp(m, "    ", 4) != 0) return 0;
    if (m[4] == '.' || strncmp(m, "    call", 8) == 0 || strncmp(m, "    j", 5) == 0 ||
        strncmp(m, "    ret", 7) == 0 || strncmp(m, "    leave", 9) == 0 ||
        strncmp(m, "    rep", 7) == 0 || strncmp(m, "    set", 7) == 0) {
        return 0;
    }
    return 1;
}

static int is_rax_rsp_disjoint_rc_insn(const char *m) {
    if (!m || m[0] == 0) return 0;
    if (strstr(m, "%rax") || strstr(m, "%eax") || strstr(m, "%ax") ||
        strstr(m, "%al") || strstr(m, "%ah") || strstr(m, "%rsp")) {
        return 0;
    }
    if ((strncmp(m, "    movq $", 10) == 0 && strstr(m, ", %r11\n")) ||
        (strncmp(m, "    movq ", 9) == 0 && strstr(m, "(%rbp), %r11\n")) ||
        (strncmp(m, "    movslq ", 11) == 0 && strstr(m, "(%rbp), %r11\n")) ||
        (strncmp(m, "    leaq ", 9) == 0 && strstr(m, "(%rbp), %r11\n")) ||
        strcmp(m, "    xorq %r11, %r11\n") == 0 ||
        (strncmp(m, "    shlq $", 10) == 0 && strstr(m, ", %r11\n")) ||
        (strncmp(m, "    imulq $", 11) == 0 && strstr(m, ", %r11\n")) ||
        strcmp(m, "    addq %r11, %r11\n") == 0 ||
        (strncmp(m, "    movss ", 10) == 0 && strstr(m, "(%rbp), %xmm1\n")) ||
        (strncmp(m, "    movsd ", 10) == 0 && strstr(m, "(%rbp), %xmm1\n"))) {
        return 1;
    }
    return 0;
}

static int compact_peep_lines_ex(char **line_ptrs, int nlines, int hoist_loc) {
    int active = 0;
    for (int i = 0; i < nlines; i++) {
        if (line_ptrs[i][0] != 0) {
            int pos = active;
            line_ptrs[active++] = line_ptrs[i];
            if (hoist_loc && strncmp(line_ptrs[pos], "    .loc ", 9) == 0) {
                int steps = 0;
                while (pos > 0 && steps < 6 && is_loc_hoistable_prev(line_ptrs[pos - 1])) {
                    char *tmp = line_ptrs[pos - 1];
                    line_ptrs[pos - 1] = line_ptrs[pos];
                    line_ptrs[pos] = tmp;
                    pos--;
                    steps++;
                }
            }
        }
    }
    return active;
}

int zcc_x86_peephole_boost(char **line_ptrs, int *nlines_ptr) {
    int nlines = *nlines_ptr;
    int eliminated = 0;
    const char *zopt = getenv("ZCC_OPT");
    if (!zopt || zopt[0] != '1' || nlines < 100) return 0;
    int glvl = (zopt[1] >= '1' && zopt[1] <= '8') ? (zopt[1] - '0') : 8;
    unsigned int pmask = (1U << (glvl + 1)) - 1U;
    int hoist_loc = (zopt[1] == '9') ? 1 : 0;
    int max_bpass = (glvl >= 8) ? 5 : 4;
    FlitPoolEntry *flit_pool = (FlitPoolEntry *)calloc(256, sizeof(FlitPoolEntry));
    if (!flit_pool) return 0;
    int pool_count = 0;

    nlines = compact_peep_lines_ex(line_ptrs, nlines, hoist_loc);

    /* Pass 0a: Normalize 32-bit movabsq $imm, %rax -> movq $imm, %rax (and $0 -> xorl) */
    if (pmask & 0x001U) {
        for (int i = 0; i < nlines; i++) {
            if (strncmp(line_ptrs[i], "    movabsq $", 13) == 0 && strcmp(line_ptrs[i] + strlen(line_ptrs[i]) - 7, ", %rax\n") == 0) {
                char *endp = NULL;
                const char *p = line_ptrs[i] + 13;
                long long imm = (*p == '-') ? strtoll(p, &endp, 0) : (long long)strtoull(p, &endp, 0);
                if (endp && endp > p && strcmp(endp, ", %rax\n") == 0 &&
                    imm >= -2147483648LL && imm <= 2147483647LL) {
                    if (imm == 0) {
                        sprintf(line_ptrs[i], "    xorl %%eax, %%eax\n");
                    } else {
                        sprintf(line_ptrs[i], "    movq $%lld, %%rax\n", imm);
                    }
                }
            }
        }
    }

    /* Pass 0b: Float/Double .rodata Zero-Idiom (pxor) & Constant Pool Deduplication */
    for (int i = 0; (pmask & 0x001U) && i + 5 < nlines; i++) {
        if (strcmp(line_ptrs[i], "    .section .rodata\n") == 0 &&
            (strcmp(line_ptrs[i + 1], "    .p2align 2\n") == 0 || strcmp(line_ptrs[i + 1], "    .p2align 3\n") == 0) &&
            strncmp(line_ptrs[i + 2], ".LS_flit_", 9) == 0 &&
            (strncmp(line_ptrs[i + 3], "    .long ", 10) == 0 || strncmp(line_ptrs[i + 3], "    .quad ", 10) == 0) &&
            strcmp(line_ptrs[i + 4], "    .text\n") == 0 &&
            (strncmp(line_ptrs[i + 5], "    movss .LS_flit_", 19) == 0 || strncmp(line_ptrs[i + 5], "    movsd .LS_flit_", 19) == 0) &&
            strstr(line_ptrs[i + 5], "(%rip), %xmm0\n") != NULL) {

            if (strcmp(line_ptrs[i + 3], "    .long 0\n") == 0 || strcmp(line_ptrs[i + 3], "    .quad 0\n") == 0) {
                sprintf(line_ptrs[i], "    pxor %%xmm0, %%xmm0\n");
                line_ptrs[i + 1][0] = 0;
                line_ptrs[i + 2][0] = 0;
                line_ptrs[i + 3][0] = 0;
                line_ptrs[i + 4][0] = 0;
                line_ptrs[i + 5][0] = 0;
                eliminated += 5;
                i += 5;
                continue;
            }

            int found_idx = -1;
            for (int p = 0; p < pool_count; p++) {
                if (strcmp(flit_pool[p].val_line, line_ptrs[i + 3]) == 0) {
                    found_idx = p;
                    break;
                }
            }
            if (found_idx >= 0) {
                const char *mop = (strncmp(line_ptrs[i + 5], "    movss", 9) == 0) ? "movss" : "movsd";
                sprintf(line_ptrs[i], "    %s %s(%%rip), %%xmm0\n", mop, flit_pool[found_idx].label);
                line_ptrs[i + 1][0] = 0;
                line_ptrs[i + 2][0] = 0;
                line_ptrs[i + 3][0] = 0;
                line_ptrs[i + 4][0] = 0;
                line_ptrs[i + 5][0] = 0;
                eliminated += 5;
                i += 5;
                continue;
            } else if (pool_count < 256 && strlen(line_ptrs[i + 3]) < 47) {
                char lbl_buf[32];
                if (sscanf(line_ptrs[i + 2], "%31[^:]:", lbl_buf) == 1) {
                    strcpy(flit_pool[pool_count].val_line, line_ptrs[i + 3]);
                    strcpy(flit_pool[pool_count].label, lbl_buf);
                    pool_count++;
                }
            }
        }
    }

    /* Multi-pass fixed-point peephole optimization */
    for (int bpass = 0; bpass < max_bpass; bpass++) {
        nlines = compact_peep_lines_ex(line_ptrs, nlines, hoist_loc);
        for (int i = 0; i < nlines;) {
            char *l1 = line_ptrs[i];
            if (l1[0] == 0) {
                i++;
                continue;
            }

            /* 1a. Local int postfix increment/decrement (10 lines -> 3 lines) */
            if ((pmask & 0x001U) && strncmp(l1, "    leaq ", 9) == 0 && strstr(l1, "(%rbp), %rax\n") && i + 9 < nlines &&
                strcmp(line_ptrs[i + 1], "    pushq %rax\n") == 0 &&
                strcmp(line_ptrs[i + 2], "    movslq (%rax), %rax\n") == 0 &&
                strcmp(line_ptrs[i + 3], "    pushq %rax\n") == 0 &&
                (strcmp(line_ptrs[i + 4], "    addq $1, %rax\n") == 0 || strcmp(line_ptrs[i + 4], "    subq $1, %rax\n") == 0) &&
                strcmp(line_ptrs[i + 5], "    movq %rax, %r11\n") == 0 &&
                strcmp(line_ptrs[i + 6], "    popq %rdx\n") == 0 &&
                strcmp(line_ptrs[i + 7], "    popq %rax\n") == 0 &&
                strcmp(line_ptrs[i + 8], "    movl %r11d, (%rax)\n") == 0 &&
                strcmp(line_ptrs[i + 9], "    movq %rdx, %rax\n") == 0) {
                char disp[64];
                if (sscanf(l1, "    leaq %63[^(](%%rbp), %%rax", disp) == 1) {
                    int step = (strncmp(line_ptrs[i + 4], "    addq", 8) == 0) ? 1 : -1;
                    sprintf(line_ptrs[i], "    movslq %s(%%rbp), %%rax\n", disp);
                    sprintf(line_ptrs[i + 1], "    leaq %d(%%rax), %%r11\n", step);
                    sprintf(line_ptrs[i + 2], "    movl %%r11d, %s(%%rbp)\n", disp);
                    for (int k = 3; k <= 9; k++) line_ptrs[i + k][0] = 0;
                    eliminated += 7;
                    i += 10;
                    continue;
                }
            }

            /* 1b. General pointer/field/global int postfix increment/decrement (9 lines -> 4 lines) */
            if ((pmask & 0x001U) && strcmp(l1, "    pushq %rax\n") == 0 && i + 8 < nlines &&
                strcmp(line_ptrs[i + 1], "    movslq (%rax), %rax\n") == 0 &&
                strcmp(line_ptrs[i + 2], "    pushq %rax\n") == 0 &&
                (strcmp(line_ptrs[i + 3], "    addq $1, %rax\n") == 0 || strcmp(line_ptrs[i + 3], "    subq $1, %rax\n") == 0) &&
                strcmp(line_ptrs[i + 4], "    movq %rax, %r11\n") == 0 &&
                strcmp(line_ptrs[i + 5], "    popq %rdx\n") == 0 &&
                strcmp(line_ptrs[i + 6], "    popq %rax\n") == 0 &&
                strcmp(line_ptrs[i + 7], "    movl %r11d, (%rax)\n") == 0 &&
                strcmp(line_ptrs[i + 8], "    movq %rdx, %rax\n") == 0) {
                int step = (strncmp(line_ptrs[i + 3], "    addq", 8) == 0) ? 1 : -1;
                sprintf(line_ptrs[i], "    movq %%rax, %%rdx\n");
                sprintf(line_ptrs[i + 1], "    movslq (%%rdx), %%rax\n");
                sprintf(line_ptrs[i + 2], "    leaq %d(%%rax), %%r11\n", step);
                sprintf(line_ptrs[i + 3], "    movl %%r11d, (%%rdx)\n");
                for (int k = 4; k <= 8; k++) line_ptrs[i + k][0] = 0;
                eliminated += 5;
                i += 9;
                continue;
            }

            /* 1c. Identity multiplication by 1 after movabsq -> movq normalization */
            if ((pmask & 0x001U) && strcmp(l1, "    pushq %rax\n") == 0 && i + 3 < nlines &&
                strcmp(line_ptrs[i + 1], "    movq $1, %rax\n") == 0) {
                if (strcmp(line_ptrs[i + 2], "    popq %r11\n") == 0 &&
                    strcmp(line_ptrs[i + 3], "    imulq %r11, %rax\n") == 0) {
                    sprintf(line_ptrs[i], "    movq %%rax, %%r11\n");
                    line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                    eliminated += 3; i += 4; continue;
                }
                if (i + 4 < nlines && strcmp(line_ptrs[i + 2], "    cltq\n") == 0 &&
                    strcmp(line_ptrs[i + 3], "    popq %r11\n") == 0 &&
                    strcmp(line_ptrs[i + 4], "    imulq %r11, %rax\n") == 0) {
                    sprintf(line_ptrs[i], "    movq %%rax, %%r11\n");
                    line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0; line_ptrs[i + 4][0] = 0;
                    eliminated += 4; i += 5; continue;
                }
            }

            /* 2. Push/Load/Pop Quads & Triads */
            if ((pmask & 0x002U) && strcmp(l1, "    pushq %rax\n") == 0) {
                if (i + 3 < nlines && strcmp(line_ptrs[i + 2], "    movq %rax, %r11\n") == 0 &&
                    strcmp(line_ptrs[i + 3], "    popq %rax\n") == 0) {
                    char *l2 = line_ptrs[i + 1];
                    if (strncmp(l2, "    movq $", 10) == 0) {
                        char imm[64];
                        if (sscanf(l2, "    movq %63[^,]", imm) == 1) {
                            sprintf(line_ptrs[i], "    movq %s, %%r11\n", imm);
                            line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                            eliminated += 3; i += 4; continue;
                        }
                    } else if (strcmp(l2, "    xorl %eax, %eax\n") == 0) {
                        sprintf(line_ptrs[i], "    xorq %%r11, %%r11\n");
                        line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                        eliminated += 3; i += 4; continue;
                    } else if (strstr(l2, "(%rbp), %rax\n") && (strncmp(l2, "    movslq ", 11) == 0 || strncmp(l2, "    movq ", 9) == 0)) {
                        char op[16], disp[64];
                        if (sscanf(l2, "    %15s %63[^,]", op, disp) == 2) {
                            sprintf(line_ptrs[i], "    %s %s, %%r11\n", op, disp);
                            line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                            eliminated += 3; i += 4; continue;
                        }
                    } else if (strstr(l2, "(%rbp), %eax\n") && strncmp(l2, "    movl ", 9) == 0) {
                        char disp[64];
                        if (sscanf(l2, "    movl %63[^,]", disp) == 1) {
                            sprintf(line_ptrs[i], "    movl %s, %%r11d\n", disp);
                            line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                            eliminated += 3; i += 4; continue;
                        }
                    }
                }
                if (i + 3 < nlines && strncmp(line_ptrs[i + 1], "    movq $", 10) == 0 &&
                    strcmp(line_ptrs[i + 2], "    movq %rax, %rcx\n") == 0 &&
                    strcmp(line_ptrs[i + 3], "    popq %rax\n") == 0) {
                    char imm[64];
                    if (sscanf(line_ptrs[i + 1], "    movq %63[^,]", imm) == 1) {
                        sprintf(line_ptrs[i], "    movq %s, %%rcx\n", imm);
                        line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0; line_ptrs[i + 3][0] = 0;
                        eliminated += 3; i += 4; continue;
                    }
                }
                if (i + 1 < nlines && strncmp(line_ptrs[i + 1], "    popq ", 9) == 0) {
                    char preg[32];
                    if (sscanf(line_ptrs[i + 1], "    popq %31s", preg) == 1 && preg[0] == '%') {
                        if (strcmp(preg, "%rax") == 0) {
                            line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0;
                            eliminated += 2; i += 2; continue;
                        } else {
                            sprintf(line_ptrs[i], "    movq %%rax, %s\n", preg);
                            line_ptrs[i + 1][0] = 0;
                            eliminated += 1; i += 2; continue;
                        }
                    }
                }
                if ((pmask & 0x100U) && i + 2 < nlines && strcmp(line_ptrs[i + 2], "    popq %rax\n") == 0 &&
                    is_rax_rsp_disjoint_rc_insn(line_ptrs[i + 1])) {
                    line_ptrs[i][0] = 0; line_ptrs[i + 2][0] = 0;
                    eliminated += 2; i += 1; continue;
                }
                if ((pmask & 0x100U) && i + 3 < nlines && strcmp(line_ptrs[i + 3], "    popq %rax\n") == 0 &&
                    is_rax_rsp_disjoint_rc_insn(line_ptrs[i + 1]) &&
                    is_rax_rsp_disjoint_rc_insn(line_ptrs[i + 2])) {
                    line_ptrs[i][0] = 0; line_ptrs[i + 3][0] = 0;
                    eliminated += 2; i += 1; continue;
                }
                if (i + 2 < nlines && strcmp(line_ptrs[i + 2], "    popq %r11\n") == 0) {
                    char *l2 = line_ptrs[i + 1];
                    if ((strstr(l2, "(%rbp), %rax\n") && (strncmp(l2, "    movq ", 9) == 0 || strncmp(l2, "    movslq ", 11) == 0 || strncmp(l2, "    leaq ", 9) == 0)) ||
                        ((pmask & 0x100U) && strstr(l2, "(%rip), %rax\n") && (strncmp(l2, "    movq ", 9) == 0 || strncmp(l2, "    movslq ", 11) == 0 || strncmp(l2, "    leaq ", 9) == 0)) ||
                        (strncmp(l2, "    movq $", 10) == 0 && strstr(l2, ", %rax\n")) ||
                        strcmp(l2, "    xorl %eax, %eax\n") == 0) {
                        sprintf(line_ptrs[i], "    movq %%rax, %%r11\n");
                        line_ptrs[i + 2][0] = 0;
                        eliminated += 1; i += 2; continue;
                    }
                }
                if ((pmask & 0x100U) && i + 3 < nlines && strcmp(line_ptrs[i + 3], "    popq %r11\n") == 0) {
                    char *l2 = line_ptrs[i + 1];
                    char *l3 = line_ptrs[i + 2];
                    if (((strstr(l2, "(%rbp), %rax\n") || strstr(l2, "(%rip), %rax\n")) &&
                         (strncmp(l2, "    movq ", 9) == 0 || strncmp(l2, "    movslq ", 11) == 0 || strncmp(l2, "    leaq ", 9) == 0)) &&
                        strncmp(l3, "    addq $", 10) == 0 && strstr(l3, ", %rax\n")) {
                        sprintf(line_ptrs[i], "    movq %%rax, %%r11\n");
                        line_ptrs[i + 3][0] = 0;
                        eliminated += 1; i += 2; continue;
                    }
                }
            }
            if ((pmask & 0x002U) && strcmp(l1, "    cltq\n") == 0 && i + 1 < nlines && strcmp(line_ptrs[i + 1], "    cltq\n") == 0) {
                line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
            }

            /* 3. Constant %r11 Index & Offset Algebraic Folding */
            if ((pmask & 0x002U) && strcmp(l1, "    xorq %r11, %r11\n") == 0 && i + 1 < nlines) {
                char *l2 = line_ptrs[i + 1];
                if ((strncmp(l2, "    shlq $", 10) == 0 || strncmp(l2, "    imulq $", 11) == 0) && strstr(l2, ", %r11\n")) {
                    line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                }
                if (strcmp(l2, "    addq %r11, %r11\n") == 0) {
                    line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                }
                if (strcmp(l2, "    addq %r11, %rax\n") == 0 || strcmp(l2, "    subq %r11, %rax\n") == 0) {
                    line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0; eliminated += 2; i += 2; continue;
                }
                if (strcmp(l2, "    movq %rax, %r11\n") == 0) {
                    line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;
                }
            }
            if ((pmask & 0x002U) && strncmp(l1, "    movq $", 10) == 0 && strstr(l1, ", %r11\n") && i + 1 < nlines) {
                long long c = 0, s = 0;
                char *l2 = line_ptrs[i + 1];
                if (sscanf(l1, "    movq $%lld, %%r11", &c) == 1) {
                    if (sscanf(l2, "    shlq $%lld, %%r11", &s) == 1 && strstr(l2, ", %r11\n") && s >= 0 && s < 62) {
                        long long nc = c << s;
                        if (nc >= -2147483647LL && nc <= 2147483647LL) {
                            sprintf(line_ptrs[i], "    movq $%lld, %%r11\n", nc);
                            line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                        }
                    } else if (strcmp(l2, "    addq %r11, %r11\n") == 0) {
                        long long nc = c * 2;
                        if (nc >= -2147483647LL && nc <= 2147483647LL) {
                            sprintf(line_ptrs[i], "    movq $%lld, %%r11\n", nc);
                            line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                        }
                    } else if (sscanf(l2, "    imulq $%lld, %%r11", &s) == 1 && strstr(l2, ", %r11\n")) {
                        long long nc = c * s;
                        if (nc >= -2147483647LL && nc <= 2147483647LL) {
                            sprintf(line_ptrs[i], "    movq $%lld, %%r11\n", nc);
                            line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                        }
                    } else if ((strcmp(l2, "    addq %r11, %rax\n") == 0 || strcmp(l2, "    subq %r11, %rax\n") == 0) &&
                               c >= -2147483647LL && c <= 2147483647LL) {
                        const char *aop = (strncmp(l2, "    addq", 8) == 0) ? "addq" : "subq";
                        sprintf(line_ptrs[i], "    %s $%lld, %%rax\n", aop, c);
                        line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                    }
                }
            }

            /* 4a. leaq disp(%rbp), %rax + addq $c, %rax -> leaq (disp+c)(%rbp), %rax */
            if ((pmask & 0x004U) && strncmp(l1, "    leaq ", 9) == 0 && strstr(l1, "(%rbp), %rax\n") && i + 1 < nlines) {
                long long d1 = 0, d2 = 0;
                if (sscanf(l1, "    leaq %lld(%%rbp), %%rax", &d1) == 1 &&
                    strncmp(line_ptrs[i + 1], "    addq $", 10) == 0 && strstr(line_ptrs[i + 1], ", %rax\n") &&
                    sscanf(line_ptrs[i + 1], "    addq $%lld, %%rax", &d2) == 1) {
                    sprintf(line_ptrs[i], "    leaq %lld(%%rbp), %%rax\n", d1 + d2);
                    line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                }
            }

            /* 4b. Base-Displacement Local Load & Store Folding */
            if ((pmask & 0x004U) && strncmp(l1, "    leaq ", 9) == 0 && strstr(l1, "(%rbp), %rax\n") && i + 1 < nlines) {
                char disp[64];
                if (sscanf(l1, "    leaq %63[^(](%%rbp), %%rax", disp) == 1) {
                    char *l2 = line_ptrs[i + 1];
                    const char *op = NULL;
                    const char *dest = "%rax";
                    if (strcmp(l2, "    movslq (%rax), %rax\n") == 0) { op = "movslq"; dest = "%rax"; }
                    else if (strcmp(l2, "    movq (%rax), %rax\n") == 0) { op = "movq"; dest = "%rax"; }
                    else if (strcmp(l2, "    movl (%rax), %eax\n") == 0) { op = "movl"; dest = "%eax"; }
                    else if (strcmp(l2, "    movzbl (%rax), %eax\n") == 0) { op = "movzbl"; dest = "%eax"; }
                    else if (strcmp(l2, "    movzwl (%rax), %eax\n") == 0) { op = "movzwl"; dest = "%eax"; }
                    else if (strcmp(l2, "    movsbq (%rax), %rax\n") == 0) { op = "movsbq"; dest = "%rax"; }
                    else if (strcmp(l2, "    movswq (%rax), %rax\n") == 0) { op = "movswq"; dest = "%rax"; }
                    else if (strcmp(l2, "    movss (%rax), %xmm0\n") == 0) { op = "movss"; dest = "%xmm0"; }
                    else if (strcmp(l2, "    movsd (%rax), %xmm0\n") == 0) { op = "movsd"; dest = "%xmm0"; }
                    if (op) {
                        sprintf(line_ptrs[i], "    %s %s(%%rbp), %s\n", op, disp, dest);
                        line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                    }
                    {
                        const char *sop = NULL, *src = NULL;
                        if (strcmp(l2, "    movl %r11d, (%rax)\n") == 0) { sop = "movl"; src = "%r11d"; }
                        else if (strcmp(l2, "    movq %r11, (%rax)\n") == 0) { sop = "movq"; src = "%r11"; }
                        else if (strcmp(l2, "    movb %r11b, (%rax)\n") == 0) { sop = "movb"; src = "%r11b"; }
                        else if (strcmp(l2, "    movw %r11w, (%rax)\n") == 0) { sop = "movw"; src = "%r11w"; }
                        if (sop) {
                            sprintf(line_ptrs[i], "    %s %s, %s(%%rbp)\n", sop, src, disp);
                            line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                        }
                    }
                }
            }

            /* 4c. Consecutive addq $c1, %rax + addq $c2, %rax & Struct-Member Offset Load/Store Folding */
            if ((pmask & 0x004U) && strncmp(l1, "    addq $", 10) == 0 && strstr(l1, ", %rax\n") && i + 1 < nlines) {
                long long d1 = 0, d2 = 0;
                char *l2 = line_ptrs[i + 1];
                if (sscanf(l1, "    addq $%lld, %%rax", &d1) == 1 &&
                    strncmp(l2, "    addq $", 10) == 0 && strstr(l2, ", %rax\n") &&
                    sscanf(l2, "    addq $%lld, %%rax", &d2) == 1) {
                    long long sum = d1 + d2;
                    if (sum == 0) {
                        line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0; eliminated += 2; i += 2; continue;
                    } else if (sum >= -2147483647LL && sum <= 2147483647LL) {
                        sprintf(line_ptrs[i], "    addq $%lld, %%rax\n", sum);
                        line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                    }
                }
                char imm[64], op[16], dst[16];
                if (sscanf(l1, "    addq $%63[^,], %%rax", imm) == 1) {
                    if (strstr(l2, " (%rax), %") &&
                        (strncmp(l2, "    movq (", 10) == 0 || strncmp(l2, "    movslq (", 12) == 0 ||
                         strncmp(l2, "    movl (", 10) == 0 || strncmp(l2, "    movzbl (", 12) == 0 ||
                         strncmp(l2, "    movzwl (", 12) == 0 || strncmp(l2, "    movsbq (", 12) == 0 ||
                         strncmp(l2, "    movswq (", 12) == 0 || strncmp(l2, "    movss (", 11) == 0 ||
                         strncmp(l2, "    movsd (", 11) == 0) &&
                        sscanf(l2, "    %15s (%%rax), %15s", op, dst) == 2) {
                        sprintf(line_ptrs[i], "    %s %s(%%rax), %s\n", op, imm, dst);
                        line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                    }
                    if (i + 2 < nlines && strcmp(l2, "    popq %r11\n") == 0) {
                        char *l3 = line_ptrs[i + 2];
                        const char *sop = NULL, *src = NULL;
                        if (strcmp(l3, "    movl %r11d, (%rax)\n") == 0) { sop = "movl"; src = "%r11d"; }
                        else if (strcmp(l3, "    movq %r11, (%rax)\n") == 0) { sop = "movq"; src = "%r11"; }
                        else if (strcmp(l3, "    movb %r11b, (%rax)\n") == 0) { sop = "movb"; src = "%r11b"; }
                        else if (strcmp(l3, "    movw %r11w, (%rax)\n") == 0) { sop = "movw"; src = "%r11w"; }
                        if (sop) {
                            sprintf(line_ptrs[i], "    popq %%r11\n");
                            sprintf(line_ptrs[i + 1], "    %s %s, %s(%%rax)\n", sop, src, imm);
                            line_ptrs[i + 2][0] = 0; eliminated += 1; i += 2; continue;
                        }
                    }
                }
            }

            /* 5a. Redundant GPR<->XMM Roundtrip & Direct XMM Store */
            if ((pmask & 0x008U) && (strcmp(l1, "    movd %xmm0, %eax\n") == 0 || strcmp(l1, "    movq %xmm0, %rax\n") == 0) && i + 1 < nlines) {
                if (strcmp(line_ptrs[i + 1], "    movd %eax, %xmm0\n") == 0 || strcmp(line_ptrs[i + 1], "    movq %rax, %xmm0\n") == 0) {
                    line_ptrs[i + 1][0] = 0; eliminated += 1;
                    if (i + 2 < nlines && (strcmp(line_ptrs[i + 2], "    subq $8, %rsp\n") == 0 ||
                                           strcmp(line_ptrs[i + 2], "    movss (%rsp), %xmm1\n") == 0 ||
                                           strcmp(line_ptrs[i + 2], "    movsd (%rsp), %xmm1\n") == 0)) {
                        line_ptrs[i][0] = 0; eliminated += 1;
                    }
                    i += 2; continue;
                }
            }
            if ((pmask & 0x008U) && (strcmp(l1, "    movd %xmm0, %eax\n") == 0 || strcmp(l1, "    movq %xmm0, %rax\n") == 0) &&
                i + 2 < nlines && strcmp(line_ptrs[i + 1], "    movq %rax, %r11\n") == 0) {
                char *l3 = line_ptrs[i + 2];
                int is_f32_store = (strncmp(l3, "    movl %r11d, ", 16) == 0);
                int is_f64_store = (strncmp(l3, "    movq %r11, ", 15) == 0 && strncmp(l1, "    movq", 8) == 0);
                if ((is_f32_store || is_f64_store) && (strstr(l3, "(%rbp)\n") || strstr(l3, "(%rax)\n"))) {
                    const char *xop = is_f64_store ? "movsd" : "movss";
                    int mlen = is_f64_store ? 15 : 16;
                    char mem[64];
                    if (sscanf(l3 + mlen, "%63s", mem) == 1) {
                        char orig_l1[32];
                        strcpy(orig_l1, l1);
                        sprintf(line_ptrs[i], "    %s %%xmm0, %s\n", xop, mem);
                        strcpy(line_ptrs[i + 1], orig_l1);
                        line_ptrs[i + 2][0] = 0; eliminated += 1;
                        if (i + 3 < nlines && strcmp(line_ptrs[i + 3], "    movq %r11, %rax\n") == 0) {
                            line_ptrs[i + 3][0] = 0; eliminated += 1;
                        }
                        i += 1; continue;
                    }
                } else if (i + 3 < nlines && strncmp(l3, "    movq ", 9) == 0 && strstr(l3, "(%rbp), %rax\n")) {
                    char *l4 = line_ptrs[i + 3];
                    int is_f32_ind = (strncmp(l4, "    movl %r11d, ", 16) == 0);
                    int is_f64_ind = (strncmp(l4, "    movq %r11, ", 15) == 0 && strncmp(l1, "    movq", 8) == 0);
                    if ((is_f32_ind || is_f64_ind) && strstr(l4, "(%rax)\n")) {
                        const char *xop = is_f64_ind ? "movsd" : "movss";
                        int mlen = is_f64_ind ? 15 : 16;
                        char load_ln[64], mem[64], orig_l1[32];
                        if (strlen(l3) < 63 && sscanf(l4 + mlen, "%63s", mem) == 1) {
                            strcpy(orig_l1, l1);
                            strcpy(load_ln, l3);
                            strcpy(line_ptrs[i], load_ln);
                            sprintf(line_ptrs[i + 1], "    %s %%xmm0, %s\n", xop, mem);
                            strcpy(line_ptrs[i + 2], orig_l1);
                            line_ptrs[i + 3][0] = 0; eliminated += 1;
                            if (i + 4 < nlines && strcmp(line_ptrs[i + 4], "    movq %r11, %rax\n") == 0) {
                                line_ptrs[i + 4][0] = 0; eliminated += 1;
                            }
                            i += 2; continue;
                        }
                    }
                }
            }

            /* 5b. Bounded-Window SSE Stack Spill Elision */
            if ((pmask & 0x008U) && strcmp(l1, "    subq $8, %rsp\n") == 0 && i + 3 < nlines &&
                (strcmp(line_ptrs[i + 1], "    movss %xmm0, (%rsp)\n") == 0 || strcmp(line_ptrs[i + 1], "    movsd %xmm0, (%rsp)\n") == 0)) {
                int matched_spill = 0;
                int limit = (i + 10 < nlines - 1) ? (i + 10) : (nlines - 2);
                for (int j = i + 2; j <= limit; j++) {
                    char *mj = line_ptrs[j];
                    if (mj[0] == 0) continue;
                    if ((strcmp(mj, "    movss (%rsp), %xmm1\n") == 0 || strcmp(mj, "    movsd (%rsp), %xmm1\n") == 0) &&
                        strcmp(line_ptrs[j + 1], "    addq $8, %rsp\n") == 0) {
                        if (j > i + 2) {
                            sprintf(line_ptrs[i], "    movaps %%xmm0, %%xmm1\n");
                            line_ptrs[i + 1][0] = 0;
                            line_ptrs[j][0] = 0;
                            line_ptrs[j + 1][0] = 0;
                            eliminated += 3;
                            matched_spill = 1;
                        }
                        break;
                    }
                    if (strstr(mj, "%rsp") || strstr(mj, "%xmm1") ||
                        strncmp(mj, "    call", 8) == 0 || strncmp(mj, "    j", 5) == 0 ||
                        strncmp(mj, "    push", 8) == 0 || strncmp(mj, "    pop", 7) == 0 ||
                        strncmp(mj, "    ret", 7) == 0 || mj[0] != ' ') {
                        break;
                    }
                }
                if (matched_spill) {
                    i += 2; continue;
                }
            }

            /* 5c. Dead GPR Mirror before SSE Operation, Direct Float Store & XMM Call Argument Stack Bypass */
            if ((pmask & 0x008U) && (strcmp(l1, "    movq %xmm0, %rax\n") == 0 || strcmp(l1, "    movd %xmm0, %eax\n") == 0) && i + 3 < nlines &&
                strcmp(line_ptrs[i + 1], "    movq %rax, %r11\n") == 0 &&
                strcmp(line_ptrs[i + 2], "    popq %rax\n") == 0 &&
                (strcmp(line_ptrs[i + 3], "    movl %r11d, (%rax)\n") == 0 || strcmp(line_ptrs[i + 3], "    movq %r11, (%rax)\n") == 0)) {
                const char *xst = (strcmp(line_ptrs[i + 3], "    movl %r11d, (%rax)\n") == 0) ? "movss" : "movsd";
                char orig_l1[32];
                strcpy(orig_l1, l1);
                sprintf(line_ptrs[i], "    popq %%rax\n");
                sprintf(line_ptrs[i + 1], "    %s %%xmm0, (%%rax)\n", xst);
                strcpy(line_ptrs[i + 2], orig_l1);
                line_ptrs[i + 3][0] = 0; eliminated += 1;
                if (i + 4 < nlines && strcmp(line_ptrs[i + 4], "    movq %r11, %rax\n") == 0) {
                    line_ptrs[i + 4][0] = 0; eliminated += 1;
                }
                i += 2; continue;
            }
            if ((pmask & 0x008U) && (strcmp(l1, "    movq %xmm0, %rax\n") == 0 || strcmp(l1, "    movd %xmm0, %eax\n") == 0) && i + 3 < nlines &&
                strcmp(line_ptrs[i + 1], "    pushq %rax\n") == 0) {
                int sdepth = 0, found_j = -1;
                char txmm[16] = {0};
                int limit = (i + 28 < nlines - 1) ? (i + 28) : (nlines - 2);
                for (int j = i + 2; j <= limit; j++) {
                    char *mj = line_ptrs[j];
                    if (mj[0] == 0 || strncmp(mj, "    .loc ", 9) == 0 || strncmp(mj, "    .file ", 10) == 0) continue;
                    if (sdepth == 0 && strcmp(mj, "    popq %rax\n") == 0 &&
                        (strncmp(line_ptrs[j + 1], "    movq %rax, %xmm", 19) == 0 || strncmp(line_ptrs[j + 1], "    movd %eax, %xmm", 19) == 0)) {
                        if (sscanf(line_ptrs[j + 1] + 15, "%15s", txmm) == 1) {
                            int touched = 0;
                            for (int k = i + 2; k < j; k++) {
                                if (line_ptrs[k][0] != 0 && strstr(line_ptrs[k], txmm)) { touched = 1; break; }
                            }
                            if (!touched) {
                                int nk = j + 2;
                                while (nk < nlines && (line_ptrs[nk][0] == 0 || strncmp(line_ptrs[nk], "    .loc ", 9) == 0 || strncmp(line_ptrs[nk], "    .file ", 10) == 0 || strncmp(line_ptrs[nk], "    movaps ", 11) == 0)) nk++;
                                if (nk < nlines && (strcmp(line_ptrs[nk], "    popq %rax\n") == 0 || strncmp(line_ptrs[nk], "    movl $", 10) == 0 || strcmp(line_ptrs[nk], "    xorl %eax, %eax\n") == 0)) {
                                    found_j = j;
                                }
                            }
                        }
                        break;
                    }
                    if (strncmp(mj, "    pushq ", 10) == 0 || strcmp(mj, "    subq $8, %rsp\n") == 0) sdepth++;
                    else if (strncmp(mj, "    popq ", 9) == 0 || strcmp(mj, "    addq $8, %rsp\n") == 0) { if (sdepth == 0) break; sdepth--; }
                    else if (strstr(mj, "%rsp") || strncmp(mj, "    call", 8) == 0 || strncmp(mj, "    j", 5) == 0 || strncmp(mj, "    ret", 7) == 0 || (mj[0] != ' ' && strncmp(mj, ".LS_flit_", 9) != 0 && strncmp(mj, ".LC", 3) != 0)) break;
                }
                if (found_j != -1) {
                    if (strcmp(txmm, "%xmm0") == 0) {
                        line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0;
                        line_ptrs[found_j][0] = 0; line_ptrs[found_j + 1][0] = 0;
                        eliminated += 4; i += 2; continue;
                    } else {
                        sprintf(line_ptrs[i], "    movaps %%xmm0, %s\n", txmm);
                        line_ptrs[i + 1][0] = 0;
                        line_ptrs[found_j][0] = 0; line_ptrs[found_j + 1][0] = 0;
                        eliminated += 3; i += 2; continue;
                    }
                }
            }
            if ((pmask & 0x008U) && (strcmp(l1, "    movq %xmm0, %rax\n") == 0 || strcmp(l1, "    movd %xmm0, %eax\n") == 0) && i + 1 < nlines) {
                char *l2 = line_ptrs[i + 1];
                if (strcmp(l2, "    movaps %xmm0, %xmm1\n") == 0 ||
                    strcmp(l2, "    movss (%rsp), %xmm1\n") == 0 ||
                    strcmp(l2, "    movsd (%rsp), %xmm1\n") == 0 ||
                    strncmp(l2, "    addss ", 10) == 0 || strncmp(l2, "    subss ", 10) == 0 ||
                    strncmp(l2, "    mulss ", 10) == 0 || strncmp(l2, "    divss ", 10) == 0 ||
                    strncmp(l2, "    addsd ", 10) == 0 || strncmp(l2, "    subsd ", 10) == 0 ||
                    strncmp(l2, "    mulsd ", 10) == 0 || strncmp(l2, "    divsd ", 10) == 0 ||
                    strncmp(l2, "    ucomiss ", 12) == 0 || strncmp(l2, "    ucomisd ", 12) == 0 ||
                    strncmp(l2, "    cvtss2sd ", 13) == 0 || strncmp(l2, "    cvtsd2ss ", 13) == 0 ||
                    (strcmp(l2, "    subq $8, %rsp\n") == 0 && i + 2 < nlines &&
                     (strcmp(line_ptrs[i + 2], "    movss %xmm0, (%rsp)\n") == 0 || strcmp(line_ptrs[i + 2], "    movsd %xmm0, (%rsp)\n") == 0))) {
                    int k = i + 2;
                    while (k < nlines && (line_ptrs[k][0] == 0 || strncmp(line_ptrs[k], "    .loc ", 9) == 0 || strncmp(line_ptrs[k], "    .file ", 10) == 0 ||
                           strncmp(line_ptrs[k], "    movaps ", 11) == 0 || strncmp(line_ptrs[k], "    movss ", 10) == 0 || strncmp(line_ptrs[k], "    movsd ", 10) == 0 ||
                           strncmp(line_ptrs[k], "    addss ", 10) == 0 || strncmp(line_ptrs[k], "    subss ", 10) == 0 || strncmp(line_ptrs[k], "    mulss ", 10) == 0 || strncmp(line_ptrs[k], "    divss ", 10) == 0 ||
                           strncmp(line_ptrs[k], "    addsd ", 10) == 0 || strncmp(line_ptrs[k], "    subsd ", 10) == 0 || strncmp(line_ptrs[k], "    mulsd ", 10) == 0 || strncmp(line_ptrs[k], "    divsd ", 10) == 0 ||
                           strncmp(line_ptrs[k], "    ucomiss ", 12) == 0 || strncmp(line_ptrs[k], "    ucomisd ", 12) == 0 || strncmp(line_ptrs[k], "    cvtss2sd ", 13) == 0 || strncmp(line_ptrs[k], "    cvtsd2ss ", 13) == 0 ||
                           strcmp(line_ptrs[k], "    pxor %xmm0, %xmm0\n") == 0 || strcmp(line_ptrs[k], "    subq $8, %rsp\n") == 0 || strcmp(line_ptrs[k], "    addq $8, %rsp\n") == 0)) k++;
                    if (k < nlines) {
                        char *nl = line_ptrs[k];
                        int nlen = (int)strlen(nl);
                        if (strcmp(nl, "    movq %xmm0, %rax\n") == 0 || strcmp(nl, "    movd %xmm0, %eax\n") == 0 ||
                            strncmp(nl, "    set", 7) == 0 || strncmp(nl, "    cvttss2si ", 14) == 0 || strncmp(nl, "    cvttsd2si ", 14) == 0 ||
                            strcmp(nl, "    xorl %eax, %eax\n") == 0 || strcmp(nl, "    xorq %rax, %rax\n") == 0 ||
                            strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0 ||
                            (nlen >= 7 && (strcmp(nl + nlen - 7, ", %rax\n") == 0 || strcmp(nl + nlen - 7, ", %eax\n") == 0) &&
                             !strstr(nl, "(%rax)") && !strstr(nl, "(%eax)") && (strncmp(nl, "    mov", 7) == 0 || strncmp(nl, "    lea", 7) == 0))) {
                            line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;
                        }
                    }
                }
            }

            /* 6. DPO-Evolved Reverse-Coupled Register Back-Propagation & Commutative Pop Fusion */
            if ((pmask & 0x080U) && (strncmp(l1, "    movss ", 10) == 0 || strncmp(l1, "    movsd ", 10) == 0) &&
                strstr(l1, "(%rbp), %xmm0\n") && i + 2 < nlines &&
                strcmp(line_ptrs[i + 1], "    movaps %xmm0, %xmm1\n") == 0) {
                int k = i + 2;
                while (k < nlines && (line_ptrs[k][0] == 0 || strncmp(line_ptrs[k], "    .loc ", 9) == 0 || strncmp(line_ptrs[k], "    .file ", 10) == 0)) k++;
                if (k < nlines) {
                    char *nl = line_ptrs[k];
                    if (((strncmp(nl, "    movss ", 10) == 0 || strncmp(nl, "    movsd ", 10) == 0) &&
                         (strstr(nl, "(%rbp), %xmm0\n") || strstr(nl, "(%rip), %xmm0\n"))) ||
                        strcmp(nl, "    pxor %xmm0, %xmm0\n") == 0) {
                        int l1_len = (int)strlen(l1);
                        if (l1_len > 14) {
                            l1[l1_len - 2] = '1';
                            line_ptrs[i + 1][0] = 0;
                            eliminated += 1; i += 2; continue;
                        }
                    }
                }
            }
            if ((pmask & 0x080U) && i + 1 < nlines && strcmp(line_ptrs[i + 1], "    movl %eax, %eax\n") == 0) {
                if (strcmp(l1, "    xorl %eax, %eax\n") == 0 || strcmp(l1, "    movzbl %al, %eax\n") == 0 ||
                    strncmp(l1, "    movl $", 10) == 0 ||
                    (strncmp(l1, "    movq $", 10) == 0 && l1[10] >= '0' && l1[10] <= '9' && atoll(l1 + 10) <= 2147483647LL)) {
                    line_ptrs[i + 1][0] = 0;
                    eliminated += 1;
                }
            }
            if (pmask & 0x100U) {
                int l1_len = (int)strlen(l1);
                if (l1_len > 12 && strcmp(l1 + l1_len - 7, ", %rax\n") == 0 &&
                    ((strncmp(l1, "    leaq ", 9) == 0 && (strstr(l1, "(%rbp), %rax\n") || strstr(l1, "(%rip), %rax\n"))) ||
                     (strncmp(l1, "    movq ", 9) == 0 && (strstr(l1, "(%rbp), %rax\n") || strstr(l1, "(%rip), %rax\n"))) ||
                     (strncmp(l1, "    movslq ", 11) == 0 && (strstr(l1, "(%rbp), %rax\n") || strstr(l1, "(%rip), %rax\n"))) ||
                     strncmp(l1, "    movq $", 10) == 0)) {
                    int k1 = i + 1;
                    while (k1 < nlines && (line_ptrs[k1][0] == 0 || strncmp(line_ptrs[k1], "    .loc ", 9) == 0 || strncmp(line_ptrs[k1], "    .file ", 10) == 0)) k1++;
                    if (k1 < nlines && strcmp(line_ptrs[k1], "    movq %rax, %r11\n") == 0) {
                        int k2 = k1 + 1;
                        while (k2 < nlines && (line_ptrs[k2][0] == 0 || strncmp(line_ptrs[k2], "    .loc ", 9) == 0 || strncmp(line_ptrs[k2], "    .file ", 10) == 0)) k2++;
                        if (k2 < nlines) {
                            char *nl = line_ptrs[k2];
                            if ((strncmp(nl, "    leaq ", 9) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                (strncmp(nl, "    movq ", 9) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                (strncmp(nl, "    movslq ", 11) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                strncmp(nl, "    movq $", 10) == 0 || strcmp(nl, "    xorl %eax, %eax\n") == 0 ||
                                strcmp(nl, "    popq %rax\n") == 0) {
                                memcpy(l1 + l1_len - 7, ", %r11\n", 7);
                                line_ptrs[k1][0] = 0;
                                eliminated += 1; i = k2; continue;
                            }
                        }
                    } else if (k1 < nlines && strcmp(line_ptrs[k1], "    pushq %rax\n") == 0) {
                        int k2 = k1 + 1;
                        while (k2 < nlines && (line_ptrs[k2][0] == 0 || strncmp(line_ptrs[k2], "    .loc ", 9) == 0 || strncmp(line_ptrs[k2], "    .file ", 10) == 0)) k2++;
                        if (k2 < nlines) {
                            char *nl = line_ptrs[k2];
                            if ((strncmp(nl, "    leaq ", 9) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                (strncmp(nl, "    movq ", 9) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                (strncmp(nl, "    movslq ", 11) == 0 && (strstr(nl, "(%rbp), %rax\n") || strstr(nl, "(%rip), %rax\n"))) ||
                                strncmp(nl, "    movq $", 10) == 0 || strcmp(nl, "    xorl %eax, %eax\n") == 0) {
                                int k3 = k2 + 1;
                                while (k3 < nlines && (line_ptrs[k3][0] == 0 || strncmp(line_ptrs[k3], "    .loc ", 9) == 0 || strncmp(line_ptrs[k3], "    .file ", 10) == 0)) k3++;
                                if (k3 < nlines && strncmp(line_ptrs[k3], "    addq $", 10) == 0 && strstr(line_ptrs[k3], ", %rax\n")) {
                                    k3++;
                                    while (k3 < nlines && (line_ptrs[k3][0] == 0 || strncmp(line_ptrs[k3], "    .loc ", 9) == 0 || strncmp(line_ptrs[k3], "    .file ", 10) == 0)) k3++;
                                }
                                if (k3 < nlines && strcmp(line_ptrs[k3], "    popq %r11\n") == 0) {
                                    memcpy(l1 + l1_len - 7, ", %r11\n", 7);
                                    line_ptrs[k1][0] = 0;
                                    line_ptrs[k3][0] = 0;
                                    eliminated += 2; i = k2; continue;
                                }
                            }
                        }
                    }
                }
            }
            if ((pmask & 0x010U) && bpass == (max_bpass - 1) && strcmp(l1, "    movq %rax, %r11\n") == 0 && i + 2 < nlines &&
                strcmp(line_ptrs[i + 1], "    popq %rax\n") == 0) {
                char *l3 = line_ptrs[i + 2];
                if (strcmp(l3, "    addq %r11, %rax\n") == 0 ||
                    strcmp(l3, "    imulq %r11, %rax\n") == 0 ||
                    strcmp(l3, "    andq %r11, %rax\n") == 0 ||
                    strcmp(l3, "    orq %r11, %rax\n") == 0 ||
                    strcmp(l3, "    xorq %r11, %rax\n") == 0) {
                    sprintf(line_ptrs[i], "    popq %%r11\n");
                    line_ptrs[i + 1][0] = 0;
                    eliminated += 1;
                    i += 1; continue;
                }
            }

            /* 7. Dead %rax/%eax Write Before Immediate Overwrite & Ping-Pong Moves */
            if ((pmask & 0x020U) && strcmp(l1, "    movq %rax, %r11\n") == 0 && i + 1 < nlines) {
                if (strcmp(line_ptrs[i + 1], "    movq %r11, %rax\n") == 0) {
                    line_ptrs[i + 1][0] = 0; eliminated += 1; i += 2; continue;
                }
                if (strcmp(line_ptrs[i + 1], "    movq %rax, %r11\n") == 0) {
                    line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;
                }
            }
            if ((pmask & 0x020U) && (strcmp(l1, "    movq %r11, %rax\n") == 0 || strcmp(l1, "    movd %xmm0, %eax\n") == 0 ||
                strcmp(l1, "    movq %xmm0, %rax\n") == 0 || strcmp(l1, "    movq %rdx, %rax\n") == 0)) {
                int k = i + 1;
                while (k < nlines && (line_ptrs[k][0] == 0 || strncmp(line_ptrs[k], "    .loc ", 9) == 0 || strncmp(line_ptrs[k], "    .file ", 10) == 0)) k++;
                if (k < nlines) {
                    char *nl = line_ptrs[k];
                    int nlen = (int)strlen(nl);
                    if (nlen >= 7 && (strcmp(nl + nlen - 7, ", %rax\n") == 0 || strcmp(nl + nlen - 7, ", %eax\n") == 0) &&
                        !strstr(nl, "(%rax)") && !strstr(nl, "(%eax)") &&
                        (((strncmp(nl, "    mov", 7) == 0 || strncmp(nl, "    lea", 7) == 0) && (strstr(nl, "(%rbp)") || strstr(nl, "(%rip)"))) ||
                         strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0 ||
                         strcmp(nl, "    xorl %eax, %eax\n") == 0 || strcmp(nl, "    xorq %rax, %rax\n") == 0)) {
                        line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;
                    }
                }
            }

            i++;
        }
    }

    /* 8. Intra-Function Static %rsp 16-Byte Call Alignment Elision */
    nlines = compact_peep_lines_ex(line_ptrs, nlines, hoist_loc);
    if ((pmask & 0x040U) && nlines >= 600) {
        long long rsp_depth = -1;
        int i = 0;
        while (i < nlines) {
            char *l = line_ptrs[i];
            if (strcmp(l, "    movq %rsp, %rbp\n") == 0 || strcmp(l, "    movq %rbp, %rsp\n") == 0 || strcmp(l, "    leave\n") == 0) {
                rsp_depth = 0;
                i++; continue;
            }
            if (strcmp(l, "    movq %rsp, %r11\n") == 0 && i + 10 < nlines &&
                strcmp(line_ptrs[i + 1], "    andq $15, %r11\n") == 0 &&
                strcmp(line_ptrs[i + 2], "    testq %r11, %r11\n") == 0 &&
                strncmp(line_ptrs[i + 3], "    je .Lcall_aligned_", 22) == 0 &&
                strcmp(line_ptrs[i + 4], "    subq $8, %rsp\n") == 0 &&
                strncmp(line_ptrs[i + 5], "    call ", 9) == 0 &&
                strcmp(line_ptrs[i + 6], "    addq $8, %rsp\n") == 0 &&
                strncmp(line_ptrs[i + 7], "    jmp .Lcall_end_", 19) == 0 &&
                strncmp(line_ptrs[i + 8], ".Lcall_aligned_", 15) == 0 &&
                strcmp(line_ptrs[i + 9], line_ptrs[i + 5]) == 0 &&
                strncmp(line_ptrs[i + 10], ".Lcall_end_", 11) == 0) {
                if (rsp_depth >= 0 && (rsp_depth % 16) == 0) {
                    line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0;
                    line_ptrs[i + 3][0] = 0; line_ptrs[i + 4][0] = 0;
                    line_ptrs[i + 6][0] = 0; line_ptrs[i + 7][0] = 0;
                    line_ptrs[i + 8][0] = 0; line_ptrs[i + 9][0] = 0; line_ptrs[i + 10][0] = 0;
                    eliminated += 10;
                } else if (rsp_depth >= 0 && (rsp_depth % 16) == 8) {
                    line_ptrs[i][0] = 0; line_ptrs[i + 1][0] = 0; line_ptrs[i + 2][0] = 0;
                    line_ptrs[i + 3][0] = 0;
                    line_ptrs[i + 7][0] = 0; line_ptrs[i + 8][0] = 0;
                    line_ptrs[i + 9][0] = 0; line_ptrs[i + 10][0] = 0;
                    eliminated += 8;
                }
                i += 11; continue;
            }
            if (rsp_depth >= 0) {
                long long val = 0;
                if (strncmp(l, "    subq $", 10) == 0 && strstr(l, ", %rsp\n") && sscanf(l, "    subq $%lld, %%rsp", &val) == 1) {
                    rsp_depth += val;
                } else if (strncmp(l, "    addq $", 10) == 0 && strstr(l, ", %rsp\n") && sscanf(l, "    addq $%lld, %%rsp", &val) == 1) {
                    rsp_depth -= val;
                } else if (strncmp(l, "    pushq ", 10) == 0 && strcmp(l, "    pushq %rbp\n") != 0) {
                    rsp_depth += 8;
                } else if (strncmp(l, "    popq ", 9) == 0 && strcmp(l, "    popq %rbp\n") != 0) {
                    rsp_depth -= 8;
                } else if (strchr(l, ':') != NULL || strncmp(l, "    j", 5) == 0 || strncmp(l, "    ret", 7) == 0 ||
                           (strstr(l, "%rsp") && strncmp(l, "    .", 5) != 0 && !strstr(l, "(%rsp)") &&
                            strcmp(l, "    movq %rsp, %r11\n") != 0 && strcmp(l, "    cmpq %r11, %rsp\n") != 0)) {
                    rsp_depth = -1;
                }
            }
            i++;
        }
    }

    *nlines_ptr = compact_peep_lines_ex(line_ptrs, nlines, 0);
    free(flit_pool);
    return eliminated;
}

