#include "loop_validator.h"
#include "zcc_ir_opt_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Opcode reverse_cmp_op(Opcode op) {
    switch (op) {
        case OP_LT: return OP_GT;
        case OP_LE: return OP_GE;
        case OP_GT: return OP_LT;
        case OP_GE: return OP_LE;
        default: return op;
    }
}

static bool can_reach_without(Function *fn, BlockID start, BlockID target, BlockID avoid) {
    if (start == target) return true;
    if (start == avoid) return false;

    bool *visited = calloc(fn->n_blocks, sizeof(bool));
    BlockID *queue = malloc(fn->n_blocks * sizeof(BlockID));
    int head = 0, tail = 0;

    queue[tail++] = start;
    visited[start] = true;

    bool reached = false;
    while (head < tail) {
        BlockID curr = queue[head++];
        if (curr == target) {
            reached = true;
            break;
        }

        Block *bb = fn->blocks[curr];
        if (!bb) continue;

        for (uint32_t i = 0; i < bb->n_succs; i++) {
            BlockID succ = bb->succs[i];
            if (succ == avoid) continue;
            if (succ < fn->n_blocks && !visited[succ]) {
                visited[succ] = true;
                queue[tail++] = succ;
            }
        }
    }

    free(visited);
    free(queue);
    return reached;
}

