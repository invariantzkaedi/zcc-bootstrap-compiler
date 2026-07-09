#include "pointer_ssa.h"
#include "zcc_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define AMBIGUOUS 65537

typedef struct {
    RegID base;
    int64_t offset;
} BaseOffsetKey;

/*
 * Retrieve or allocate a tracking slot for the memory location (base, offset).
 * If the 1024 capacity limit is reached, it returns -1. In this case, points-to
 * propagation for values loaded from/stored to this location degrades conservatively
 * (resulting in no points-to targets for those loads, skipping rewrites but maintaining safety).
 */
static int get_or_create_mem_location(RegID base, int64_t offset, BaseOffsetKey *locations, int *n_locations) {
    for (int i = 0; i < *n_locations; i++) {
        if (locations[i].base == base && locations[i].offset == offset) {
            return i;
        }
    }
    if (*n_locations >= 1024) {
        return -1;
    }
    int id = (*n_locations)++;
    locations[id].base = base;
    locations[id].offset = offset;
    return id;
}

uint32_t opt_pointer_ssa_rewrite_pass(Function *fn) {
    /* We choose Option (b): extend OP_LOAD/OP_STORE with address-mode folding displacement (amf.disp).
     * This avoids inserting new instructions and is fully handled during assembly generation. */

    RegID *points_to_base = calloc(MAX_INSTRS, sizeof(RegID));
    int64_t *points_to_offset = calloc(MAX_INSTRS, sizeof(int64_t));

    BaseOffsetKey *mem_locations = calloc(1024, sizeof(BaseOffsetKey));
    int n_mem_locations = 0;
    RegID *mem_points_to_base = calloc(1024, sizeof(RegID));
    int64_t *mem_points_to_offset = calloc(1024, sizeof(int64_t));

    if (!points_to_base || !points_to_offset || !mem_locations || !mem_points_to_base || !mem_points_to_offset) {
        free(points_to_base);
        free(points_to_offset);
        free(mem_locations);
        free(mem_points_to_base);
        free(mem_points_to_offset);
        return 0;
    }

    /* Initialize OP_ALLOCA base registers */
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->op == OP_ALLOCA && ins->dst < MAX_INSTRS) {
                points_to_base[ins->dst] = ins->dst;
                points_to_offset[ins->dst] = 0;
            }
        }
    }

    /* Forward propagate points-to sets with constant offsets */
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *blk = fn->blocks[bi];
            if (!blk->reachable) continue;
            for (Instr *ins = blk->head; ins; ins = ins->next) {
                RegID target_base = 0;
                int64_t target_offset = 0;

                if (ins->op == OP_COPY) {
                    if (ins->src[0] < MAX_INSTRS) {
                        target_base = points_to_base[ins->src[0]];
                        target_offset = points_to_offset[ins->src[0]];
                    }
                } else if (ins->op == OP_GEP) {
                    if (ins->src[0] < MAX_INSTRS) {
                        RegID base = points_to_base[ins->src[0]];
                        int64_t offset = points_to_offset[ins->src[0]];
                        if (base != 0 && base != AMBIGUOUS) {
                            if (ins->n_src > 1) {
                                RegID idx_reg = ins->src[1];
                                if (idx_reg < MAX_INSTRS) {
                                    Instr *idx_def = fn->def_of[idx_reg];
                                    if (idx_def && idx_def->op == OP_CONST) {
                                        // Reject GEP offset accumulation on signed overflow
                                        if ((idx_def->imm > 0 && offset > INT64_MAX - idx_def->imm) ||
                                            (idx_def->imm < 0 && offset < INT64_MIN - idx_def->imm)) {
                                            target_base = AMBIGUOUS;
                                        } else {
                                            int64_t next_offset = offset + idx_def->imm;
                                            // Reject if offset is negative or exceeds alloca size boundary
                                            bool bounds_ok = true;
                                            if (base < MAX_INSTRS) {
                                                Instr *alloca_ins = fn->def_of[base];
                                                if (alloca_ins && alloca_ins->op == OP_ALLOCA) {
                                                    int64_t alloca_size = alloca_ins->imm;
                                                    if (next_offset < 0 || next_offset >= alloca_size) {
                                                        bounds_ok = false;
                                                    }
                                                }
                                            }
                                            if (bounds_ok) {
                                                target_base = base;
                                                target_offset = next_offset;
                                            } else {
                                                target_base = AMBIGUOUS;
                                            }
                                        }
                                    } else {
                                        target_base = AMBIGUOUS;
                                    }
                                } else {
                                    target_base = AMBIGUOUS;
                                }
                            } else {
                                target_base = base;
                                target_offset = offset;
                            }
                        }
                    }
                } else if (ins->op == OP_ADD) {
                    if (ins->src[0] < MAX_INSTRS && ins->n_src > 1 && ins->src[1] < MAX_INSTRS) {
                        RegID r0 = ins->src[0];
                        RegID r1 = ins->src[1];
                        RegID base0 = points_to_base[r0];
                        RegID base1 = points_to_base[r1];
                        if (base0 != 0 && base1 != 0) {
                            target_base = AMBIGUOUS;
                        } else if (base0 != 0 && base0 != AMBIGUOUS) {
                            Instr *def1 = fn->def_of[r1];
                            if (def1 && def1->op == OP_CONST) {
                                int64_t offset = points_to_offset[r0];
                                int64_t imm = def1->imm;
                                if ((imm > 0 && offset > INT64_MAX - imm) ||
                                    (imm < 0 && offset < INT64_MIN - imm)) {
                                    target_base = AMBIGUOUS;
                                } else {
                                    int64_t next_offset = offset + imm;
                                    bool bounds_ok = true;
                                    if (base0 < MAX_INSTRS) {
                                        Instr *alloca_ins = fn->def_of[base0];
                                        if (alloca_ins && alloca_ins->op == OP_ALLOCA) {
                                            int64_t alloca_size = alloca_ins->imm;
                                            if (next_offset < 0 || next_offset >= alloca_size) {
                                                bounds_ok = false;
                                            }
                                        }
                                    }
                                    if (bounds_ok) {
                                        target_base = base0;
                                        target_offset = next_offset;
                                    } else {
                                        target_base = AMBIGUOUS;
                                    }
                                }
                            }
                        } else if (base1 != 0 && base1 != AMBIGUOUS) {
                            Instr *def0 = fn->def_of[r0];
                            if (def0 && def0->op == OP_CONST) {
                                int64_t offset = points_to_offset[r1];
                                int64_t imm = def0->imm;
                                if ((imm > 0 && offset > INT64_MAX - imm) ||
                                    (imm < 0 && offset < INT64_MIN - imm)) {
                                    target_base = AMBIGUOUS;
                                } else {
                                    int64_t next_offset = offset + imm;
                                    bool bounds_ok = true;
                                    if (base1 < MAX_INSTRS) {
                                        Instr *alloca_ins = fn->def_of[base1];
                                        if (alloca_ins && alloca_ins->op == OP_ALLOCA) {
                                            int64_t alloca_size = alloca_ins->imm;
                                            if (next_offset < 0 || next_offset >= alloca_size) {
                                                bounds_ok = false;
                                            }
                                        }
                                    }
                                    if (bounds_ok) {
                                        target_base = base1;
                                        target_offset = next_offset;
                                    } else {
                                        target_base = AMBIGUOUS;
                                    }
                                }
                            }
                        }
                    }
                } else if (ins->op == OP_PHI) {
                    target_base = 0;
                    target_offset = 0;
                    for (uint32_t p = 0; p < ins->n_phi; p++) {
                        RegID phi_reg = ins->phi[p].reg;
                        if (phi_reg < MAX_INSTRS) {
                            RegID src_base = points_to_base[phi_reg];
                            int64_t src_offset = points_to_offset[phi_reg];
                            if (src_base != 0) {
                                if (target_base == 0) {
                                    target_base = src_base;
                                    target_offset = src_offset;
                                } else if (target_base != AMBIGUOUS) {
                                    if (src_base != target_base || src_offset != target_offset) {
                                        target_base = AMBIGUOUS;
                                    }
                                }
                            }
                        }
                    }
                } else if (ins->op == OP_LOAD && ins->n_src >= 1) {
                    RegID ptr_reg = ins->src[0];
                    if (ptr_reg < MAX_INSTRS) {
                        RegID base = points_to_base[ptr_reg];
                        int64_t offset = points_to_offset[ptr_reg];
                        if (base != 0 && base != AMBIGUOUS) {
                            int loc_id = get_or_create_mem_location(base, offset, mem_locations, &n_mem_locations);
                            if (loc_id >= 0) {
                                target_base = mem_points_to_base[loc_id];
                                target_offset = mem_points_to_offset[loc_id];
                            }
                        }
                    }
                } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                    RegID ptr_reg = ins->src[1];
                    RegID val_reg = ins->src[0];
                    if (ptr_reg < MAX_INSTRS && val_reg < MAX_INSTRS) {
                        RegID base = points_to_base[ptr_reg];
                        int64_t offset = points_to_offset[ptr_reg];
                        if (base != 0 && base != AMBIGUOUS) {
                            RegID stored_base = points_to_base[val_reg];
                            int64_t stored_offset = points_to_offset[val_reg];
                            if (stored_base != 0) {
                                int loc_id = get_or_create_mem_location(base, offset, mem_locations, &n_mem_locations);
                                if (loc_id >= 0) {
                                    if (mem_points_to_base[loc_id] == 0) {
                                        mem_points_to_base[loc_id] = stored_base;
                                        mem_points_to_offset[loc_id] = stored_offset;
                                        changed = true;
                                    } else if (mem_points_to_base[loc_id] != AMBIGUOUS) {
                                        if (mem_points_to_base[loc_id] != stored_base || mem_points_to_offset[loc_id] != stored_offset) {
                                            mem_points_to_base[loc_id] = AMBIGUOUS;
                                            changed = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (ins->dst && ins->dst < MAX_INSTRS && target_base != 0) {
                    if (points_to_base[ins->dst] != target_base || points_to_offset[ins->dst] != target_offset) {
                        points_to_base[ins->dst] = target_base;
                        points_to_offset[ins->dst] = target_offset;
                        changed = true;
                    }
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
                            RegID base = points_to_base[arg];
                            if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS) {
                                escaped[base] = true;
                            }
                        }
                    }
                } else if (ins->op == OP_RET && ins->n_src >= 1) {
                    RegID ret_val = ins->src[0];
                    if (ret_val < MAX_INSTRS) {
                        RegID base = points_to_base[ret_val];
                        if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS) {
                            escaped[base] = true;
                        }
                    }
                } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                    RegID val_reg = ins->src[0];
                    if (val_reg < MAX_INSTRS) {
                        RegID val_base = points_to_base[val_reg];
                        if (val_base != 0 && val_base != AMBIGUOUS && val_base < MAX_INSTRS) {
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
                    RegID base = points_to_base[ptr_reg];
                    int64_t offset = points_to_offset[ptr_reg];
                    if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && ptr_reg != base && (!escaped || !escaped[base])) {
                        ins->src[0] = base;
                        ins->amf.folded = true;
                        ins->amf.base = base;
                        ins->amf.disp = offset;
                        rewrites++;
                    }
                }
            } else if (ins->op == OP_STORE && ins->n_src >= 2) {
                RegID ptr_reg = ins->src[1];
                if (ptr_reg < MAX_INSTRS) {
                    RegID base = points_to_base[ptr_reg];
                    int64_t offset = points_to_offset[ptr_reg];
                    if (base != 0 && base != AMBIGUOUS && base < MAX_INSTRS && ptr_reg != base && (!escaped || !escaped[base])) {
                        ins->src[1] = base;
                        ins->amf.folded = true;
                        ins->amf.base = base;
                        ins->amf.disp = offset;
                        rewrites++;
                    }
                }
            }
        }
    }

    free(escaped);
    free(points_to_base);
    free(points_to_offset);
    free(mem_locations);
    free(mem_points_to_base);
    free(mem_points_to_offset);
    return rewrites;
}
