#ifndef ZCC_LOOP_VALIDATOR_H
#define ZCC_LOOP_VALIDATOR_H

#include "zcc_ir.h"
#include <stdbool.h>

typedef struct {
    BlockID preheader;
    BlockID header;
    BlockID latch;
    BlockID exit;

    RegID ind_var;        /* Induction variable register */
    int64_t step;         /* Constant step amount */
    Opcode step_op;       /* OP_ADD or OP_SUB */

    RegID cmp_reg;        /* Register holding compare result */
    Opcode cmp_op;        /* Comparison opcode: OP_LT, OP_LE, etc. (relative to ind_var) */
    int64_t limit;        /* Loop limit/bound constant value */
    bool limit_is_const;

    int64_t trip_count;   /* Computed trip count, or -1 if dynamic/uncomputable */
} LoopCanonicalInfo;

bool opt_detect_canonical_loop(Function *fn, BlockID header_id, LoopCanonicalInfo *info);

#endif
