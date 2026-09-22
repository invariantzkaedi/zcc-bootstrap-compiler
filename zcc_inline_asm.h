/* ================================================================ */
/* ZCC Extended GCC Inline Assembly Engine (LIMIT-001)               */
/* System V AMD64 Operand Constraints, Register Binding & Codegen   */
/* ================================================================ */

#ifndef ZCC_INLINE_ASM_H
#define ZCC_INLINE_ASM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#endif /* ZCC_INLINE_ASM_H */

/* ================================================================ */
/* PARSER SECTION (included from part3.c)                           */
/* ================================================================ */
#ifdef ZCC_INLINE_ASM_PARSE
#undef ZCC_INLINE_ASM_PARSE

static void parse_extended_asm_operands(Compiler *cc, Node *asmn) {
    AsmOperand operands[32];
    int num_ops = 0;
    char *clobbers[32];
    int num_clobs = 0;
    int section = 0; /* 1: outputs, 2: inputs, 3: clobbers */

    while (cc->tk == TK_COLON && section < 3) {
        next_token(cc); /* consume ':' */
        section++;

        if (section == 1) {
            /* Output operands */
            while (cc->tk != TK_COLON && cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
                if (num_ops >= 32) {
                    error(cc, "too many inline asm operands");
                    break;
                }
                AsmOperand *op = &operands[num_ops];
                memset(op, 0, sizeof(AsmOperand));
                op->is_output = 1;

                /* Optional [name] */
                if (cc->tk == TK_LBRACKET) {
                    next_token(cc);
                    if (cc->tk == TK_IDENT) {
                        strncpy(op->symbol_name, cc->tk_text, 63);
                        next_token(cc);
                    }
                    expect(cc, TK_RBRACKET);
                }

                /* Constraint string */
                if (cc->tk == TK_STR) {
                    strncpy(op->constraint, cc->tk_str, 31);
                    next_token(cc);
                }

                if (strchr(op->constraint, '+')) {
                    op->is_rw = 1;
                }

                expect(cc, TK_LPAREN);
                op->expr = parse_expr(cc);
                expect(cc, TK_RPAREN);

                num_ops++;
                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else {
                    break;
                }
            }
        } else if (section == 2) {
            /* Input operands */
            while (cc->tk != TK_COLON && cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
                if (num_ops >= 32) {
                    error(cc, "too many inline asm operands");
                    break;
                }
                AsmOperand *op = &operands[num_ops];
                memset(op, 0, sizeof(AsmOperand));
                op->is_output = 0;

                /* Optional [name] */
                if (cc->tk == TK_LBRACKET) {
                    next_token(cc);
                    if (cc->tk == TK_IDENT) {
                        strncpy(op->symbol_name, cc->tk_text, 63);
                        next_token(cc);
                    }
                    expect(cc, TK_RBRACKET);
                }

                /* Constraint string */
                if (cc->tk == TK_STR) {
                    strncpy(op->constraint, cc->tk_str, 31);
                    next_token(cc);
                }

                expect(cc, TK_LPAREN);
                op->expr = parse_expr(cc);
                expect(cc, TK_RPAREN);

                num_ops++;
                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else {
                    break;
                }
            }
        } else if (section == 3) {
            /* Clobber list */
            while (cc->tk != TK_COLON && cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
                if (cc->tk == TK_STR) {
                    if (num_clobs < 32) {
                        clobbers[num_clobs++] = cc_strdup(cc, cc->tk_str);
                    }
                    next_token(cc);
                }
                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else {
                    break;
                }
            }
        }
    }

    /* Skip optional 4th section (goto labels) if present */
    if (cc->tk == TK_COLON) {
        next_token(cc);
        while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
            next_token(cc);
        }
    }

    if (cc->tk == TK_RPAREN) {
        next_token(cc);
    }

    if (num_ops > 0) {
        asmn->num_asm_operands = num_ops;
        asmn->asm_operands = (AsmOperand *)cc_alloc(cc, sizeof(AsmOperand) * num_ops);
        memcpy(asmn->asm_operands, operands, sizeof(AsmOperand) * num_ops);
    }
    if (num_clobs > 0) {
        asmn->num_asm_clobbers = num_clobs;
        asmn->asm_clobbers = (char **)cc_alloc(cc, sizeof(char *) * num_clobs);
        memcpy(asmn->asm_clobbers, clobbers, sizeof(char *) * num_clobs);
    }
}

