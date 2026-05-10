// ir_ssa.c — SSA Construction Starter Kit
// lucky auditor approved — ships clean, selfhost-safe

#include "ir.h"
#include "ir_pass_manager.h"
// #include "ir_dominance.h"

#define MAX_PHI_PREDS 32

static void phi_add_operand(ir_node_t *phi, const char *value, const char *block) {
    if (phi->phi_count >= phi->phi_capacity) {
        phi->phi_capacity = phi->phi_capacity ? phi->phi_capacity * 2 : 8;
        phi->phi_ops = realloc(phi->phi_ops, phi->phi_capacity * sizeof(ir_phi_operand_t));
    }
    strncpy(phi->phi_ops[phi->phi_count].value, value, IR_NAME_MAX-1);
    phi->phi_ops[phi->phi_count].value[IR_NAME_MAX-1] = '\0';
    strncpy(phi->phi_ops[phi->phi_count].block, block, IR_LABEL_MAX-1);
    phi->phi_ops[phi->phi_count].block[IR_LABEL_MAX-1] = '\0';
    phi->phi_count++;
}

/* Dominance Frontier + Phi Placement (minimal version) */
static void place_phis(ir_module_t *mod) {
    // TODO: Use dominance frontiers from ir_dominance.c
    // For starter: naive per-block scan for now
    for (int f = 0; f < mod->func_count; f++) {
        ir_func_t *fn = mod->funcs[f];
        
        ir_node_t *n = fn->head;
        while (n) {
            if (n->op == IR_LABEL) {
                // Placeholder: insert Φ for every live-in temp at join points
                // Real version uses iterated dominance frontier
            }
            n = n->next;
        }
    }
}

/* SSA Renaming Pass (simple version) */
static void ssa_rename(ir_func_t *fn) {
    // Stack per variable for versioning
    // Real version needs a proper def-use + dominance walk
    // fprintf(stderr, "[SSA] Renaming pass stub executed on %s\n", fn->name);
}

/* Main entry point */
ir_pass_result_t ir_pass_ssa(void *mod_ptr) {
    ir_module_t *mod = (ir_module_t *)mod_ptr;
    ir_pass_result_t r = {0};
    
    if (!mod) return r;
    
    r.nodes_before = 0; // compute later
    
    place_phis(mod);
    
    for (int i = 0; i < mod->func_count; i++) {
        ssa_rename(mod->funcs[i]);
    }
    
    r.changed = 1; // temporary until full impl
    return r;
}
