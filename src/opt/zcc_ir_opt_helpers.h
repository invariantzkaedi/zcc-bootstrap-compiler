#ifndef ZCC_IR_OPT_HELPERS_H
#define ZCC_IR_OPT_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include "zcc_ir.h"

bool reg_is_const(Function *fn, int reg, int64_t *out);
int  make_const(Function *fn, IRType ty, int64_t k, Instr *insert_before);
Instr* def_of(Function *fn, int reg);
bool replace_all_uses(Function *fn, int old_reg, int new_reg);
bool erase_instr(Function *fn, Instr *it);
bool rewrite_to_copy(Function *fn, Instr *it, int src_reg);
bool rewrite_to_const(Function *fn, Instr *it, int64_t k);
int  count_uses(Function *fn, RegID reg);
int  resolve_copy(Function *fn, int reg);

bool will_overflow_add(IRType ty, int64_t a, int64_t b);
bool will_overflow_mul(IRType ty, int64_t a, int64_t b);
bool is_all_ones_for_type(IRType ty, int64_t k);

void rebuild_def_use(Function *fn);
int fn_count_instructions(const Function *fn);
int fn_count_blocks(const Function *fn);
int fn_max_register(const Function *fn);
void licm_build_def_block(Function *fn);

#endif
