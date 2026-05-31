#ifndef ZCC_SMT_PROVER_H
#define ZCC_SMT_PROVER_H

#include <stddef.h>

/* Global configuration flags */
extern int g_emit_smt_proofs;
extern char g_smt_proofs_dir[256];

/* Prover API */
void smt_prove_push_pop_elision(
    const char *reg1,
    const char *reg2,
    int is_replaced,
    size_t line_index
);

void smt_prove_arith_nullification(
    const char *instruction,
    const char *reg,
    size_t line_index
);

void smt_prove_push_lea_pop_triad(
    const char *lea_instruction,
    const char *pop_reg,
    size_t line_index
);

#endif /* ZCC_SMT_PROVER_H */
