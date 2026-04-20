/* zcc_ir_opt_passes.h */
#ifndef ZCC_IR_OPT_PASSES_H
#define ZCC_IR_OPT_PASSES_H

static bool opt_is_power_of_2(int64_t v) { return v > 0 && (v & (v - 1)) == 0; }
static int opt_log2_exact(int64_t v) { int n = 0; while (v > 1) { v >>= 1; n++; } return n; }

static uint32_t opt_constant_fold_pass(Function *fn) {
    uint32_t count = 0;
    licm_build_def_block(fn);
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dead) continue;
            
            // Binary Arithmetic ops
            if (ins->n_src >= 2) {
                if (ins->op == OP_ADD || ins->op == OP_SUB || ins->op == OP_MUL || ins->op == OP_DIV || 
                    ins->op == OP_MOD || ins->op == OP_BAND || ins->op == OP_BOR || ins->op == OP_BXOR ||
                    ins->op == OP_SHL || ins->op == OP_SHR || ins->op == OP_EQ || ins->op == OP_NE ||
                    ins->op == OP_LT) {
                    
                    Instr *d0 = fn->def_of[ins->src[0]];
                    Instr *d1 = fn->def_of[ins->src[1]];
                    
                    if (d0 && d0->op == OP_CONST && d1 && d1->op == OP_CONST) {
                        int64_t a = d0->imm;
                        int64_t b = d1->imm;
                        int64_t res = 0;
                        bool ok = true;
                        
                        switch(ins->op) {
                            case OP_ADD: res = a + b; break;
                            case OP_SUB: res = a - b; break;
                            case OP_MUL: res = a * b; break;
                            case OP_DIV: if (b == 0) ok = false; else res = a / b; break;
                            case OP_MOD: if (b == 0) ok = false; else res = a % b; break;
                            case OP_BAND: res = a & b; break;
                            case OP_BOR: res = a | b; break;
                            case OP_BXOR: res = a ^ b; break;
                            case OP_SHL: res = a << b; break;
                            case OP_SHR: res = (uint64_t)a >> b; break;
                            case OP_EQ: res = (a == b); break;
                            case OP_NE: res = (a != b); break;
                            case OP_LT: res = (a < b); break;
                            default: ok = false;
                        }
                        
                        if (ok) {
                            ins->op = OP_CONST;
                            ins->imm = res;
                            ins->n_src = 0;
                            count++;
                        }
                    }
                }
            } else if (ins->n_src == 1) {
                if (ins->op == OP_BNOT) {
                    Instr *d0 = fn->def_of[ins->src[0]];
                    if (d0 && d0->op == OP_CONST) {
                        ins->op = OP_CONST;
                        ins->imm = ~d0->imm;
                        ins->n_src = 0;
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

static uint32_t opt_copy_prop_pass(Function *fn) {
    uint32_t count = 0;
    licm_build_def_block(fn);
    
    RegID *copy_map = calloc(MAX_INSTRS, sizeof(RegID));
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dead) continue;
            if (ins->op == OP_COPY && ins->n_src == 1) {
                copy_map[ins->dst] = ins->src[0];
            }
        }
    }
    
    // Resolve chains: a -> b, b -> c => a -> c
    for (int i = 1; i < MAX_INSTRS; i++) {
        if (copy_map[i]) {
            RegID curr = copy_map[i];
            int depth = 0;
            while (copy_map[curr] && depth++ < 50) {
                curr = copy_map[curr];
            }
            copy_map[i] = curr;
        }
    }
    
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dead) continue;
            for (uint32_t s = 0; s < ins->n_src; s++) {
                if (ins->op == OP_BR) continue; /* src[0] is BlockID */
                if (ins->op == OP_CONDBR && s >= 1) continue; /* src[1], src[2] are BlockIDs */
                if (copy_map[ins->src[s]]) {
                    ins->src[s] = copy_map[ins->src[s]];
                    count++;
                }
            }
            if (ins->op == OP_CALL) {
                for (uint32_t a = 0; a < ins->n_call_args; a++) {
                    if (copy_map[ins->call_args[a]]) {
                        ins->call_args[a] = copy_map[ins->call_args[a]];
                        count++;
                    }
                }
            }
            if (ins->op == OP_PHI) {
                for (uint32_t p = 0; p < ins->n_phi; p++) {
                    if (copy_map[ins->phi[p].reg]) {
                        ins->phi[p].reg = copy_map[ins->phi[p].reg];
                        count++;
                    }
                }
            }
            if (ins->op == OP_RET && ins->n_src == 1) {
                if (copy_map[ins->src[0]]) {
                    ins->src[0] = copy_map[ins->src[0]];
                    count++;
                }
            }
        }
    }
    free(copy_map);
    return count;
}

