#include "loop_validator.h"
#include "clone_remap.h"
#include "zcc_ir_opt_helpers.h"
#include "zcc_opt_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void append_instruction(Block *bb, Instr *inst) {
    if (!bb->head) {
        bb->head = inst;
        bb->tail = inst;
        inst->prev = NULL;
        inst->next = NULL;
    } else {
        bb->tail->next = inst;
        inst->prev = bb->tail;
        inst->next = NULL;
        bb->tail = inst;
    }
    bb->n_instrs++;
}

static bool is_reg_operand_unroll(Opcode op, uint32_t src_idx) {
    if (op == OP_BR) return false;
    if (op == OP_CONDBR && src_idx > 0) return false;
    if (op == OP_CONST) return false;
    return true;
}

bool opt_loop_unroll_mvp_pass(Function *fn, OptMetricsSink *metrics) {
    (void)metrics;
    bool changed = false;
    
    rebuild_def_use(fn);
    
    // Scan blocks for loop headers
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb || !bb->reachable) continue;
        
        LoopCanonicalInfo info;
        if (opt_detect_canonical_loop(fn, bi, &info)) {
            // M4-09: Eligibility gate
            if (info.trip_count >= 0 && info.trip_count <= 8) {
                uint32_t body_instrs = bb->n_instrs;
                if (info.latch != info.header) {
                    Block *latch = fn->blocks[info.latch];
                    if (latch) body_instrs += latch->n_instrs;
                }
                
                if (body_instrs > 40) continue;
                
                bool strict_single_loop = true;
                Block *h_blk = fn->blocks[info.header];
                Block *l_blk = fn->blocks[info.latch];
                if (h_blk && l_blk) {
                    for (uint32_t s = 0; s < h_blk->n_succs; s++) {
                        if (h_blk->succs[s] != info.latch && h_blk->succs[s] != info.exit) {
                            strict_single_loop = false;
                        }
                    }
                    if (info.header != info.latch) {
                        for (uint32_t s = 0; s < l_blk->n_succs; s++) {
                            if (l_blk->succs[s] != info.header) {
                                strict_single_loop = false;
                            }
                        }
                        for (uint32_t p = 0; p < l_blk->n_preds; p++) {
                            if (l_blk->preds[p] != info.header) {
                                strict_single_loop = false;
                            }
                        }
                    }
                } else {
                    strict_single_loop = false;
                }
                if (!strict_single_loop) continue;


                // Single exit loop verify
                if (info.exit >= fn->n_blocks || !fn->blocks[info.exit]) continue;

                printf("[Unroll MVP] Unrolling loop at header bb%d: trip_count=%lld, exit=bb%d\n", 
                       bi, info.trip_count, info.exit);

                int64_t T = info.trip_count;
                Block *header = fn->blocks[info.header];
                Block *latch = fn->blocks[info.latch];
                Block *exit_blk = fn->blocks[info.exit];

                // Gather PHI info in header
                RegID val_pre = 0;
                RegID val_latch = 0;
                for (Instr *ins = header->head; ins && ins->op == OP_PHI; ins = ins->next) {
                    if (ins->dst == info.ind_var) {
                        for (uint32_t i = 0; i < ins->n_phi; i++) {
                            if (ins->phi[i].block == info.preheader) {
                                val_pre = ins->phi[i].reg;
                            } else if (ins->phi[i].block == info.latch) {
                                val_latch = ins->phi[i].reg;
                            }
                        }
                    }
                }

                if (T > 0 && (!val_pre || !val_latch)) continue;

                // Record original block count before cloning
                uint32_t original_n_blocks = fn->n_blocks;

                // Create contexts for iterations 0 to T
                CloneContext *ctxs = malloc((T + 1) * sizeof(CloneContext));
                for (int64_t k = 0; k <= T; k++) {
                    clone_ctx_init(&ctxs[k]);
                }

                // Phase 1: Clone block skeletons
                Block **headers = malloc((T + 1) * sizeof(Block*));
                Block **latches = malloc(T * sizeof(Block*));

                for (int64_t k = 0; k <= T; k++) {
                    headers[k] = clone_block_skeleton(fn, header, &ctxs[k]);
                }
                if (latch != header) {
                    for (int64_t k = 0; k < T; k++) {
                        latches[k] = clone_block_skeleton(fn, latch, &ctxs[k]);
                    }
                }

                // Phase 2: Clone instructions and connect CFG sequence
                for (int64_t k = 0; k < T; k++) {
                    // Clone header_k
                    for (Instr *inst = header->head; inst; inst = inst->next) {
                        if (inst->op == OP_PHI) {
                            // Replace PHI with a copy of loop-carried value
                            Instr *copy = calloc(1, sizeof(Instr));
                            copy->op = OP_COPY;
                            copy->dst = ++fn->n_regs;
                            copy->id = copy->dst;
                            copy->n_src = 1;
                            if (k == 0) {
                                copy->src[0] = val_pre;
                            } else {
                                copy->src[0] = clone_ctx_get_local(&ctxs[k - 1], val_latch);
                            }
                            
                            // Map the original PHI destination to this copy's destination
                            clone_ctx_map_local(&ctxs[k], inst->dst, copy->dst);
                            
                            // Copy register type
                            extern char g_reg_types[MAX_INSTRS][16];
                            extern bool g_reg_is_param[MAX_INSTRS];
                            if (inst->dst < MAX_INSTRS) {
                                strcpy(g_reg_types[copy->dst], g_reg_types[inst->dst]);
                                g_reg_is_param[copy->dst] = g_reg_is_param[inst->dst];
                            }
                            append_instruction(headers[k], copy);
                        } else if (inst->op == OP_BR || inst->op == OP_CONDBR) {
                            // Branch unconditionally to latch (or next header)
                            Instr *br = calloc(1, sizeof(Instr));
                            br->op = OP_BR;
                            br->n_src = 1;
                            br->src[0] = (latch != header) ? latches[k]->id : headers[k + 1]->id;
                            append_instruction(headers[k], br);
                        } else {
                            Instr *cloned = clone_instruction(fn, inst, &ctxs[k]);
                            append_instruction(headers[k], cloned);
                        }
                    }

                    // Clone latch_k (if separate)
                    if (latch != header) {
                        for (Instr *inst = latch->head; inst; inst = inst->next) {
                            if (inst->op == OP_BR || inst->op == OP_CONDBR) {
                                Instr *br = calloc(1, sizeof(Instr));
                                br->op = OP_BR;
                                br->n_src = 1;
                                br->src[0] = headers[k + 1]->id;
                                append_instruction(latches[k], br);
                            } else {
                                Instr *cloned = clone_instruction(fn, inst, &ctxs[k]);
                                append_instruction(latches[k], cloned);
                            }
                        }
                    }
                }

                // Populate header_T (exit header copy)
                for (Instr *inst = header->head; inst; inst = inst->next) {
                    if (inst->op == OP_PHI) {
                        Instr *copy = calloc(1, sizeof(Instr));
                        copy->op = OP_COPY;
                        copy->dst = ++fn->n_regs;
                        copy->id = copy->dst;
                        copy->n_src = 1;
                        copy->src[0] = (T == 0) ? val_pre : clone_ctx_get_local(&ctxs[T - 1], val_latch);
                        
                        clone_ctx_map_local(&ctxs[T], inst->dst, copy->dst);
                        
                        extern char g_reg_types[MAX_INSTRS][16];
                        extern bool g_reg_is_param[MAX_INSTRS];
                        if (inst->dst < MAX_INSTRS) {
                            strcpy(g_reg_types[copy->dst], g_reg_types[inst->dst]);
                            g_reg_is_param[copy->dst] = g_reg_is_param[inst->dst];
                        }
                        append_instruction(headers[T], copy);
                    } else if (inst->op == OP_BR || inst->op == OP_CONDBR) {
                        // Exit branch
                        Instr *br = calloc(1, sizeof(Instr));
                        br->op = OP_BR;
                        br->n_src = 1;
                        br->src[0] = info.exit;
                        append_instruction(headers[T], br);
                    }
                }

                // Redirect preheader terminator to headers[0]
                Block *preh = fn->blocks[info.preheader];
                if (preh && preh->tail) {
                    if (preh->tail->op == OP_BR) {
                        preh->tail->src[0] = headers[0]->id;
                    } else if (preh->tail->op == OP_CONDBR) {
                        if (preh->tail->src[1] == header->id) preh->tail->src[1] = headers[0]->id;
                        if (preh->tail->src[2] == header->id) preh->tail->src[2] = headers[0]->id;
                    }
                }

                // Update PHI nodes in the exit block to reference headers[T] and its cloned registers
                for (Instr *inst = exit_blk->head; inst && inst->op == OP_PHI; inst = inst->next) {
                    for (uint32_t i = 0; i < inst->n_phi; i++) {
                        if (inst->phi[i].block == header->id) {
                            inst->phi[i].block = headers[T]->id;
                            inst->phi[i].reg = clone_ctx_get_local(&ctxs[T], inst->phi[i].reg);
                        }
                    }
                }

                // Rewrite uses of loop-defined registers outside the loop to point to final iteration versions
                for (uint32_t ob = 0; ob < original_n_blocks; ob++) {
                    if (ob == info.header || ob == info.latch) continue;
                    Block *obb = fn->blocks[ob];
                    if (!obb) continue;

                    for (Instr *inst = obb->head; inst; inst = inst->next) {
                        if (inst->op == OP_PHI) continue;

                        for (uint32_t i = 0; i < inst->n_src; i++) {
                            if (is_reg_operand_unroll(inst->op, i)) {
                                RegID src_reg = inst->src[i];
                                if (src_reg != 0 && clone_ctx_get_local(&ctxs[0], src_reg) != src_reg) {
                                    // Search backwards for the final mapped register
                                    RegID final_reg = src_reg;
                                    for (int64_t k = T; k >= 0; k--) {
                                        RegID mapped = clone_ctx_get_local(&ctxs[k], src_reg);
                                        if (mapped != src_reg) {
                                            final_reg = mapped;
                                            break;
                                        }
                                    }
                                    inst->src[i] = final_reg;
                                }
                            }
                        }
                    }
                }

                free(headers);
                if (latch != header) free(latches);
                free(ctxs);

                changed = true;
                break; // Process one loop per pass run to preserve stability
            }
        }
    }

    if (changed) {
        rebuild_preds_succs(fn);
        update_block_reachability(fn);
        rebuild_def_use(fn);
        run_ssa_verifier_assert(fn, "unroll");
    }

    return changed;
}
