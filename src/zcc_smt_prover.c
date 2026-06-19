#include "zcc_smt_prover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../prelude.h"

int g_emit_smt_proofs = 0;
char g_smt_proofs_dir[256] = "/tmp/zcc_proofs";

/* Helper to strip % from register name */
static const char *clean_reg(const char *reg) {
    if (reg[0] == '%') return reg + 1;
    return reg;
}

/* Helper to ensure output directory exists */
static void ensure_proofs_dir(void) {
#ifdef _WIN32
    _mkdir(g_smt_proofs_dir);
#else
    mkdir(g_smt_proofs_dir, 0777);
#endif
}

/* Helper to start standard SMT2 file */
static FILE *start_smt_file(const char *name, size_t line_index) {
    ensure_proofs_dir();
    char path[512];
    sprintf(path, "%s/proof_%s_line%lu.smt2", g_smt_proofs_dir, name, (unsigned long)line_index);
    FILE *f = fopen(path, "w");
    if (!f) return NULL;

    fprintf(f, "; ZCC FORMAL VERIFICATION LAYER: %s AUTOMATED PROOF\n", name);
    fprintf(f, "(set-logic QF_ABV)\n\n");

    /* Declare memory array */
    fprintf(f, "(declare-fun mem_0 () (Array (_ BitVec 64) (_ BitVec 64)))\n");

    /* Declare canonical general purpose registers */
    fprintf(f, "(declare-fun rax_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rbx_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rcx_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rdx_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rsi_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rdi_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rsp_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun rbp_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r8_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r9_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r10_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r11_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r12_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r13_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r14_0 () (_ BitVec 64))\n");
    fprintf(f, "(declare-fun r15_0 () (_ BitVec 64))\n\n");

    return f;
}

/* 1. Push/Pop Sequence Pairs Prover */
void smt_prove_push_pop_elision(
    const char *reg1,
    const char *reg2,
    int is_replaced,
    size_t line_index
) {
    FILE *f = start_smt_file(is_replaced ? "push_pop_replace" : "push_pop_elide", line_index);
    if (!f) return;

    const char *r1 = clean_reg(reg1);
    const char *r2 = clean_reg(reg2);

    fprintf(f, "; --- PRE-OPTIMIZATION STATE ---\n");
    fprintf(f, "; push %s\n", r1);
    fprintf(f, "(define-fun rsp_1 () (_ BitVec 64) (bvsub rsp_0 #x0000000000000008))\n");
    fprintf(f, "(define-fun mem_1 () (Array (_ BitVec 64) (_ BitVec 64)) (store mem_0 rsp_1 %s_0))\n", r1);
    fprintf(f, "; pop %s\n", r2);
    fprintf(f, "(define-fun %s_1 () (_ BitVec 64) (select mem_1 rsp_1))\n", r2);
    fprintf(f, "(define-fun rsp_2 () (_ BitVec 64) (bvadd rsp_1 #x0000000000000008))\n\n");

    fprintf(f, "; --- POST-OPTIMIZATION STATE ---\n");
    if (is_replaced) {
        fprintf(f, "; mov %s, %s\n", r1, r2);
        fprintf(f, "(define-fun %s_post () (_ BitVec 64) %s_0)\n", r2, r1);
    } else {
        fprintf(f, "; elided push/pop\n");
        fprintf(f, "(define-fun %s_post () (_ BitVec 64) %s_0)\n", r2, r2);
    }
    fprintf(f, "(define-fun rsp_post () (_ BitVec 64) rsp_0)\n\n");

    fprintf(f, "; --- EQUIVALENCE PROOF Target ---\n");
    fprintf(f, "; Proving final states of modified target registers are semantics-identical\n");
    fprintf(f, "(assert (not (and (= %s_1 %s_post) (= rsp_2 rsp_post))))\n\n", r2, r2);

    fprintf(f, "(check-sat)\n");
    fprintf(f, "(get-model)\n");
    fclose(f);
}

/* 2. Arithmetic Nullification Prover */
void smt_prove_arith_nullification(
    const char *instruction,
    const char *reg,
    size_t line_index
) {
    FILE *f = start_smt_file("arith_nullify", line_index);
    if (!f) return;

    const char *r = clean_reg(reg);

    fprintf(f, "; --- PRE-OPTIMIZATION STATE ---\n");
    fprintf(f, "; %s", instruction);
    if (strstr(instruction, "addq")) {
        fprintf(f, "(define-fun %s_1 () (_ BitVec 64) (bvadd %s_0 #x0000000000000000))\n", r, r);
    } else {
        fprintf(f, "(define-fun %s_1 () (_ BitVec 64) (bvsub %s_0 #x0000000000000000))\n", r, r);
    }

    fprintf(f, "; --- POST-OPTIMIZATION STATE ---\n");
    fprintf(f, "; elided operation\n");
    fprintf(f, "(define-fun %s_post () (_ BitVec 64) %s_0)\n\n", r, r);

    fprintf(f, "; --- EQUIVALENCE PROOF Target ---\n");
    fprintf(f, "(assert (not (= %s_1 %s_post)))\n\n", r, r);

    fprintf(f, "(check-sat)\n");
    fprintf(f, "(get-model)\n");
    fclose(f);
}

