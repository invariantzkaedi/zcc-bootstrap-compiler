#include "zcc_smt_prover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

int g_emit_smt_proofs = 0;
char g_smt_proofs_dir[256] = "/tmp/zcc_proofs";

#define MAX_DECLARED_SYMBOLS 64

/* Per-proof thread-safe context */
typedef struct {
    FILE *file;
    char declared_symbols[MAX_DECLARED_SYMBOLS][128];
    int declared_symbols_count;
} SmtProofContext;

static unsigned int fnv1a_hash(const char *str) {
    unsigned int hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash;
}

/* Helper to sanitize register/memory symbols (thread-safe, collision-resilient, strips prefix decoration) */
static void clean_reg(const char *input, char *output, size_t output_size) {
    if (!input || !*input) {
        snprintf(output, output_size, "empty_sym__0");
        return;
    }

    const char *src = input;
    if (*src == '%') {
        src++;
    }

    if (!*src) {
        snprintf(output, output_size, "fallback__%08x", fnv1a_hash(input));
        return;
    }

    size_t i = 0;
    int semantic_change = 0;
    int truncated = 0;

    if (isdigit((unsigned char)src[0])) {
        if (i < output_size - 1) {
            output[i++] = '_';
        }
        semantic_change = 1;
    }

    for (; *src; src++) {
        if (i >= output_size - 1) {
            truncated = 1;
            break;
        }

        if (isalnum((unsigned char)*src) || *src == '_') {
            output[i++] = *src;
        } else {
            output[i++] = '_';
            semantic_change = 1;
        }
    }

    output[i] = '\0';

    if (semantic_change || truncated) {
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "__%08x", fnv1a_hash(input));

        size_t suffix_len = strlen(suffix);
        if (suffix_len >= output_size) {
            snprintf(output, output_size, "sym__%08x", fnv1a_hash(input));
            return;
        }

        size_t max_prefix = output_size - suffix_len - 1;
        output[max_prefix] = '\0';
        strcat(output, suffix);
    }
}

static int is_standard_register(const char *name) {
    const char *std_regs[] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
        "ax", "bx", "cx", "dx", "si", "di", "sp", "bp",
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
        "al", "bl", "cl", "dl", "sil", "dil", "spl", "bpl",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
        "rip", "eip", "ip"
    };
    int count = sizeof(std_regs) / sizeof(std_regs[0]);
    for (int i = 0; i < count; i++) {
        if (strcmp(name, std_regs[i]) == 0) return 1;
    }
    return 0;
}

static int remember_declared_symbol(SmtProofContext *ctx, const char *name) {
    if (ctx->declared_symbols_count >= MAX_DECLARED_SYMBOLS) {
        return 0;
    }
    snprintf(ctx->declared_symbols[ctx->declared_symbols_count], sizeof(ctx->declared_symbols[0]), "%s", name);
    ctx->declared_symbols_count++;
    return 1;
}

