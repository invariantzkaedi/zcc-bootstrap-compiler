#include "zcc_smt_prover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

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