#endif /* ZCC_INLINE_ASM_PARSE */

/* ================================================================ */
/* CODE GENERATION SECTION (included from part4.c)                  */
/* ================================================================ */
#ifdef ZCC_INLINE_ASM_CODEGEN
#undef ZCC_INLINE_ASM_CODEGEN

enum {
    ASM_REG_RAX = 0,
    ASM_REG_RCX = 1,
    ASM_REG_RDX = 2,
    ASM_REG_RBX = 3,
    ASM_REG_RSI = 4,
    ASM_REG_RDI = 5,
    ASM_REG_R8  = 6,
    ASM_REG_R9  = 7,
    ASM_REG_R10 = 8,
    ASM_REG_R11 = 9,
    ASM_REG_MEM = 10,
    ASM_REG_IMM = 11
};

static const char *asm_reg_names_64[10] = {
    "%rax", "%rcx", "%rdx", "%rbx", "%rsi", "%rdi", "%r8", "%r9", "%r10", "%r11"
};
static const char *asm_reg_names_32[10] = {
    "%eax", "%ecx", "%edx", "%ebx", "%esi", "%edi", "%r8d", "%r9d", "%r10d", "%r11d"
};
static const char *asm_reg_names_16[10] = {
    "%ax", "%cx", "%dx", "%bx", "%si", "%di", "%r8w", "%r9w", "%r10w", "%r11w"
};
static const char *asm_reg_names_8[10] = {
    "%al", "%cl", "%dl", "%bl", "%sil", "%dil", "%r8b", "%r9b", "%r10b", "%r11b"
};
static const char *asm_reg_names_8h[4] = {
    "%ah", "%ch", "%dh", "%bh"
};

static int get_asm_fixed_reg(const char *c, AsmOperand *op) {
    if (strchr(c, 'r') || strchr(c, 'q') || strchr(c, 'g')) return -1; /* Preference for register allocation */
    while (*c) {
        if (*c == 'a') return ASM_REG_RAX;
        if (*c == 'c') return ASM_REG_RCX;
        if (*c == 'd') return ASM_REG_RDX;
        if (*c == 'b') return ASM_REG_RBX;
        if (*c == 'S') return ASM_REG_RSI;
        if (*c == 'D') return ASM_REG_RDI;
        if (*c == 'm') return ASM_REG_MEM;
        if (*c == 'i' || *c == 'n') {
            if (op && op->expr && op->expr->kind == ND_NUM) return ASM_REG_IMM;
            return -1;
        }
        c++;
    }
    return -1;
}

static const char *get_asm_operand_repr(Compiler *cc, AsmOperand *op, char modifier, char *buf, size_t buf_sz) {
    if (op->reg_id == ASM_REG_IMM) {
        if (op->expr && op->expr->kind == ND_NUM) {
            snprintf(buf, buf_sz, "$%lld", (long long)op->expr->int_val);
            return buf;
        }
        return "$0";
    }

    if (op->reg_id == ASM_REG_MEM) {
        if (op->expr && op->expr->kind == ND_VAR && op->expr->sym) {
            if (op->expr->sym->is_local) {
                snprintf(buf, buf_sz, "%d(%%rbp)", op->expr->sym->stack_offset);
            } else {
                snprintf(buf, buf_sz, "%s(%%rip)", op->expr->sym->name);
            }
            return buf;
        }
        return "(%r11)";
    }

    int reg_id = op->reg_id;
    if (reg_id < 0 || reg_id >= 10) reg_id = ASM_REG_RAX;

    if (modifier == 'q') return asm_reg_names_64[reg_id];
    if (modifier == 'k') return asm_reg_names_32[reg_id];
    if (modifier == 'w') return asm_reg_names_16[reg_id];
    if (modifier == 'b') return asm_reg_names_8[reg_id];
    if (modifier == 'h' && reg_id < 4) return asm_reg_names_8h[reg_id];

    int sz = (op->expr && op->expr->type) ? type_size(op->expr->type) : 8;
    if (sz == 4) return asm_reg_names_32[reg_id];
    if (sz == 2) return asm_reg_names_16[reg_id];
    if (sz == 1) return asm_reg_names_8[reg_id];
    return asm_reg_names_64[reg_id];
}

