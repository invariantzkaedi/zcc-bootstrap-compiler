#include "clone_remap.h"
#include "zcc_ir_verify.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void clone_ctx_init(CloneContext *ctx) {
    memset(ctx, 0, sizeof(CloneContext));
}

void clone_ctx_map_local(CloneContext *ctx, RegID old_reg, RegID new_reg) {
    for (int i = 0; i < ctx->n_locals; i++) {
        if (ctx->local_map[i].old_reg == old_reg) {
            ctx->local_map[i].new_reg = new_reg;
            return;
        }
    }
    if (ctx->n_locals < MAX_CLONE_MAP_ENTRIES) {
        ctx->local_map[ctx->n_locals].old_reg = old_reg;
        ctx->local_map[ctx->n_locals].new_reg = new_reg;
        ctx->n_locals++;
    } else {
        fprintf(stderr, "CloneContext: local map overflow\n");
        exit(1);
    }
}

RegID clone_ctx_get_local(const CloneContext *ctx, RegID old_reg) {
    if (old_reg == 0) return 0;
    for (int i = 0; i < ctx->n_locals; i++) {
        if (ctx->local_map[i].old_reg == old_reg) {
            return ctx->local_map[i].new_reg;
        }
    }
    return old_reg; // Fallback to original register if not mapped
}

void clone_ctx_map_block(CloneContext *ctx, BlockID old_blk, BlockID new_blk) {
    for (int i = 0; i < ctx->n_blocks; i++) {
        if (ctx->block_map[i].old_blk == old_blk) {
            ctx->block_map[i].new_blk = new_blk;
            return;
        }
    }
    if (ctx->n_blocks < MAX_CLONE_MAP_ENTRIES) {
        ctx->block_map[ctx->n_blocks].old_blk = old_blk;
        ctx->block_map[ctx->n_blocks].new_blk = new_blk;
        ctx->n_blocks++;
    } else {
        fprintf(stderr, "CloneContext: block map overflow\n");
        exit(1);
    }
}

BlockID clone_ctx_get_block(const CloneContext *ctx, BlockID old_blk) {
    if (old_blk == 0) return 0;
    for (int i = 0; i < ctx->n_blocks; i++) {
        if (ctx->block_map[i].old_blk == old_blk) {
            return ctx->block_map[i].new_blk;
        }
    }
    return old_blk; // Fallback to original block if not mapped
}

void clone_ctx_map_value(CloneContext *ctx, int old_val, int new_val) {
    for (int i = 0; i < ctx->n_values; i++) {
        if (ctx->value_map[i].old_val == old_val) {
            ctx->value_map[i].new_val = new_val;
            return;
        }
    }
    if (ctx->n_values < MAX_CLONE_MAP_ENTRIES) {
        ctx->value_map[ctx->n_values].old_val = old_val;
        ctx->value_map[ctx->n_values].new_val = new_val;
        ctx->n_values++;
    } else {
        fprintf(stderr, "CloneContext: value map overflow\n");
        exit(1);
    }
}

int clone_ctx_get_value(const CloneContext *ctx, int old_val) {
    for (int i = 0; i < ctx->n_values; i++) {
        if (ctx->value_map[i].old_val == old_val) {
            return ctx->value_map[i].new_val;
        }
    }
    return old_val; // For generic value constants/expressions, return old_val as fallback
}

void clone_ctx_dump(const CloneContext *ctx) {
    printf("--- CloneContext Dump ---\n");
    printf("Locals (%d):\n", ctx->n_locals);
    for (int i = 0; i < ctx->n_locals; i++) {
        printf("  %%r%d -> %%r%d\n", ctx->local_map[i].old_reg, ctx->local_map[i].new_reg);
    }
    printf("Blocks (%d):\n", ctx->n_blocks);
    for (int i = 0; i < ctx->n_blocks; i++) {
        printf("  bb%d -> bb%d\n", ctx->block_map[i].old_blk, ctx->block_map[i].new_blk);
    }
    printf("Values (%d):\n", ctx->n_values);
    for (int i = 0; i < ctx->n_values; i++) {
        printf("  %d -> %d\n", ctx->value_map[i].old_val, ctx->value_map[i].new_val);
    }
    printf("-------------------------\n");
}