static int declare_register_if_needed(SmtProofContext *ctx, const char *clean_name) {
    if (is_standard_register(clean_name)) {
        return 1;
    }
    for (int i = 0; i < ctx->declared_symbols_count; i++) {
        if (strcmp(ctx->declared_symbols[i], clean_name) == 0) {
            return 1;
        }
    }
    if (!remember_declared_symbol(ctx, clean_name)) {
        fprintf(stderr, "SMT declaration registry exhausted\n");
        return 0;
    }
    fprintf(ctx->file, "(declare-fun %s_0 () (_ BitVec 64))\n", clean_name);
    return 1;
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
static SmtProofContext *start_smt_file(const char *name, size_t line_index) {
    ensure_proofs_dir();
    char path[512];
    sprintf(path, "%s/proof_%s_line%lu.smt2", g_smt_proofs_dir, name, (unsigned long)line_index);
    FILE *f = fopen(path, "w");
    if (!f) return NULL;

    SmtProofContext *ctx = (SmtProofContext *)malloc(sizeof(SmtProofContext));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    ctx->file = f;
    ctx->declared_symbols_count = 0;

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

    return ctx;
}

static void close_smt_file(SmtProofContext *ctx) {
    if (ctx) {
        if (ctx->file) {
            fclose(ctx->file);
        }
        free(ctx);
    }
}

/* 1. Push/Pop Sequence Pairs Prover */
void smt_prove_push_pop_elision(
    const char *reg1,
    const char *reg2,
    int is_replaced,
    size_t line_index
) {
    SmtProofContext *ctx = start_smt_file(is_replaced ? "push_pop_replace" : "push_pop_elide", line_index);
    if (!ctx) return;
    FILE *f = ctx->file;

    char r1[128];
    char r2[128];
    clean_reg(reg1, r1, sizeof(r1));
    clean_reg(reg2, r2, sizeof(r2));

    if (!declare_register_if_needed(ctx, r1) || !declare_register_if_needed(ctx, r2)) {
        close_smt_file(ctx);
        return;
    }

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
    close_smt_file(ctx);
}

/* 2. Arithmetic Nullification Prover */
void smt_prove_arith_nullification(
    const char *instruction,
    const char *reg,
    size_t line_index
) {
    SmtProofContext *ctx = start_smt_file("arith_nullify", line_index);
    if (!ctx) return;
    FILE *f = ctx->file;

    char r[128];
    clean_reg(reg, r, sizeof(r));

    if (!declare_register_if_needed(ctx, r)) {
        close_smt_file(ctx);
        return;
    }

    fprintf(f, "; --- PRE-OPTIMIZATION STATE ---\n");
    fprintf(f, "; %s\n", instruction);
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
    close_smt_file(ctx);
}

/* 3. Push/Lea/Pop Triad Prover */
void smt_prove_push_lea_pop_triad(
    const char *lea_instruction,
    const char *pop_reg,
    size_t line_index
) {
    SmtProofContext *ctx = start_smt_file("push_lea_pop_triad", line_index);
    if (!ctx) return;
    FILE *f = ctx->file;

    char pr[128];
    clean_reg(pop_reg, pr, sizeof(pr));

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
                clean_reg(base, base_reg, sizeof(base_reg));
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

    if (!declare_register_if_needed(ctx, pr)) {
        close_smt_file(ctx);
        return;
    }
    if (!is_rip) {
        if (!declare_register_if_needed(ctx, base_reg)) {
            close_smt_file(ctx);
            return;
        }
    }

    fprintf(f, "; --- PRE-OPTIMIZATION STATE ---\n");
    fprintf(f, "; push rax\n");
    fprintf(f, "(define-fun rsp_1 () (_ BitVec 64) (bvsub rsp_0 #x0000000000000008))\n");
    fprintf(f, "(define-fun mem_1 () (Array (_ BitVec 64) (_ BitVec 64)) (store mem_0 rsp_1 rax_0))\n");
    
    fprintf(f, "; %s\n", lea_instruction);
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

    int pr_is_rax = (strcmp(pr, "rax") == 0);

    fprintf(f, "; pop %s\n", pr);
    if (pr_is_rax) {
        fprintf(f, "(define-fun rax_2 () (_ BitVec 64) (select mem_1 rsp_1))\n");
    } else {
        fprintf(f, "(define-fun %s_1 () (_ BitVec 64) (select mem_1 rsp_1))\n", pr);
    }
    fprintf(f, "(define-fun rsp_2 () (_ BitVec 64) (bvadd rsp_1 #x0000000000000008))\n\n");

    fprintf(f, "; --- POST-OPTIMIZATION STATE ---\n");
    fprintf(f, "; mov rax, %s\n", pr);
    if (pr_is_rax) {
        fprintf(f, "(define-fun rax_post () (_ BitVec 64) rax_0)\n");
    } else {
        fprintf(f, "(define-fun %s_post () (_ BitVec 64) rax_0)\n", pr);
        fprintf(f, "; %s\n", lea_instruction);
        if (is_rip) {
            fprintf(f, "(define-fun rax_post () (_ BitVec 64) rip_label_addr)\n");
        } else {
            if (disp >= 0) {
                fprintf(f, "(define-fun rax_post () (_ BitVec 64) (bvadd %s_0 #x%016llx))\n", base_reg, (unsigned long long)disp);
            } else {
                fprintf(f, "(define-fun rax_post () (_ BitVec 64) (bvsub %s_0 #x%016llx))\n", base_reg, (unsigned long long)(-disp));
            }
        }
    }
    fprintf(f, "(define-fun rsp_post () (_ BitVec 64) rsp_0)\n\n");

    fprintf(f, "; --- EQUIVALENCE PROOF Target ---\n");
    if (pr_is_rax) {
        fprintf(f, "(assert (not (and (= rax_2 rax_post) (= rsp_2 rsp_post))))\n\n");
    } else {
        fprintf(f, "(assert (not (and (= rax_1 rax_post) (= %s_1 %s_post) (= rsp_2 rsp_post))))\n\n", pr, pr);
    }

    fprintf(f, "(check-sat)\n");
    fprintf(f, "(get-model)\n");
    close_smt_file(ctx);
}

void smt_prove_stack_alignment(
    const char *func_name,
    size_t stack_size,
    int alignment,
    size_t line_index
) {
    if (!g_emit_smt_proofs) return;

    SmtProofContext *ctx = start_smt_file("stack_align", line_index);
    if (!ctx) return;
    FILE *f = ctx->file;

    fprintf(f, "; SMT-LIB2 Proof: 16-Byte Stack Alignment Invariant\n");
    fprintf(f, "; Function: %s, Requested Stack: %zu, Required Align: %d\n\n",
            func_name ? func_name : "anon", stack_size, alignment);
    fprintf(f, "(declare-const sp_0 (_ BitVec 64))\n");
    fprintf(f, "; Invariant: Initial SP is 16-byte aligned\n");
    fprintf(f, "(assert (= (bvand sp_0 #x000000000000000F) #x0000000000000000))\n\n");

    size_t align_mask = (size_t)(alignment - 1);
    size_t aligned_size = (stack_size + align_mask) & ~align_mask;

    fprintf(f, "(define-fun sp_alloc () (_ BitVec 64) (bvsub sp_0 #x%016llx))\n", (unsigned long long)aligned_size);
    fprintf(f, "; Prove: Allocated SP maintains 16-byte alignment\n");
    fprintf(f, "(assert (not (= (bvand sp_alloc #x000000000000000F) #x0000000000000000)))\n\n");
    fprintf(f, "(check-sat)\n");
    fprintf(f, "(get-model)\n");
    close_smt_file(ctx);
}


