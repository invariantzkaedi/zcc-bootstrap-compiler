#include "zcc_ir.h"
#include "zcc_ir_opt_helpers.h"
#include "zcc_ir_opt_passes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool g_reg_is_param[MAX_INSTRS];

typedef enum {
    LATTICE_TOP,
    LATTICE_CONSTANT,
    LATTICE_BOTTOM
} LatticeState;

typedef struct {
    LatticeState state;
    int64_t val;
} SccpValue;

bool opt_sccp_pass(Function *fn, void *metrics) {
    (void)metrics;
    bool changed = false;

    // Allocate lattice value table for all registers
    SccpValue *lat = calloc(MAX_INSTRS, sizeof(SccpValue));
    // Allocate reachability table for all blocks
    bool *bb_reach = calloc(fn->n_blocks, sizeof(bool));

    // Entry block is reachable
    bb_reach[0] = true;

    // Mark parameter registers as BOTTOM (overdefined)
    for (int i = 1; i < MAX_INSTRS; i++) {
        if (g_reg_is_param[i]) {
            lat[i].state = LATTICE_BOTTOM;
        } else {
            lat[i].state = LATTICE_TOP;
        }
    }

    // Iterative fixpoint algorithm
    bool iter_changed = true;
    int max_iters = 1000;
    int iters = 0;

    while (iter_changed && iters++ < max_iters) {
        iter_changed = false;

        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *bb = fn->blocks[bi];
            if (!bb || !bb_reach[bi]) continue;

            for (Instr *it = bb->head; it; it = it->next) {
                SccpValue old_val = (it->dst != 0 && it->dst < MAX_INSTRS) ? lat[it->dst] : (SccpValue){0};

                if (it->op == OP_CONST) {
                    lat[it->dst].state = LATTICE_CONSTANT;
                    lat[it->dst].val = it->imm;
                } else if (it->op == OP_COPY) {
                    RegID src = it->src[0];
                    if (src < MAX_INSTRS) {
                        lat[it->dst] = lat[src];
                    }
                } else if (it->op == OP_PHI) {
                    SccpValue meet = { .state = LATTICE_TOP, .val = 0 };
                    for (uint32_t i = 0; i < it->n_phi; i++) {
                        BlockID pred_id = it->phi[i].block;
                        if (pred_id < fn->n_blocks && fn->blocks[pred_id] && bb_reach[pred_id]) {
                            RegID phi_reg = it->phi[i].reg;
                            if (phi_reg < MAX_INSTRS) {
                                SccpValue incoming = lat[phi_reg];
                                if (incoming.state == LATTICE_BOTTOM) {
                                    meet.state = LATTICE_BOTTOM;
                                } else if (incoming.state == LATTICE_CONSTANT) {
                                    if (meet.state == LATTICE_TOP) {
                                        meet = incoming;
                                    } else if (meet.state == LATTICE_CONSTANT) {
                                        if (meet.val != incoming.val) {
                                            meet.state = LATTICE_BOTTOM;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    lat[it->dst] = meet;
                } else if (it->n_src == 2 && (it->op == OP_ADD || it->op == OP_SUB || it->op == OP_MUL || it->op == OP_DIV || it->op == OP_MOD || it->op == OP_BAND || it->op == OP_BOR || it->op == OP_BXOR || it->op == OP_SHL || it->op == OP_SHR || it->op == OP_EQ || it->op == OP_NE || it->op == OP_LT || it->op == OP_LE || it->op == OP_GT || it->op == OP_GE)) {
                    RegID s0 = it->src[0];
                    RegID s1 = it->src[1];
                    if (s0 < MAX_INSTRS && s1 < MAX_INSTRS) {
                        SccpValue v0 = lat[s0];
                        SccpValue v1 = lat[s1];
                        if (s0 == s1 && (it->op == OP_SUB || it->op == OP_BXOR || it->op == OP_EQ || it->op == OP_NE || it->op == OP_LE || it->op == OP_GE)) {
                            lat[it->dst].state = LATTICE_CONSTANT;
                            if (it->op == OP_SUB || it->op == OP_BXOR || it->op == OP_NE) {
                                lat[it->dst].val = 0;
                            } else {
                                lat[it->dst].val = 1;
                            }
                        } else if (v0.state == LATTICE_BOTTOM || v1.state == LATTICE_BOTTOM) {
                            lat[it->dst].state = LATTICE_BOTTOM;
                        } else if (v0.state == LATTICE_CONSTANT && v1.state == LATTICE_CONSTANT) {
                            lat[it->dst].state = LATTICE_CONSTANT;
                            int64_t a = v0.val;
                            int64_t b = v1.val;
                            int64_t res = 0;
                            switch (it->op) {
                                case OP_ADD: res = a + b; break;
                                case OP_SUB: res = a - b; break;
                                case OP_MUL: res = a * b; break;
                                case OP_DIV: res = (b == 0) ? 0 : a / b; break;
                                case OP_MOD: res = (b == 0) ? 0 : a % b; break;
                                case OP_BAND: res = a & b; break;
                                case OP_BOR: res = a | b; break;
                                case OP_BXOR: res = a ^ b; break;
                                case OP_SHL: res = a << b; break;
                                case OP_SHR: res = a >> b; break;
                                case OP_EQ: res = (a == b); break;
                                case OP_NE: res = (a != b); break;
                                case OP_LT: res = (a < b); break;
                                case OP_LE: res = (a <= b); break;
                                case OP_GT: res = (a > b); break;
                                case OP_GE: res = (a >= b); break;
                                default: break;
                            }
                            lat[it->dst].val = res;
                        } else {
                            lat[it->dst].state = LATTICE_TOP;
                        }
                    }
                } else if (it->op == OP_CONDBR) {
                    RegID cond = it->src[0];
                    if (cond < MAX_INSTRS) {
                        SccpValue cv = lat[cond];
                        if (cv.state == LATTICE_CONSTANT) {
                            if (cv.val != 0) {
                                if (!bb_reach[it->src[1]]) {
                                    bb_reach[it->src[1]] = true;
                                    iter_changed = true;
                                }
                            } else {
                                if (!bb_reach[it->src[2]]) {
                                    bb_reach[it->src[2]] = true;
                                    iter_changed = true;
                                }
                            }
                        } else if (cv.state == LATTICE_BOTTOM) {
                            if (!bb_reach[it->src[1]]) {
                                bb_reach[it->src[1]] = true;
                                iter_changed = true;
                            }
                            if (!bb_reach[it->src[2]]) {
                                bb_reach[it->src[2]] = true;
                                iter_changed = true;
                            }
                        }
                    }
                } else if (it->op == OP_BR) {
                    BlockID dest = it->src[0];
                    if (dest < fn->n_blocks && !bb_reach[dest]) {
                        bb_reach[dest] = true;
                        iter_changed = true;
                    }
                }

                if (it->dst != 0 && it->dst < MAX_INSTRS) {
                    if (lat[it->dst].state != old_val.state || lat[it->dst].val != old_val.val) {
                        iter_changed = true;
                    }
                }
            }
        }
    }

    // Rewrite constant instructions and conditional branches
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb || !bb_reach[bi]) continue;

        for (Instr *it = bb->head; it; it = it->next) {
            if (it->dst != 0 && it->dst < MAX_INSTRS) {
                if (lat[it->dst].state == LATTICE_CONSTANT && it->op != OP_CONST && it->op != OP_PHI) {
                    it->op = OP_CONST;
                    it->imm = lat[it->dst].val;
                    it->n_src = 0;
                    changed = true;
                }
            }

            if (it->op == OP_CONDBR) {
                RegID cond = it->src[0];
                if (cond < MAX_INSTRS && lat[cond].state == LATTICE_CONSTANT) {
                    it->op = OP_BR;
                    if (lat[cond].val != 0) {
                        it->src[0] = it->src[1]; // target is then_blk
                    } else {
                        it->src[0] = it->src[2]; // target is else_blk
                    }
                    it->n_src = 1;
                    changed = true;
                }
            }
        }
    }

    free(lat);
    free(bb_reach);

    if (changed) {
        rebuild_def_use(fn);
    }

    // Call cfg simplify at the end to clean up unreachable blocks
    if (opt_cfg_simplify_pass(fn, NULL)) {
        changed = true;
    }

    return changed;
}
