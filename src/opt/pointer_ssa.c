#include "pointer_ssa.h"
#include "zcc_ir.h"
#include <stdlib.h>

#define AMBIGUOUS 65537

uint32_t opt_pointer_ssa_rewrite_pass(Function *fn) {
    /* Allocate points_to and mem_points_to maps */
    RegID *points_to = calloc(MAX_INSTRS, sizeof(RegID));
    RegID *mem_points_to = calloc(MAX_INSTRS, sizeof(RegID));
    if (!points_to || !mem_points_to) {
        free(points_to);
        free(mem_points_to);
        return 0;
    }

    /* Initialize OP_ALLOCA base registers */
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->op == OP_ALLOCA && ins->dst < MAX_INSTRS) {
                points_to[ins->dst] = ins->dst;
            }
        }
    }

    /* Forward propagate points-to sets */
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *blk = fn->blocks[bi];
            if (!blk->reachable) continue;
            for (Instr *ins = blk->head; ins; ins = ins->next) {
                RegID target = 0;

                if (ins->op == OP_COPY) {
                    if (ins->src[0] < MAX_INSTRS) {
                        target = points_to[ins->src[0]];
                    }
                } else if (ins->op == OP_GEP) {
                    bool safe_gep = true;
                    if (ins->n_src > 1) {
                        RegID idx_reg = ins->src[1];
                        if (idx_reg < MAX_INSTRS) {
                            Instr *idx_def = fn->def_of[idx_reg];
                            if (idx_def && idx_def->op == OP_CONST) {
                                if (idx_def->imm != 0) {
                                    safe_gep = false;
                                }
                            } else {
                                safe_gep = false;
                            }
                        } else {
                            safe_gep = false;
                        }
                    }
                    if (safe_gep && ins->src[0] < MAX_INSTRS) {
                        target = points_to[ins->src[0]];
                    }
                } else if (ins->op == OP_PHI) {
                    target = 0;
                    for (uint32_t p = 0; p < ins->n_phi; p++) {
                        RegID phi_reg = ins->phi[p].reg;
                        if (phi_reg < MAX_INSTRS) {
                            RegID src_target = points_to[phi_reg];
                            if (target == 0) {
                                target = src_target;
                            } else if (src_target != 0 && target != src_target) {
                                target = AMBIGUOUS;
                            }
                        }
                    }
                } else if (ins->op == OP_LOAD && ins->n_src >= 1) {
                    RegID ptr_reg = ins->src[0];
                    if (ptr_reg < MAX_INSTRS) {
                        RegID base = points_to[ptr_reg];
                        if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS) {
                            target = mem_points_to[base];
                        }
                    }
                } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                    RegID ptr_reg = ins->src[1];
                    RegID val_reg = ins->src[0];
                    if (ptr_reg < MAX_INSTRS && val_reg < MAX_INSTRS) {
                        RegID base = points_to[ptr_reg];
                        if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS) {
                            RegID stored_val = points_to[val_reg];
                            if (stored_val != 0) {
                                if (mem_points_to[base] == 0) {
                                    mem_points_to[base] = stored_val;
                                    changed = true;
                                } else if (mem_points_to[base] != stored_val && mem_points_to[base] != AMBIGUOUS) {
                                    mem_points_to[base] = AMBIGUOUS;
                                    changed = true;
                                }
                            }
                        }
                    }
                }

                if (ins->dst && ins->dst < MAX_INSTRS && target != 0 && points_to[ins->dst] != target) {
                    points_to[ins->dst] = target;
                    changed = true;
                }
            }
        }
    }

    /* Compute escaped allocas */
    bool *escaped = calloc(MAX_INSTRS, sizeof(bool));
    if (escaped) {
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *blk = fn->blocks[bi];
            if (!blk->reachable) continue;
            for (Instr *ins = blk->head; ins; ins = ins->next) {
                if (ins->op == OP_CALL) {
                    for (uint32_t s = 0; s < ins->n_src; s++) {
                        RegID arg = ins->src[s];
                        if (arg < MAX_INSTRS) {
                            RegID base = points_to[arg];
                            if (base != 0 && base < MAX_INSTRS) {
                                escaped[base] = true;
                            }
                        }
                    }
                } else if (ins->op == OP_RET && ins->n_src >= 1) {
                    RegID ret_val = ins->src[0];
                    if (ret_val < MAX_INSTRS) {
                        RegID base = points_to[ret_val];
                        if (base != 0 && base < MAX_INSTRS) {
                            escaped[base] = true;
                        }
                    }
                } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                    RegID val_reg = ins->src[0];
                    if (val_reg < MAX_INSTRS) {
                        RegID val_base = points_to[val_reg];
                        if (val_base != 0 && val_base < MAX_INSTRS) {
                            escaped[val_base] = true;
                        }
                    }
                }
            }
        }
    }
    /* Rewrite indirect load/store instructions */
    uint32_t rewrites = 0;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->op == OP_LOAD && ins->n_src >= 1) {
                RegID ptr_reg = ins->src[0];
                if (ptr_reg < MAX_INSTRS) {
                    RegID base = points_to[ptr_reg];
                    if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && (!escaped || !escaped[base])) {
                        ins->src[0] = base;
                        rewrites++;
                    }
                }
            } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                RegID ptr_reg = ins->src[1];
                if (ptr_reg < MAX_INSTRS) {
                    RegID base = points_to[ptr_reg];
                    if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && (!escaped || !escaped[base])) {
                        ins->src[1] = base;
                        rewrites++;
                    }
                }
            }
        }
    }

    free(escaped);
    free(points_to);
    free(mem_points_to);
    return rewrites;
}