/* 3. Push/Lea/Pop Triad Prover */
void smt_prove_push_lea_pop_triad(
    const char *lea_instruction,
    const char *pop_reg,
    size_t line_index
) {
    FILE *f = start_smt_file("push_lea_pop_triad", line_index);
    if (!f) return;

    const char *pr = clean_reg(pop_reg);

    /* Parse displacement and base register from leaq instruction */
    /* e.g. "    leaq -8(%rbp), %rax" */
    char base_reg[64] = "rbp";
    long long disp = 0;
    int is_rip = 0;

    const char *paren = strchr(lea_instruction, '(');
    if (paren) {
        const char *end_paren = strchr(paren, ')');
        if (end_paren) {
            char base[64];
            size_t len = end_paren - (paren + 1);
            if (len >= 63) len = 63;
            strncpy(base, paren + 1, len);
            base[len] = '\0';
            
            if (strcmp(base, "%rip") == 0) {
                is_rip = 1;
            } else {
                strcpy(base_reg, clean_reg(base));
            }

            /* Parse displacement constant */
            const char *p = lea_instruction;
            while (*p && isspace((unsigned char)*p)) p++;
            if (strncmp(p, "leaq ", 5) == 0) p += 5;
            while (*p && isspace((unsigned char)*p)) p++;
            
            char disp_str[64];
            size_t disp_len = paren - p;
            if (disp_len >= 63) disp_len = 63;
            strncpy(disp_str, p, disp_len);
            disp_str[disp_len] = '\0';
            disp = strtoll(disp_str, NULL, 0);
        }
    }

    fprintf(f, "; --- PRE-OPTIMIZATION STATE ---\n");
    fprintf(f, "; push rax\n");
    fprintf(f, "(define-fun rsp_1 () (_ BitVec 64) (bvsub rsp_0 #x0000000000000008))\n");
    fprintf(f, "(define-fun mem_1 () (Array (_ BitVec 64) (_ BitVec 64)) (store mem_0 rsp_1 rax_0))\n");
    
    fprintf(f, "; %s", lea_instruction);
    if (is_rip) {
        fprintf(f, "(declare-fun rip_label_addr () (_ BitVec 64))\n");
        fprintf(f, "(define-fun rax_1 () (_ BitVec 64) rip_label_addr)\n");
    } else {
        if (disp >= 0) {
            fprintf(f, "(define-fun rax_1 () (_ BitVec 64) (bvadd %s_0 #x%016llx))\n", base_reg, (unsigned long long)disp);
        } else {
            fprintf(f, "(define-fun rax_1 () (_ BitVec 64) (bvsub %s_0 #x%016llx))\n", base_reg, (unsigned long long)(-disp));
        }
    }

    fprintf(f, "; pop %s\n", pr);
    fprintf(f, "(define-fun %s_1 () (_ BitVec 64) (select mem_1 rsp_1))\n", pr);
    fprintf(f, "(define-fun rsp_2 () (_ BitVec 64) (bvadd rsp_1 #x0000000000000008))\n\n");

    fprintf(f, "; --- POST-OPTIMIZATION STATE ---\n");
    fprintf(f, "; mov rax, %s\n", pr);
    fprintf(f, "(define-fun %s_post () (_ BitVec 64) rax_0)\n", pr);
    fprintf(f, "; %s", lea_instruction);
    if (is_rip) {
        fprintf(f, "(define-fun rax_post () (_ BitVec 64) rip_label_addr)\n");
    } else {
        if (disp >= 0) {
            fprintf(f, "(define-fun rax_post () (_ BitVec 64) (bvadd %s_0 #x%016llx))\n", base_reg, (unsigned long long)disp);
        } else {
            fprintf(f, "(define-fun rax_post () (_ BitVec 64) (bvsub %s_0 #x%016llx))\n", base_reg, (unsigned long long)(-disp));
        }
    }
    fprintf(f, "(define-fun rsp_post () (_ BitVec 64) rsp_0)\n\n");

    fprintf(f, "; --- EQUIVALENCE PROOF Target ---\n");
    fprintf(f, "(assert (not (and (= rax_1 rax_post) (= %s_1 %s_post) (= rsp_2 rsp_post))))\n\n", pr, pr);

    fprintf(f, "(check-sat)\n");
    fprintf(f, "(get-model)\n");
    fclose(f);
}

static const char *op_to_smt_fun(int op) {
    switch(op) {
        case OP_ADD: return "bvadd";
        case OP_SUB: return "bvsub";
        case OP_MUL: return "bvmul";
        case OP_DIV: return "bvudiv";
        case OP_MOD: return "bvurem";
        case OP_BAND: return "bvand";
        case OP_BOR: return "bvor";
        case OP_BXOR: return "bvxor";
        case OP_BNOT: return "bvnot";
        case OP_SHL: return "bvshl";
        case OP_SHR: return "bvlshr";
        default: return NULL;
    }
}