static void codegen_extended_asm(Compiler *cc, Node *node) {
    int i, k;
    unsigned int used_mask = 0;
    int match_to[32];
    static const int alloc_order[10] = {
        ASM_REG_RAX, ASM_REG_RDX, ASM_REG_RCX, ASM_REG_RSI, ASM_REG_RDI,
        ASM_REG_R8, ASM_REG_R9, ASM_REG_R10, ASM_REG_R11, ASM_REG_RBX
    };

    for (i = 0; i < 32; i++) match_to[i] = -1;

    /* 1. Mark registers listed in clobbers */
    for (i = 0; i < node->num_asm_clobbers; i++) {
        const char *clob = node->asm_clobbers[i];
        if (strstr(clob, "rax") || strstr(clob, "eax") || strstr(clob, "ax")) used_mask |= (1 << ASM_REG_RAX);
        if (strstr(clob, "rcx") || strstr(clob, "ecx") || strstr(clob, "cx")) used_mask |= (1 << ASM_REG_RCX);
        if (strstr(clob, "rdx") || strstr(clob, "edx") || strstr(clob, "dx")) used_mask |= (1 << ASM_REG_RDX);
        if (strstr(clob, "rbx") || strstr(clob, "ebx") || strstr(clob, "bx")) used_mask |= (1 << ASM_REG_RBX);
        if (strstr(clob, "rsi") || strstr(clob, "esi") || strstr(clob, "si")) used_mask |= (1 << ASM_REG_RSI);
        if (strstr(clob, "rdi") || strstr(clob, "edi") || strstr(clob, "di")) used_mask |= (1 << ASM_REG_RDI);
        if (strstr(clob, "r8"))  used_mask |= (1 << ASM_REG_R8);
        if (strstr(clob, "r9"))  used_mask |= (1 << ASM_REG_R9);
        if (strstr(clob, "r10")) used_mask |= (1 << ASM_REG_R10);
        if (strstr(clob, "r11")) used_mask |= (1 << ASM_REG_R11);
    }

    /* 2. Assign fixed register constraints */
    for (i = 0; i < node->num_asm_operands; i++) {
        AsmOperand *op = &node->asm_operands[i];
        int fixed_reg = get_asm_fixed_reg(op->constraint, op);
        if (fixed_reg >= 0) {
            op->reg_id = fixed_reg;
            if (fixed_reg != ASM_REG_MEM && fixed_reg != ASM_REG_IMM) {
                used_mask |= (1 << fixed_reg);
            }
        } else {
            op->reg_id = -1;
        }
    }

    /* 3. Identify matching constraints ("0".."9") */
    for (i = 0; i < node->num_asm_operands; i++) {
        AsmOperand *op = &node->asm_operands[i];
        if (op->reg_id < 0) {
            for (k = 0; op->constraint[k]; k++) {
                if (op->constraint[k] >= '0' && op->constraint[k] <= '9') {
                    int m = op->constraint[k] - '0';
                    if (m < i && m < node->num_asm_operands) {
                        match_to[i] = m;
                        break;
                    }
                }
            }
        }
    }

    /* 4. Allocate general "r" operands from pool */
    for (i = 0; i < node->num_asm_operands; i++) {
        AsmOperand *op = &node->asm_operands[i];
        if (match_to[i] >= 0) {
            int target_op = match_to[i];
            if (node->asm_operands[target_op].reg_id < 0) {
                int allocated = -1;
                for (k = 0; k < 10; k++) {
                    int cand = alloc_order[k];
                    if (!(used_mask & (1 << cand))) {
                        allocated = cand;
                        used_mask |= (1 << cand);
                        break;
                    }
                }
                if (allocated < 0) allocated = ASM_REG_R10;
                node->asm_operands[target_op].reg_id = allocated;
            }
            op->reg_id = node->asm_operands[target_op].reg_id;
        } else if (op->reg_id < 0) {
            int allocated = -1;
            for (k = 0; k < 10; k++) {
                int cand = alloc_order[k];
                if (!(used_mask & (1 << cand))) {
                    allocated = cand;
                    used_mask |= (1 << cand);
                    break;
                }
            }
            if (allocated < 0) allocated = ASM_REG_R10;
            op->reg_id = allocated;
        }
    }

    /* Callee-saved register protection */
    int uses_rbx = (used_mask & (1 << ASM_REG_RBX)) != 0;
    int clob_r12 = 0, clob_r13 = 0, clob_r14 = 0, clob_r15 = 0;
    for (i = 0; i < node->num_asm_clobbers; i++) {
        const char *clob = node->asm_clobbers[i];
        if (strstr(clob, "r12")) clob_r12 = 1;
        if (strstr(clob, "r13")) clob_r13 = 1;
        if (strstr(clob, "r14")) clob_r14 = 1;
        if (strstr(clob, "r15")) clob_r15 = 1;
        if (strstr(clob, "rbx") || strstr(clob, "ebx") || strstr(clob, "bx")) uses_rbx = 1;
    }
    if (uses_rbx) fprintf(cc->out, "    pushq %%rbx\n");
    if (clob_r12) fprintf(cc->out, "    pushq %%r12\n");
    if (clob_r13) fprintf(cc->out, "    pushq %%r13\n");
    if (clob_r14) fprintf(cc->out, "    pushq %%r14\n");
    if (clob_r15) fprintf(cc->out, "    pushq %%r15\n");

    /* 5. Evaluate and load input operands (including read-write outputs) onto stack */
    int input_eval_count = 0;
    int eval_op_indices[32];
    for (i = 0; i < node->num_asm_operands; i++) {
        AsmOperand *op = &node->asm_operands[i];
        if (!op->is_output || op->is_rw) {
            if (op->reg_id != ASM_REG_MEM && op->reg_id != ASM_REG_IMM) {
                codegen_expr_checked(cc, op->expr);
                fprintf(cc->out, "    pushq %%rax\n");
                eval_op_indices[input_eval_count++] = i;
            }
        }
    }

    /* Pop evaluated inputs into their assigned physical registers (reverse order) */
    for (i = input_eval_count - 1; i >= 0; i--) {
        int op_idx = eval_op_indices[i];
        AsmOperand *op = &node->asm_operands[op_idx];
        fprintf(cc->out, "    popq %s\n", asm_reg_names_64[op->reg_id]);
    }

    /* 6. Template String Substitution */
    char sub_buf[4096];
    int sub_len = 0;
    const char *tpl = node->asm_string ? node->asm_string : "";
    int j = 0;

    while (tpl[j] && sub_len < 4000) {
        if (tpl[j] == '%' && tpl[j+1] == '%') {
            sub_buf[sub_len++] = '%';
            j += 2;
        } else if (tpl[j] == '%') {
            j++;
            char modifier = 0;
            if (tpl[j] == 'k' || tpl[j] == 'q' || tpl[j] == 'w' || tpl[j] == 'b' || tpl[j] == 'h') {
                modifier = tpl[j++];
            }
            if (isdigit(tpl[j])) {
                int op_idx = 0;
                while (isdigit(tpl[j])) {
                    op_idx = op_idx * 10 + (tpl[j++] - '0');
                }
                if (op_idx < node->num_asm_operands) {
                    char mem_buf[64];
                    const char *repr = get_asm_operand_repr(cc, &node->asm_operands[op_idx], modifier, mem_buf, sizeof(mem_buf));
                    int rlen = strlen(repr);
                    if (sub_len + rlen < 4000) {
                        strcpy(sub_buf + sub_len, repr);
                        sub_len += rlen;
                    }
                }
            } else if (tpl[j] == '[') {
                j++;
                char name_buf[64];
                int nlen = 0;
                while (tpl[j] && tpl[j] != ']' && nlen < 63) {
                    name_buf[nlen++] = tpl[j++];
                }
                name_buf[nlen] = 0;
                if (tpl[j] == ']') j++;
                int found_idx = -1;
                for (k = 0; k < node->num_asm_operands; k++) {
                    if (strcmp(node->asm_operands[k].symbol_name, name_buf) == 0) {
                        found_idx = k;
                        break;
                    }
                }
                if (found_idx >= 0) {
                    char mem_buf[64];
                    const char *repr = get_asm_operand_repr(cc, &node->asm_operands[found_idx], modifier, mem_buf, sizeof(mem_buf));
                    int rlen = strlen(repr);
                    if (sub_len + rlen < 4000) {
                        strcpy(sub_buf + sub_len, repr);
                        sub_len += rlen;
                    }
                }
            } else {
                sub_buf[sub_len++] = '%';
                if (modifier) sub_buf[sub_len++] = modifier;
            }
        } else {
            sub_buf[sub_len++] = tpl[j++];
        }
    }
    sub_buf[sub_len] = 0;

    /* 7. Emit substituted assembly lines */
    char *line_p = sub_buf;
    while (*line_p) {
        char *nl = strchr(line_p, '\n');
        if (nl) *nl = 0;
        while (*line_p == ' ' || *line_p == '\t') line_p++;
        char *end = line_p + strlen(line_p) - 1;
        while (end >= line_p && (*end == ' ' || *end == '\t' || *end == '\r' || *end == ';')) {
            *end = 0;
            end--;
        }
        if (*line_p) {
            fprintf(cc->out, "    %s\n", line_p);
        }
        if (!nl) break;
        line_p = nl + 1;
    }

    /* 8. Write back output operands to their target lvalues */
    for (i = 0; i < node->num_asm_operands; i++) {
        AsmOperand *op = &node->asm_operands[i];
        if (op->is_output && op->reg_id != ASM_REG_MEM && op->reg_id != ASM_REG_IMM) {
            int sz = (op->expr && op->expr->type) ? type_size(op->expr->type) : 8;
            if (op->expr && op->expr->kind == ND_VAR && op->expr->sym) {
                if (op->expr->sym->is_local) {
                    int off = op->expr->sym->stack_offset;
                    if (sz == 8) fprintf(cc->out, "    movq %s, %d(%%rbp)\n", asm_reg_names_64[op->reg_id], off);
                    else if (sz == 4) fprintf(cc->out, "    movl %s, %d(%%rbp)\n", asm_reg_names_32[op->reg_id], off);
                    else if (sz == 2) fprintf(cc->out, "    movw %s, %d(%%rbp)\n", asm_reg_names_16[op->reg_id], off);
                    else if (sz == 1) fprintf(cc->out, "    movb %s, %d(%%rbp)\n", asm_reg_names_8[op->reg_id], off);
                } else {
                    char *name = op->expr->sym->name;
                    if (sz == 8) fprintf(cc->out, "    movq %s, %s(%%rip)\n", asm_reg_names_64[op->reg_id], name);
                    else if (sz == 4) fprintf(cc->out, "    movl %s, %s(%%rip)\n", asm_reg_names_32[op->reg_id], name);
                    else if (sz == 2) fprintf(cc->out, "    movw %s, %s(%%rip)\n", asm_reg_names_16[op->reg_id], name);
                    else if (sz == 1) fprintf(cc->out, "    movb %s, %s(%%rip)\n", asm_reg_names_8[op->reg_id], name);
                }
            } else if (op->expr) {
                /* General lvalue: compute address into %rax, then store */
                fprintf(cc->out, "    pushq %s\n", asm_reg_names_64[op->reg_id]);
                codegen_addr_checked(cc, op->expr);
                fprintf(cc->out, "    popq %%r11\n");
                if (sz == 8) fprintf(cc->out, "    movq %%r11, (%%rax)\n");
                else if (sz == 4) fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
                else if (sz == 2) fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
                else if (sz == 1) fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
            }
        }
    }

    if (clob_r15) fprintf(cc->out, "    popq %%r15\n");
    if (clob_r14) fprintf(cc->out, "    popq %%r14\n");
    if (clob_r13) fprintf(cc->out, "    popq %%r13\n");
    if (clob_r12) fprintf(cc->out, "    popq %%r12\n");
    if (uses_rbx) fprintf(cc->out, "    popq %%rbx\n");
}

#endif /* ZCC_INLINE_ASM_CODEGEN */
