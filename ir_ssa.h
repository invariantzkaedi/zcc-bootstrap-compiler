#pragma once
#include "ir.h"
#include "ir_pass_manager.h"
#include "ir_dominance.h"

ir_pass_result_t ir_pass_ssa(void *mod_ptr);

/* SSA Renaming API */
void ssa_rename_function(ir_func_t *fn, const dom_cfg_t *cfg);