static void print_hex64(FILE *f, long long val) {
    fprintf(f, "#x%016llx", (unsigned long long)val);
}

void smt_prove_ir_strength_reduction(
    const char *opt_name,
    int op_before,
    int op_after,
    long long val_before,
    long long val_after,
    int bit_width,
    size_t instr_id
) {
    FILE *f = start_smt_file(opt_name, instr_id);
    if (!f) return;

    fprintf(f, "; --- IR STRENGTH REDUCTION PROOF ---\n");
    fprintf(f, "(declare-fun x () (_ BitVec 64))\n\n");

    fprintf(f, "; Pre-optimization:\n");
    const char *smt_op_before = op_to_smt_fun(op_before);
    if (smt_op_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (%s x ", smt_op_before);
        print_hex64(f, val_before);
        fprintf(f, "))\n");
    } else {
        fprintf(f, "(define-fun pre () (_ BitVec 64) x)\n");
    }

    fprintf(f, "\n; Post-optimization:\n");
    const char *smt_op_after = op_to_smt_fun(op_after);
    if (op_after == OP_ADD && val_after == 0) {
        fprintf(f, "(define-fun post () (_ BitVec 64) (bvadd x x))\n");
    } else if (smt_op_after) {
        fprintf(f, "(define-fun post () (_ BitVec 64) (%s x ", smt_op_after);
        print_hex64(f, val_after);
        fprintf(f, "))\n");
    } else {
        fprintf(f, "(define-fun post () (_ BitVec 64) x)\n");
    }

    fprintf(f, "\n; Equivalence Target:\n");
    fprintf(f, "(assert (not (= pre post)))\n\n");
    fprintf(f, "(check-sat)\n");
    fclose(f);
}

void smt_prove_ir_peephole(
    const char *opt_name,
    int op_before,
    int op_after,
    long long val_before,
    long long val_after,
    int has_val_before,
    int has_val_after,
    int bit_width,
    size_t instr_id
) {
    FILE *f = start_smt_file(opt_name, instr_id);
    if (!f) return;

    fprintf(f, "; --- IR PEEPHOLE PROOF ---\n");
    fprintf(f, "(declare-fun x () (_ BitVec 64))\n\n");

    fprintf(f, "; Pre-optimization:\n");
    const char *smt_op_before = op_to_smt_fun(op_before);
    if (op_before == OP_SUB && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (bvsub x x))\n");
    } else if (op_before == OP_BXOR && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (bvxor x x))\n");
    } else if (op_before == OP_BAND && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (bvand x x))\n");
    } else if (op_before == OP_BOR && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (bvor x x))\n");
    } else if (op_before == OP_EQ && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (= x x) #x0000000000000001 #x0000000000000000))\n");
    } else if (op_before == OP_NE && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (not (= x x)) #x0000000000000001 #x0000000000000000))\n");
    } else if (op_before == OP_LT && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (bvslt x x) #x0000000000000001 #x0000000000000000))\n");
    } else if (op_before == OP_LE && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (bvsle x x) #x0000000000000001 #x0000000000000000))\n");
    } else if (op_before == OP_GT && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (bvsgt x x) #x0000000000000001 #x0000000000000000))\n");
    } else if (op_before == OP_GE && !has_val_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (ite (bvsge x x) #x0000000000000001 #x0000000000000000))\n");
    } else if (smt_op_before) {
        fprintf(f, "(define-fun pre () (_ BitVec 64) (%s x ", smt_op_before);
        print_hex64(f, val_before);
        fprintf(f, "))\n");
    } else {
        fprintf(f, "(define-fun pre () (_ BitVec 64) x)\n");
    }

    fprintf(f, "\n; Post-optimization:\n");
    const char *smt_op_after = op_to_smt_fun(op_after);
    if (op_after == OP_CONST) {
        fprintf(f, "(define-fun post () (_ BitVec 64) ");
        print_hex64(f, val_after);
        fprintf(f, ")\n");
    } else if (op_after == OP_BNOT) {
        fprintf(f, "(define-fun post () (_ BitVec 64) (bvnot x))\n");
    } else if (op_after == OP_COPY) {
        fprintf(f, "(define-fun post () (_ BitVec 64) x)\n");
    } else if (smt_op_after && has_val_after) {
        fprintf(f, "(define-fun post () (_ BitVec 64) (%s x ", smt_op_after);
        print_hex64(f, val_after);
        fprintf(f, "))\n");
    } else {
        fprintf(f, "(define-fun post () (_ BitVec 64) x)\n");
    }

    fprintf(f, "\n; Equivalence Target:\n");
    fprintf(f, "(assert (not (= pre post)))\n\n");
    fprintf(f, "(check-sat)\n");
    fclose(f);
}
