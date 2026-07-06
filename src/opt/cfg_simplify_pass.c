#include "zcc_ir.h"
#include "zcc_ir_opt_helpers.h"
#include "zcc_ir_opt_passes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool opt_cfg_simplify_pass(Function *fn, void *metrics) {
    (void)metrics;
    bool changed = false;

    // 1. Compute reachability from entry block (0)
    bool *reachable = calloc(fn->n_blocks, sizeof(bool));
    BlockID *queue = malloc(fn->n_blocks * sizeof(BlockID));
    int head = 0, tail = 0;

    queue[tail++] = 0;
    reachable[0] = true;

    while (head < tail) {
        BlockID curr = queue[head++];
        Block *bb = fn->blocks[curr];
        if (!bb) continue;

        for (uint32_t si = 0; si < bb->n_succs; si++) {
            BlockID succ = bb->succs[si];
            if (succ < fn->n_blocks && fn->blocks[succ] && !reachable[succ]) {
                reachable[succ] = true;
                queue[tail++] = succ;
            }
        }
    }

    // 2. Remove unreachable blocks
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        if (!reachable[bi]) {
            // Free instructions
            Instr *it = bb->head;
            while (it) {
                Instr *next = it->next;
                free(it);
                it = next;
            }
            free(bb);
            fn->blocks[bi] = NULL;
            changed = true;
        }
    }

    // 3. Rebuild predecessor/successor lists for remaining blocks
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        bb->n_succs = 0;
        bb->n_preds = 0;
    }

    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        Instr *t = bb->tail;
        if (!t) continue;

        if (t->op == OP_BR) {
            BlockID dest = t->src[0];
            if (dest < fn->n_blocks && fn->blocks[dest]) {
                bb->succs[bb->n_succs++] = dest;
                Block *dest_bb = fn->blocks[dest];
                dest_bb->preds[dest_bb->n_preds++] = bi;
            }
        } else if (t->op == OP_CONDBR) {
            BlockID dest1 = t->src[1];
            BlockID dest2 = t->src[2];
            if (dest1 < fn->n_blocks && fn->blocks[dest1]) {
                bb->succs[bb->n_succs++] = dest1;
                Block *dest_bb1 = fn->blocks[dest1];
                dest_bb1->preds[dest_bb1->n_preds++] = bi;
            }
            if (dest2 < fn->n_blocks && fn->blocks[dest2]) {
                bb->succs[bb->n_succs++] = dest2;
                Block *dest_bb2 = fn->blocks[dest2];
                dest_bb2->preds[dest_bb2->n_preds++] = bi;
            }
        }
    }

    // 4. Update PHI nodes in remaining blocks
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        Instr *it = bb->head;
        while (it) {
            Instr *next = it->next;
            if (it->op == OP_PHI) {
                // Filter out incoming edges from removed/unreachable blocks
                uint32_t new_n_phi = 0;
                PhiSource new_phi[MAX_PHI_SOURCES];
                for (uint32_t i = 0; i < it->n_phi; i++) {
                    BlockID pred_id = it->phi[i].block;
                    if (pred_id < fn->n_blocks && fn->blocks[pred_id]) {
                        new_phi[new_n_phi++] = it->phi[i];
                    }
                }
                
                it->n_phi = new_n_phi;
                memcpy(it->phi, new_phi, new_n_phi * sizeof(PhiSource));

                // If only 1 incoming block remains, replace uses and erase PHI
                if (it->n_phi == 1) {
                    replace_all_uses(fn, it->dst, it->phi[0].reg);
                    erase_instr(fn, it);
                    changed = true;
                } else if (it->n_phi == 0) {
                    erase_instr(fn, it);
                    changed = true;
                }
            }
            it = next;
        }
    }

    free(reachable);
    free(queue);

    if (changed) {
        rebuild_def_use(fn);
    }
    return changed;
}
