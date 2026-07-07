#include "clone_remap.h"
#include "zcc_ir_opt_helpers.h"
#include "zcc_opt_metrics.h"
#include "zcc_ir_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Function *find_callee_function(Module *m, const char *name) {
    for (int i = 0; i < m->n_funcs; i++) {
        if (strcmp(m->funcs[i]->name, name) == 0) {
            return m->funcs[i];
        }
    }
    return NULL;
}

static bool is_reg_operand_inline(Opcode op, uint32_t src_idx) {
    if (op == OP_BR) return false;
    if (op == OP_CONDBR && src_idx > 0) return false;
    if (op == OP_CONST) return false;
    return true;
}

bool opt_inline_mvp_pass(Module *m, Function *fn, OptMetricsSink *metrics) {
    (void)metrics;
    bool changed = false;
    
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb || !bb->reachable) continue;
        
        for (Instr *it = bb->head; it; it = it->next) {
            if (it->op == OP_CALL) {
                // Reject recursive calls
                if (strcmp(it->call_name, fn->name) == 0) continue;

                Function *callee = find_callee_function(m, it->call_name);
                if (!callee) continue;

                uint32_t callee_instrs = fn_count_instructions(callee);
                if (callee_instrs > 50) continue;

                // M4-13: Single return block constraint
                uint32_t ret_block_count = 0;
                BlockID callee_ret_bb_id = NO_BLOCK;
                for (uint32_t cbi = 0; cbi < callee->n_blocks; cbi++) {
                    Block *cbb = callee->blocks[cbi];
                    if (cbb && cbb->tail && cbb->tail->op == OP_RET) {
                        ret_block_count++;
                        callee_ret_bb_id = cbi;
                    }
                }

                if (ret_block_count != 1) continue;

                printf("[Inline MVP] Inlining call to '%s' in function '%s'\n", it->call_name, fn->name);

                // Split caller block at callsite instruction
                Block *bb_after = calloc(1, sizeof(Block));
                bb_after->id = fn->n_blocks++;
                sprintf(bb_after->name, "bb%d_split", bb_after->id);
                bb_after->reachable = true;
                fn->blocks[bb_after->id] = bb_after;

                // Count instructions before callsite
                uint32_t insts_before = 0;
                for (Instr *tmp = bb->head; tmp && tmp != it; tmp = tmp->next) {
                    insts_before++;
                }

                bb_after->head = it->next;
                bb_after->tail = bb->tail;
                bb_after->n_instrs = bb->n_instrs - insts_before - 1;
                if (bb_after->head) {
                    bb_after->head->prev = NULL;
                }

                // Adjust bb (bb_before)
                bb->tail = it->prev;
                if (bb->tail) {
                    bb->tail->next = NULL;
                } else {
                    bb->head = NULL;
                }
                bb->n_instrs = insts_before;

                // Record original block and register counts
                uint32_t original_n_blocks = fn->n_blocks;

                // Setup CloneContext
                CloneContext ctx;
                clone_ctx_init(&ctx);

                // Rebuild def-use on callee to accurately identify its parameters
                rebuild_def_use(callee);

                // Map parameters to callsite arguments
                uint32_t param_idx = 0;
                for (int r = 1; r <= callee->n_regs; r++) {
                    if (callee->def_of[r] == NULL) {
                        if (param_idx < it->n_call_args) {
                            clone_ctx_map_local(&ctx, r, it->call_args[param_idx]);
                            param_idx++;
                        }
                    }
                }

                // Phase 1: Clone block skeletons
                Block **callee_clones = malloc(callee->n_blocks * sizeof(Block*));
                for (uint32_t cbi = 0; cbi < callee->n_blocks; cbi++) {
                    callee_clones[cbi] = clone_block_skeleton(fn, callee->blocks[cbi], &ctx);
                }

                BlockID entry_clone_id = callee_clones[0]->id;
                BlockID exit_clone_id = callee_clones[callee_ret_bb_id]->id;

                // Append branch to entry block clone in bb_before (bb)
                Instr *br_to_callee = calloc(1, sizeof(Instr));
                br_to_callee->op = OP_BR;
                br_to_callee->n_src = 1;
                br_to_callee->src[0] = entry_clone_id;
                
                if (!bb->head) {
                    bb->head = br_to_callee;
                    bb->tail = br_to_callee;
                } else {
                    bb->tail->next = br_to_callee;
                    br_to_callee->prev = bb->tail;
                    bb->tail = br_to_callee;
                }
                bb->n_instrs++;

                RegID cloned_ret_reg = 0;

                // Phase 2: Clone instructions
                for (uint32_t cbi = 0; cbi < callee->n_blocks; cbi++) {
                    Block *cbb = callee->blocks[cbi];
                    Block *cbb_clone = callee_clones[cbi];

                    for (Instr *c_inst = cbb->head; c_inst; c_inst = c_inst->next) {
                        if (c_inst->op == OP_RET) {
                            if (c_inst->n_src > 0) {
                                cloned_ret_reg = clone_ctx_get_local(&ctx, c_inst->src[0]);
                            }
                            
                            Instr *br_to_after = calloc(1, sizeof(Instr));
                            br_to_after->op = OP_BR;
                            br_to_after->n_src = 1;
                            br_to_after->src[0] = bb_after->id;
                            
                            if (!cbb_clone->head) {
                                cbb_clone->head = br_to_after;
                                cbb_clone->tail = br_to_after;
                            } else {
                                cbb_clone->tail->next = br_to_after;
                                br_to_after->prev = cbb_clone->tail;
                                cbb_clone->tail = br_to_after;
                            }
                            cbb_clone->n_instrs++;
                        } else {
                            Instr *cloned = clone_instruction(fn, c_inst, &ctx);
                            if (!cbb_clone->head) {
                                cbb_clone->head = cloned;
                                cbb_clone->tail = cloned;
                            } else {
                                cbb_clone->tail->next = cloned;
                                cloned->prev = cbb_clone->tail;
                                cbb_clone->tail = cloned;
                            }
                            cbb_clone->n_instrs++;
                        }
                    }
                }

                // If caller expected a return value, rewrite its uses to cloned_ret_reg
                if (it->dst != 0 && cloned_ret_reg != 0) {
                    for (uint32_t ob = 0; ob < original_n_blocks; ob++) {
                        Block *obb = fn->blocks[ob];
                        if (!obb) continue;

                        for (Instr *inst = obb->head; inst; inst = inst->next) {
                            if (inst->op == OP_PHI) continue;

                            for (uint32_t i = 0; i < inst->n_src; i++) {
                                if (is_reg_operand_inline(inst->op, i)) {
                                    if (inst->src[i] == it->dst) {
                                        inst->src[i] = cloned_ret_reg;
                                    }
                                }
                            }
                        }
                    }
                }

                // Redirect PHI nodes in successors of bb_after
                for (uint32_t b_idx = 0; b_idx < fn->n_blocks; b_idx++) {
                    Block *obb = fn->blocks[b_idx];
                    if (obb) {
                        phi_replace_incoming_pred(obb, bb->id, bb_after->id);
                    }
                }

                free(callee_clones);
                changed = true;
                break; // Process one call site per pass run for correctness/stability
            }
        }
        if (changed) break;
    }

    if (changed) {
        rebuild_preds_succs(fn);
        update_block_reachability(fn);
        rebuild_def_use(fn);
        run_ssa_verifier_assert(fn, "inline");
    }

    return changed;
}
