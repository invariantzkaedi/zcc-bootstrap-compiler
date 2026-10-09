#include "src/zcc_smt_prover.h"
#include "prelude.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern char g_smt_proofs_dir[256];
extern int g_emit_smt_proofs;

void *thread_func(void *arg) {
    long id = (long)arg;
    char reg1[64];
    char reg2[64];
    sprintf(reg1, "%%thread_%ld_r1", id);
    sprintf(reg2, "%%thread_%ld_r2", id);
    smt_prove_push_pop_elision(reg1, reg2, 1, 200 + id);
    return NULL;
}

int main() {
    g_emit_smt_proofs = 1;
    strcpy(g_smt_proofs_dir, "test_proofs");

    // 1. rax (replaced)
    smt_prove_push_pop_elision("rax", "rbx", 1, 100);

    // 2. %tmp.1 (replaced)
    smt_prove_push_pop_elision("%tmp.1", "rcx", 1, 101);

    // 3. %foo-bar (replaced)
    smt_prove_push_pop_elision("%foo-bar", "rdx", 1, 102);

    // 4. %foo_bar (replaced)
    smt_prove_push_pop_elision("%foo_bar", "rsi", 1, 103);

    // 5. %123temp (replaced)
    smt_prove_push_pop_elision("%123temp", "rdi", 1, 104);

    // 6. % (replaced)
    smt_prove_push_pop_elision("%", "r8", 1, 105);

    // 7. two identical nonstandard operands (elided)
    smt_prove_push_pop_elision("%nonstd", "%nonstd", 0, 106);

    // 8. two distinct operands that sanitize identically (replaced)
    smt_prove_push_pop_elision("%foo-bar", "%foo_bar", 1, 107);

    // 9. names longer than 127 characters (replaced)
    char long_name1[200];
    char long_name2[200];
    memset(long_name1, 'a', 150);
    long_name1[150] = '\0';
    memset(long_name2, 'b', 150);
    long_name2[150] = '\0';
    smt_prove_push_pop_elision(long_name1, long_name2, 1, 108);

    // 10. RIP-relative addressing
    smt_prove_push_lea_pop_triad("leaq 8(%rip), %rax", "rax", 109);

    // 11. multiple dynamic registers in one proof (replaced)
    smt_prove_push_pop_elision("%dyn1", "%dyn2", 1, 110);

    // 12. prefixed standard registers (replaced)
    smt_prove_push_pop_elision("%rax", "%rbx", 1, 111);

    // 13. parallel proof generation (threads 0 to 3)
    pthread_t threads[4];
    for (long i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void *)i);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    // 14. IR Strength Reduction Proofs
    smt_prove_ir_strength_reduction("ir_mul2_add", OP_MUL, OP_ADD, 2, 0, 64, 301);
    smt_prove_ir_strength_reduction("ir_mul_shl", OP_MUL, OP_SHL, 8, 3, 64, 302);
    smt_prove_ir_strength_reduction("ir_div_shr", OP_DIV, OP_SHR, 16, 4, 64, 303);
    smt_prove_ir_strength_reduction("ir_mod_band", OP_MOD, OP_BAND, 32, 31, 64, 304);
    smt_prove_ir_strength_reduction("ir_mul2_add", OP_MUL, OP_ADD, 2, 0, 32, 305);

    // 15. IR Algebraic Peephole Proofs
    smt_prove_ir_peephole("ir_peep_self_zero", OP_SUB, OP_CONST, 0, 0, 0, 1, 64, 401);
    smt_prove_ir_peephole("ir_peep_self_zero", OP_BXOR, OP_CONST, 0, 0, 0, 1, 64, 402);
    smt_prove_ir_peephole("ir_peep_self_copy", OP_BAND, OP_COPY, 0, 0, 0, 0, 64, 403);
    smt_prove_ir_peephole("ir_peep_band_not_self", OP_BAND, OP_CONST, 0, 0, 0, 1, 64, 404);
    smt_prove_ir_peephole("ir_peep_bor_not_self", OP_BOR, OP_CONST, -1, -1, 0, 1, 64, 405);
    smt_prove_ir_peephole("ir_peep_double_bnot", OP_BNOT, OP_COPY, 0, 0, 0, 0, 64, 406);
    smt_prove_ir_peephole("ir_peep_op_zero", OP_ADD, OP_COPY, 0, 0, 1, 0, 64, 407);
    smt_prove_ir_peephole("ir_peep_op_one", OP_MUL, OP_COPY, 1, 0, 1, 0, 64, 408);
    smt_prove_ir_peephole("ir_peep_op_zero_const", OP_MUL, OP_CONST, 0, 0, 1, 1, 64, 409);
    smt_prove_ir_peephole("ir_peep_band_neg1", OP_BAND, OP_COPY, -1, 0, 1, 0, 64, 410);
    smt_prove_ir_peephole("ir_peep_bxor_neg1", OP_BXOR, OP_BNOT, -1, 0, 1, 0, 64, 411);
    smt_prove_ir_peephole("ir_peep_cmp_self", OP_EQ, OP_CONST, 0, 1, 0, 1, 64, 412);
    smt_prove_ir_peephole("ir_peep_cmp_self", OP_NE, OP_CONST, 0, 0, 0, 1, 64, 413);
    smt_prove_ir_peephole("ir_peep_shift_zero", OP_SHL, OP_COPY, 0, 0, 1, 0, 64, 414);

    // 16. Stack Alignment Proof
    smt_prove_stack_alignment("main", 64, 16, 501);

    printf("Generated test SMT proofs successfully.\n");
    return 0;
}