Block *clone_block_skeleton(Function *fn, const Block *old_block, CloneContext *ctx) {
    if (fn->n_blocks >= MAX_BLOCKS) {
        fprintf(stderr, "Error: Max blocks exceeded\n");
        exit(1);
    }
    Block *new_bb = calloc(1, sizeof(Block));
    new_bb->id = fn->n_blocks;
    sprintf(new_bb->name, "bb%d_clone", new_bb->id);
    new_bb->reachable = true;
    new_bb->loop_depth = old_block->loop_depth;
    new_bb->is_loop_header = false;
    new_bb->is_pre_header = false;
    
    clone_ctx_map_block(ctx, old_block->id, new_bb->id);
    fn->blocks[fn->n_blocks++] = new_bb;
    return new_bb;
}

Instr *clone_instruction(Function *fn, const Instr *old_inst, CloneContext *ctx) {
    Instr *new_inst = calloc(1, sizeof(Instr));
    new_inst->op = old_inst->op;
    new_inst->imm = old_inst->imm;
    new_inst->is_float = old_inst->is_float;
    new_inst->src_is_float = old_inst->src_is_float;
    new_inst->src_size = old_inst->src_size;
    new_inst->dst_size = old_inst->dst_size;
    new_inst->src_unsigned = old_inst->src_unsigned;
    new_inst->dst_unsigned = old_inst->dst_unsigned;
    new_inst->escape = old_inst->escape;
    new_inst->is_param = old_inst->is_param;
    new_inst->exec_freq = old_inst->exec_freq;
    new_inst->line_no = old_inst->line_no;
    new_inst->ir_type = old_inst->ir_type;
    new_inst->alias_class = old_inst->alias_class;
    new_inst->sbt_base = old_inst->sbt_base;
    new_inst->sbt_offset = old_inst->sbt_offset;
    new_inst->sbt_has_cast = old_inst->sbt_has_cast;
    new_inst->amf = old_inst->amf;
    
    if (old_inst->asm_string) {
        new_inst->asm_string = strdup(old_inst->asm_string);
    }
    
    if (old_inst->op == OP_CALL) {
        strcpy(new_inst->call_name, old_inst->call_name);
        new_inst->n_call_args = old_inst->n_call_args;
        for (uint32_t i = 0; i < old_inst->n_call_args; i++) {
            new_inst->call_args[i] = clone_ctx_get_local(ctx, old_inst->call_args[i]);
            new_inst->call_args_is_float[i] = old_inst->call_args_is_float[i];
        }
    }
    
    // Remap operands (src) with branch destination mapping support
    new_inst->n_src = old_inst->n_src;
    if (old_inst->op == OP_BR) {
        new_inst->src[0] = clone_ctx_get_block(ctx, old_inst->src[0]);
    } else if (old_inst->op == OP_CONDBR) {
        new_inst->src[0] = clone_ctx_get_local(ctx, old_inst->src[0]);
        new_inst->src[1] = clone_ctx_get_block(ctx, old_inst->src[1]);
        new_inst->src[2] = clone_ctx_get_block(ctx, old_inst->src[2]);
    } else {
        for (uint32_t i = 0; i < old_inst->n_src; i++) {
            new_inst->src[i] = clone_ctx_get_local(ctx, old_inst->src[i]);
        }
    }
    
    // Remap AMF base
    if (new_inst->amf.folded && new_inst->amf.base > 0) {
        new_inst->amf.base = clone_ctx_get_local(ctx, new_inst->amf.base);
    }
    
    // Remap SBT base
    if (new_inst->sbt_base > 0) {
        new_inst->sbt_base = clone_ctx_get_local(ctx, new_inst->sbt_base);
    }
    
    // Handle destination register
    if (old_inst->dst != 0) {
        RegID new_reg = ++fn->n_regs;
        if (new_reg >= MAX_INSTRS) {
            fprintf(stderr, "Error: Max registers exceeded\n");
            exit(1);
        }
        new_inst->dst = new_reg;
        clone_ctx_map_local(ctx, old_inst->dst, new_reg);
        
        // Populate type in global mapping
        extern char g_reg_types[MAX_INSTRS][16];
        extern bool g_reg_is_param[MAX_INSTRS];
        if (old_inst->dst < MAX_INSTRS) {
            strcpy(g_reg_types[new_reg], g_reg_types[old_inst->dst]);
            g_reg_is_param[new_reg] = g_reg_is_param[old_inst->dst];
        }
    }
    
    new_inst->id = new_inst->dst; // Keep id aligned
    
    // Remap PHI basic fields
    if (old_inst->op == OP_PHI) {
        new_inst->n_phi = old_inst->n_phi;
        for (uint32_t i = 0; i < old_inst->n_phi; i++) {
            new_inst->phi[i].reg = clone_ctx_get_local(ctx, old_inst->phi[i].reg);
            new_inst->phi[i].block = clone_ctx_get_block(ctx, old_inst->phi[i].block);
        }
    }
    
    return new_inst;
}