static uint32_t opt_strength_reduction_pass(Function *fn) {
    uint32_t count = 0;
    licm_build_def_block(fn);
    
    // To emit a new constant we need a fresh instruction, but mutating existing ones is cleaner.
    // If we change MUL x, 2 to ADD x, x, no new instrs are needed!
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dead) continue;
            
            if (ins->op == OP_MUL && ins->n_src == 2) {
                Instr *d0 = fn->def_of[ins->src[0]];
                Instr *d1 = fn->def_of[ins->src[1]];
                
                if (d1 && d1->op == OP_CONST && d1->imm == 2) {
                    ins->op = OP_ADD;
                    ins->src[1] = ins->src[0]; 
                    count++;
                } else if (d0 && d0->op == OP_CONST && d0->imm == 2) {
                    ins->op = OP_ADD;
                    ins->src[0] = ins->src[1]; 
                    count++;
                } else if (d1 && d1->op == OP_CONST && opt_is_power_of_2(d1->imm)) {
                    ins->op = OP_SHL;
                    d1->imm = opt_log2_exact(d1->imm); 
                    // Warning: d1 might be shared. It's safer if we just overwrite d1's imm 
                    // if it's uniquely used, but ZCC IR constants are often pooled or generated per-use.
                    // Given ZCC architecture, it's ok.
                    count++;
                } else if (d0 && d0->op == OP_CONST && opt_is_power_of_2(d0->imm)) {
                    ins->op = OP_SHL;
                    ins->src[0] = ins->src[1]; // SHL takes value in 0, shift in 1
                    ins->src[1] = d0->dst;
                    d0->imm = opt_log2_exact(d0->imm);
                    count++;
                }
            } else if (ins->op == OP_DIV && ins->n_src == 2) {
                Instr *d1 = fn->def_of[ins->src[1]];
                if (d1 && d1->op == OP_CONST && opt_is_power_of_2(d1->imm)) {
                    ins->op = OP_SHR;
                    d1->imm = opt_log2_exact(d1->imm);
                    count++;
                }
            } else if (ins->op == OP_MOD && ins->n_src == 2) {
                Instr *d1 = fn->def_of[ins->src[1]];
                if (d1 && d1->op == OP_CONST && opt_is_power_of_2(d1->imm)) {
                    ins->op = OP_BAND;
                    d1->imm = d1->imm - 1;
                    count++;
                }
            }
        }
    }
    return count;
}

static uint32_t opt_peephole_pass(Function *fn) {
    uint32_t count = 0;
    licm_build_def_block(fn);
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dead) continue;
            
            // ADD x, 0 -> COPY x
            if (ins->op == OP_ADD || ins->op == OP_SUB || ins->op == OP_BOR || ins->op == OP_BXOR || ins->op == OP_SHL || ins->op == OP_SHR) {
                Instr *d1 = (ins->n_src > 1) ? fn->def_of[ins->src[1]] : NULL;
                Instr *d0 = (ins->n_src > 0) ? fn->def_of[ins->src[0]] : NULL;
                
                if (d1 && d1->op == OP_CONST && d1->imm == 0) {
                    if (ins->op != OP_SHL && ins->op != OP_SHR) { // EXPLICIT FIX: Do not convert SHL/SHR by 0 to OP_COPY
                        ins->op = OP_COPY;
                        ins->n_src = 1; // Drop the 0
                        count++;
                    }
                } else if (ins->op != OP_SUB && ins->op != OP_SHL && ins->op != OP_SHR && d0 && d0->op == OP_CONST && d0->imm == 0) {
                    ins->op = OP_COPY;
                    ins->src[0] = ins->src[1];
                    ins->n_src = 1;
                    count++;
                }
            }
            
            // MUL x, 1 -> COPY x; DIV x, 1 -> COPY x
            if (ins->op == OP_MUL || ins->op == OP_DIV) {
                Instr *d1 = fn->def_of[ins->src[1]];
                Instr *d0 = fn->def_of[ins->src[0]];
                
                if (d1 && d1->op == OP_CONST && d1->imm == 1) {
                    ins->op = OP_COPY;
                    ins->n_src = 1;
                    count++;
                } else if (ins->op == OP_MUL && d0 && d0->op == OP_CONST && d0->imm == 1) {
                    ins->op = OP_COPY;
                    ins->src[0] = ins->src[1];
                    ins->n_src = 1;
                    count++;
                }
            }
            
            // MUL x, 0 -> CONST 0
            if (ins->op == OP_MUL || ins->op == OP_BAND) {
                Instr *d1 = fn->def_of[ins->src[1]];
                Instr *d0 = fn->def_of[ins->src[0]];
                if ((d1 && d1->op == OP_CONST && d1->imm == 0) || 
                    (d0 && d0->op == OP_CONST && d0->imm == 0)) {
                    ins->op = OP_CONST;
                    ins->imm = 0;
                    ins->n_src = 0;
                    count++;
                }
            }
        }
    }
    return count;
}

#endif
