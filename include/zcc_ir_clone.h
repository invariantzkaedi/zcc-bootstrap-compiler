#ifndef ZCC_IR_CLONE_H
#define ZCC_IR_CLONE_H

typedef struct Function Function;
typedef struct BasicBlock BasicBlock;
typedef struct Instr Instr;

typedef struct {
    int *reg_old2new;
    int reg_cap;
} RegMap;

typedef struct {
    int *bb_old2new;
    int bb_cap;
} BbMap;

void regmap_init(RegMap *m, int old_nregs);
int  regmap_get_or_create(RegMap *m, Function *fn, int old_reg);

void bbmap_init(BbMap *m, int old_nbb);
int  bbmap_set(BbMap *m, int old_bb, int new_bb);
int  bbmap_get(const BbMap *m, int old_bb);

// Clone one instruction with remapped operands/dst
Instr *clone_instr_with_remap(Function *fn, const Instr *src, const RegMap *rmap);

// Clone a block’s instruction list
BasicBlock *clone_block_with_remap(Function *fn, const BasicBlock *src, const RegMap *rmap);

// PHI repair helpers
void phi_remove_incoming_from_pred(BasicBlock *bb, int pred_bb_id);
void phi_replace_incoming_pred(BasicBlock *bb, int old_pred, int new_pred);

#endif
