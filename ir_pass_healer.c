// ==============================================================================
// ZCC IR PASS: AUTO-HEALER (THE SURGEON)
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================
#include "ir.h"
#include "ir_vuln_tag.h"
#include <stdio.h>
#include <string.h>

static ir_node_t *ir_emit_before(ir_func_t *fn, ir_node_t *n, ir_op_t op, ir_type_t ty, 
                                 const char *dst, const char *src1, const char *src2, 
                                 const char *label, long imm, int lineno) {
    extern ir_node_t *ir_node_alloc(void);
    ir_node_t *new_node = ir_node_alloc();
    new_node->op = op;
    new_node->type = ty;
    if (dst) strncpy(new_node->dst, dst, IR_NAME_MAX - 1);
    if (src1) strncpy(new_node->src1, src1, IR_NAME_MAX - 1);
    if (src2) strncpy(new_node->src2, src2, IR_NAME_MAX - 1);
    if (label) strncpy(new_node->label, label, IR_LABEL_MAX - 1);
    new_node->imm = imm;
    new_node->lineno = lineno;
    
    // Find prev
    ir_node_t *prev = NULL;
    for (ir_node_t *scan = fn->head; scan; scan = scan->next) {
        if (scan == n) break;
        prev = scan;
    }
    
    if (prev) {
        prev->next = new_node;
    } else {
        fn->head = new_node;
    }
    new_node->next = n;
    fn->node_count++;
    return new_node;
}

typedef struct {
    int nodes_before;
    int nodes_after;
    int nodes_deleted;
    int nodes_modified;
    int changed;
} ir_pass_result_t;

ir_pass_result_t ir_pass_auto_heal(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r = {0};
    int healed = 0;

    r.nodes_before = fn->node_count;

    for (ir_node_t *n = fn->head; n; n = n->next) {
        // Detect the bleeding edge: A State Write crossing a Privilege Boundary
        if ((n->vuln_tags & IR_VULN_STATE_WRITE) && (n->vuln_tags & IR_VULN_PRIV_BOUNDARY)) {
            char owner_vreg[IR_NAME_MAX];
            char caller_vreg[IR_NAME_MAX];
            char cmp_vreg[IR_NAME_MAX];
            char not_vreg[IR_NAME_MAX];
            char zero_slot[IR_NAME_MAX];
            
            extern void ir_fresh_tmp(ir_func_t *fn, char *buf);
            ir_fresh_tmp(fn, owner_vreg);
            ir_fresh_tmp(fn, caller_vreg);
            ir_fresh_tmp(fn, cmp_vreg);
            ir_fresh_tmp(fn, not_vreg);
            ir_fresh_tmp(fn, zero_slot);

            // 1. Suture Step 1: Load the trusted Owner from Slot 0
            ir_emit_before(fn, n, IR_CONST, IR_TY_I64, zero_slot, "", "", "", 0, n->lineno);
            ir_emit_before(fn, n, IR_LOAD, IR_TY_I64, owner_vreg, zero_slot, "", "", 0, n->lineno);

            // 2. Suture Step 2: Load the active msg.sender (simulated via lifter call)
            ir_emit_before(fn, n, IR_CALL, IR_TY_I64, caller_vreg, "", "", "evm_caller", 0, n->lineno);

            // 3. Suture Step 3: The Cryptographic Guard (caller == owner)
            ir_emit_before(fn, n, IR_EQ, IR_TY_I64, cmp_vreg, caller_vreg, owner_vreg, "", 0, n->lineno);
            
            // 4. Suture Step 4: Branch to continue if equal, otherwise revert
            char continue_label[IR_LABEL_MAX];
            extern void ir_fresh_label(ir_func_t *fn, char *buf);
            ir_fresh_label(fn, continue_label);

            ir_node_t *br = ir_emit_before(fn, n, IR_BR_IF, IR_TY_VOID, "", cmp_vreg, "", continue_label, 0, n->lineno);
            br->vuln_tags |= IR_VULN_EXEC_BARRIER; // Anchor the new topology

            // The Revert Block
            ir_emit_before(fn, n, IR_RET, IR_TY_VOID, "", "0", "", "", 0, n->lineno); // EVM Revert is IR_RET 0
            
            // The Continue Label
            ir_emit_before(fn, n, IR_LABEL, IR_TY_VOID, "", "", "", continue_label, 0, n->lineno);

            healed++;
        }
    }
    
    r.nodes_after = fn->node_count;
    r.nodes_modified = healed;
    r.changed = healed > 0;
    return r;
}