void rebuild_preds_succs(Function *fn) {
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
}

void update_block_reachability(Function *fn) {
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        if (fn->blocks[bi]) fn->blocks[bi]->reachable = false;
    }

    bool *visited = calloc(fn->n_blocks, sizeof(bool));
    BlockID *queue = malloc(fn->n_blocks * sizeof(BlockID));
    int head = 0, tail = 0;

    if (fn->n_blocks > 0 && fn->blocks[0]) {
        queue[tail++] = 0;
        visited[0] = true;
        fn->blocks[0]->reachable = true;
    }

    while (head < tail) {
        BlockID curr = queue[head++];
        Block *bb = fn->blocks[curr];
        if (!bb) continue;

        for (uint32_t i = 0; i < bb->n_succs; i++) {
            BlockID succ = bb->succs[i];
            if (succ < fn->n_blocks && !visited[succ] && fn->blocks[succ]) {
                visited[succ] = true;
                fn->blocks[succ]->reachable = true;
                queue[tail++] = succ;
            }
        }
    }
    free(visited);
    free(queue);
}

void phi_replace_incoming_pred(Block *bb, BlockID old_pred, BlockID new_pred) {
    for (Instr *it = bb->head; it; it = it->next) {
        if (it->op == OP_PHI) {
            for (uint32_t i = 0; i < it->n_phi; i++) {
                if (it->phi[i].block == old_pred) {
                    it->phi[i].block = new_pred;
                }
            }
        }
    }
}

void phi_remove_incoming_from_pred(Block *bb, BlockID pred_bb_id) {
    for (Instr *it = bb->head; it; it = it->next) {
        if (it->op == OP_PHI) {
            uint32_t write_idx = 0;
            for (uint32_t i = 0; i < it->n_phi; i++) {
                if (it->phi[i].block != pred_bb_id) {
                    if (write_idx != i) {
                        it->phi[write_idx] = it->phi[i];
                    }
                    write_idx++;
                }
            }
            it->n_phi = write_idx;
        }
    }
}

void phi_add_incoming(Instr *phi_inst, RegID reg, BlockID block) {
    if (phi_inst->op != OP_PHI) return;
    if (phi_inst->n_phi >= MAX_PHI_SOURCES) {
        fprintf(stderr, "Error: Max PHI sources exceeded\n");
        exit(1);
    }
    phi_inst->phi[phi_inst->n_phi].reg = reg;
    phi_inst->phi[phi_inst->n_phi].block = block;
    phi_inst->n_phi++;
}

void run_ssa_verifier_assert(Function *fn, const char *pass_name) {
#ifndef NDEBUG
    VerifyError errors[256];
    VerifyReport report = {
        .ok = true,
        .errors = errors,
        .n_errors = 0,
        .cap_errors = 256
    };
    if (!verify_function_ir(fn, &report)) {
        fprintf(stderr, "CRITICAL ERROR: SSA/IR Verification failed after pass '%s'!\n", pass_name);
        for (int i = 0; i < report.n_errors; i++) {
            fprintf(stderr, "  [%s] Block bb%d, Inst ID %d, Reg %%r%d: %s\n",
                    report.errors[i].kind,
                    report.errors[i].bb_id,
                    report.errors[i].inst_id,
                    report.errors[i].reg_id,
                    report.errors[i].message);
        }
        exit(1);
    }
#endif
}
