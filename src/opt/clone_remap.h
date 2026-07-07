#ifndef ZCC_CLONE_REMAP_H
#define ZCC_CLONE_REMAP_H

#include "zcc_ir.h"
#include <stdint.h>

#define MAX_CLONE_MAP_ENTRIES 4096

typedef struct {
    RegID old_reg;
    RegID new_reg;
} RegMapEntry;

typedef struct {
    BlockID old_blk;
    BlockID new_blk;
} BlockMapEntry;

typedef struct {
    int old_val;
    int new_val;
} ValueMapEntry;

typedef struct {
    RegMapEntry local_map[MAX_CLONE_MAP_ENTRIES];
    int n_locals;

    BlockMapEntry block_map[MAX_CLONE_MAP_ENTRIES];
    int n_blocks;

    ValueMapEntry value_map[MAX_CLONE_MAP_ENTRIES];
    int n_values;
} CloneContext;

void clone_ctx_init(CloneContext *ctx);

void clone_ctx_map_local(CloneContext *ctx, RegID old_reg, RegID new_reg);
RegID clone_ctx_get_local(const CloneContext *ctx, RegID old_reg);

void clone_ctx_map_block(CloneContext *ctx, BlockID old_blk, BlockID new_blk);
BlockID clone_ctx_get_block(const CloneContext *ctx, BlockID old_blk);

void clone_ctx_map_value(CloneContext *ctx, int old_val, int new_val);
int clone_ctx_get_value(const CloneContext *ctx, int old_val);

void clone_ctx_dump(const CloneContext *ctx);

// M4-02: Block and instruction cloning core interfaces
Block *clone_block_skeleton(Function *fn, const Block *old_block, CloneContext *ctx);
Instr *clone_instruction(Function *fn, const Instr *old_inst, CloneContext *ctx);

// M4-03: CFG/terminator helpers
void rebuild_preds_succs(Function *fn);
void update_block_reachability(Function *fn);

// M4-04: PHI repair helpers
void phi_replace_incoming_pred(Block *bb, BlockID old_pred, BlockID new_pred);
void phi_remove_incoming_from_pred(Block *bb, BlockID pred_bb_id);
void phi_add_incoming(Instr *phi_inst, RegID reg, BlockID block);

// M4-05: Def-use rewire + SSA verifier assert
void run_ssa_verifier_assert(Function *fn, const char *pass_name);

#endif