bool opt_detect_canonical_loop(Function *fn, BlockID header_id, LoopCanonicalInfo *info) {
    memset(info, 0, sizeof(LoopCanonicalInfo));
    info->trip_count = -1;

    if (header_id >= fn->n_blocks) return false;
    Block *header = fn->blocks[header_id];
    if (!header || !header->reachable) return false;

    if (header->n_preds != 2) {
        return false;
    }

    BlockID pred0 = header->preds[0];
    BlockID pred1 = header->preds[1];
    Block *p0 = fn->blocks[pred0];
    Block *p1 = fn->blocks[pred1];
    if (!p0 || !p1) return false;

    BlockID preheader_id = NO_BLOCK;
    BlockID latch_id = NO_BLOCK;

    bool p0_is_latch = can_reach_without(fn, header_id, pred0, pred1);
    bool p1_is_latch = can_reach_without(fn, header_id, pred1, pred0);

    if (p0_is_latch && !p1_is_latch) {
        latch_id = pred0;
        preheader_id = pred1;
    } else if (p1_is_latch && !p0_is_latch) {
        latch_id = pred1;
        preheader_id = pred0;
    } else {
        return false;
    }

    info->preheader = preheader_id;
    info->header = header_id;
    info->latch = latch_id;

    Block *latch = fn->blocks[latch_id];

    BlockID exit_id = NO_BLOCK;
    RegID cond_reg = 0;

    if (header->tail && header->tail->op == OP_CONDBR) {
        cond_reg = header->tail->src[0];
        BlockID dest1 = header->tail->src[1];
        BlockID dest2 = header->tail->src[2];

        if (dest1 == latch_id) {
            exit_id = dest2;
        } else if (dest2 == latch_id) {
            exit_id = dest1;
        } else {
            if (can_reach_without(fn, dest1, latch_id, header_id)) {
                exit_id = dest2;
            } else {
                exit_id = dest1;
            }
        }
    }
    else if (latch->tail && latch->tail->op == OP_CONDBR) {
        cond_reg = latch->tail->src[0];
        BlockID dest1 = latch->tail->src[1];
        BlockID dest2 = latch->tail->src[2];
        if (dest1 == header_id) {
            exit_id = dest2;
        } else if (dest2 == header_id) {
            exit_id = dest1;
        }
    }

    if (exit_id == NO_BLOCK || cond_reg == 0) {
        return false;
    }
    info->exit = exit_id;
    info->cmp_reg = cond_reg;

    RegID ind_var = 0;
    int64_t step_val = 0;
    Opcode step_op = OP_NOP;
    RegID val_pre = 0;
    RegID val_latch = 0;

    for (Instr *ins = header->head; ins && ins->op == OP_PHI; ins = ins->next) {
        val_pre = 0;
        val_latch = 0;
        for (uint32_t i = 0; i < ins->n_phi; i++) {
            if (ins->phi[i].block == preheader_id) {
                val_pre = ins->phi[i].reg;
            } else if (ins->phi[i].block == latch_id) {
                val_latch = ins->phi[i].reg;
            }
        }

        if (val_pre && val_latch) {
            RegID resolved_latch = val_latch;
            Instr *def = def_of(fn, resolved_latch);
            while (def && def->op == OP_COPY) {
                resolved_latch = def->src[0];
                def = def_of(fn, resolved_latch);
            }
            if (def && (def->op == OP_ADD || def->op == OP_SUB)) {
                RegID op1 = def->src[0];
                RegID op2 = def->src[1];
                bool step_is_const = false;

                if (op1 == ins->dst) {
                    step_is_const = reg_is_const(fn, op2, &step_val);
                } else if (op2 == ins->dst && def->op == OP_ADD) {
                    step_is_const = reg_is_const(fn, op1, &step_val);
                }

                if (step_is_const) {
                    ind_var = ins->dst;
                    step_op = def->op;
                    break;
                }
            }
        }
    }

    if (ind_var == 0) {
        return false;
    }

    info->ind_var = ind_var;
    info->step = step_val;
    info->step_op = step_op;

    Instr *cmp = def_of(fn, cond_reg);
    if (!cmp) {
        return false;
    }

    Opcode cmp_op = cmp->op;
    if (cmp_op != OP_EQ && cmp_op != OP_NE && cmp_op != OP_LT && cmp_op != OP_LE && cmp_op != OP_GT && cmp_op != OP_GE) {
        return false;
    }

    RegID cmp_src1 = cmp->src[0];
    RegID cmp_src2 = cmp->src[1];
    int64_t limit_val = 0;
    bool limit_is_const = false;

    if (cmp_src1 == ind_var) {
        limit_is_const = reg_is_const(fn, cmp_src2, &limit_val);
        info->cmp_op = cmp_op;
    } else if (cmp_src2 == ind_var) {
        limit_is_const = reg_is_const(fn, cmp_src1, &limit_val);
        info->cmp_op = reverse_cmp_op(cmp_op);
    } else {
        int resolved1 = resolve_copy(fn, cmp_src1);
        int resolved2 = resolve_copy(fn, cmp_src2);
        if (resolved1 == ind_var) {
            limit_is_const = reg_is_const(fn, cmp_src2, &limit_val);
            info->cmp_op = cmp_op;
        } else if (resolved2 == ind_var) {
            limit_is_const = reg_is_const(fn, cmp_src1, &limit_val);
            info->cmp_op = reverse_cmp_op(cmp_op);
        } else {
            return false;
        }
    }

    info->limit = limit_val;
    info->limit_is_const = limit_is_const;

    int64_t init_val = 0;
    bool init_is_const = reg_is_const(fn, val_pre, &init_val);

    if (init_is_const && limit_is_const) {
        int64_t step = info->step;
        if (step > 0) {
            if (info->step_op == OP_ADD) {
                if (info->cmp_op == OP_LT) {
                    if (init_val >= limit_val) info->trip_count = 0;
                    else info->trip_count = (limit_val - init_val + step - 1) / step;
                } else if (info->cmp_op == OP_LE) {
                    if (init_val > limit_val) info->trip_count = 0;
                    else info->trip_count = (limit_val - init_val) / step + 1;
                } else if (info->cmp_op == OP_NE) {
                    if (init_val <= limit_val && (limit_val - init_val) % step == 0) {
                        info->trip_count = (limit_val - init_val) / step;
                    }
                }
            } else if (info->step_op == OP_SUB) {
                if (info->cmp_op == OP_GT) {
                    if (init_val <= limit_val) info->trip_count = 0;
                    else info->trip_count = (init_val - limit_val + step - 1) / step;
                } else if (info->cmp_op == OP_GE) {
                    if (init_val < limit_val) info->trip_count = 0;
                    else info->trip_count = (init_val - limit_val) / step + 1;
                } else if (info->cmp_op == OP_NE) {
                    if (init_val >= limit_val && (init_val - limit_val) % step == 0) {
                        info->trip_count = (init_val - limit_val) / step;
                    }
                }
            }
        }
    }

    return true;
}
