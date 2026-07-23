/*
 * yul_weaver.c — EVM Yul Lowering & Stack Optimization Pass
 * =========================================================
 * Implements commutative-operand swap reduction and stack allocation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "../ir.h"

#define MAX_EVM_VREGS 256
#define MAX_EVM_STACK 16

typedef struct {
    char name[32];
    int start;
    int end;
    bool spilled;
    int slot;
} vreg_info_t;

typedef struct {
    vreg_info_t vregs[MAX_EVM_VREGS];
    int n_vregs;
    int stack[MAX_EVM_STACK];
    int stack_depth;
    int scratch_ptr;
} weaver_state_t;

static int find_vreg(weaver_state_t *st, const char *name) {
    if (!name) return -1;
    for (int i = 0; i < st->n_vregs; i++) {
        if (strcmp(st->vregs[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_vreg(weaver_state_t *st, const char *name) {
    int idx = find_vreg(st, name);
    if (idx >= 0) return idx;
    if (st->n_vregs >= MAX_EVM_VREGS) return -1;
    idx = st->n_vregs++;
    snprintf(st->vregs[idx].name, 31, "%s", name);
    st->vregs[idx].start = -1;
    st->vregs[idx].end = -1;
    st->vregs[idx].spilled = false;
    st->vregs[idx].slot = -1;
    return idx;
}

static void update_liveness(weaver_state_t *st, ir_node_t *n, int line) {
    if (n->dst) {
        int idx = add_vreg(st, n->dst);
        if (idx >= 0) {
            if (st->vregs[idx].start < 0) st->vregs[idx].start = line;
            st->vregs[idx].end = line;
        }
    }
    if (n->src1) {
        int idx = add_vreg(st, n->src1);
        if (idx >= 0) {
            if (st->vregs[idx].start < 0) st->vregs[idx].start = line;
            st->vregs[idx].end = line;
        }
    }
    if (n->src2) {
        int idx = add_vreg(st, n->src2);
        if (idx >= 0) {
            if (st->vregs[idx].start < 0) st->vregs[idx].start = line;
            st->vregs[idx].end = line;
        }
    }
}

static int stack_find(weaver_state_t *st, int vreg_id) {
    for (int i = st->stack_depth - 1; i >= 0; i--) {
        if (st->stack[i] == vreg_id) return i;
    }
    return -1;
}

static void stack_push(weaver_state_t *st, int vreg_id, FILE *out) {
    if (st->stack_depth >= MAX_EVM_STACK) {
        /* Spill lowest priority item */
        int spill_vreg = st->stack[0];
        st->vregs[spill_vreg].spilled = true;
        st->vregs[spill_vreg].slot = st->scratch_ptr;
        st->scratch_ptr += 32;
        fprintf(out, "    /* SPILL %s -> slot 0x%x */\n", st->vregs[spill_vreg].name, st->vregs[spill_vreg].slot);
        fprintf(out, "    push4 0x%x\n", st->vregs[spill_vreg].slot);
        fprintf(out, "    mstore\n");
        for (int i = 0; i < st->stack_depth - 1; i++) {
            st->stack[i] = st->stack[i + 1];
        }
        st->stack_depth--;
    }
    st->stack[st->stack_depth++] = vreg_id;
}

static bool bring_to_top_opt(weaver_state_t *st, int vreg_id, FILE *out, int opt_level, int *rewrites_applied) {
    int idx = stack_find(st, vreg_id);
    if (idx >= 0) {
        int top_idx = st->stack_depth - 1;
        int depth_from_top = top_idx - idx;

        if (depth_from_top > 0) {
            if (opt_level == 1) {
#ifdef FAULT_INJECT_BAD_SWAP
                /* P1 + P2: Fault injection gated on opt_level==1 ONLY.
                 * Omits required swap on non-commutative sstore, causing
                 * Storage_opt != Storage_unopt -> Exit 1 RED. */
                fprintf(out, "    /* FAULT INJECTED: OMITTED SWAP */\n");
                return false;
#else
                fprintf(out, "    swap%d /* %s */\n", depth_from_top, st->vregs[vreg_id].name);
                int tmp = st->stack[top_idx];
                st->stack[top_idx] = st->stack[idx];
                st->stack[idx] = tmp;
                /* Note: rewrites_applied is ONLY incremented when unopt would emit swap and opt eliminates it */
                return true;
#endif
            } else {
                /* opt_level == 0: Unoptimized baseline swap emission */
                fprintf(out, "    swap%d /* %s */\n", depth_from_top, st->vregs[vreg_id].name);
                int tmp = st->stack[top_idx];
                st->stack[top_idx] = st->stack[idx];
                st->stack[idx] = tmp;
                return true;
            }
        }
    } else if (st->vregs[vreg_id].spilled) {
        /* Restore from spill */
        fprintf(out, "    mload(0x%x) /* %s */\n", st->vregs[vreg_id].slot, st->vregs[vreg_id].name);
        stack_push(st, vreg_id, out);
    }
    return false;
}

static void pop_dead_vregs(weaver_state_t *st, int line, FILE *out) {
    while (st->stack_depth > 0) {
        int top_vreg = st->stack[st->stack_depth - 1];
        if (st->vregs[top_vreg].end <= line) {
            fprintf(out, "    pop /* dead %s */\n", st->vregs[top_vreg].name);
            st->stack_depth--;
        } else {
            break;
        }
    }
}

int evm_yul_weaver_opt(ir_func_t *fn, FILE *out, int opt_level) {
    weaver_state_t st;
    ir_node_t *n;
    int line = 0;
    int rewrites_applied = 0;
    
    memset(&st, 0, sizeof(st));
    st.scratch_ptr = 0x80; /* Leave standard 0x00-0x60 scratch/free-memory */

    /* Pass 1: Liveness analysis */
    for (n = fn->head; n; n = n->next) {
        update_liveness(&st, n, line++);
    }

    fprintf(out, "object \"StateHealer\" {\n");
    fprintf(out, "  code {\n");
    yul_emit_fixed_point_helpers(out);

    /* Pass 2: Emitting bounded Yul */
    line = 0;
    for (n = fn->head; n; n = n->next, line++) {
        fprintf(out, "    /* line %d: op %d */\n", line, n->op);
        if (n->op == IR_ADD || n->op == IR_SUB || n->op == IR_MUL || n->op == IR_DIV ||
            n->op == IR_FADD || n->op == IR_FSUB || n->op == IR_FMUL || n->op == IR_FDIV) {
            int src1_id = find_vreg(&st, n->src1);
            int src2_id = find_vreg(&st, n->src2);
            int dst_id = find_vreg(&st, n->dst);
            
            bool is_commutative = (n->op == IR_ADD || n->op == IR_MUL || n->op == IR_FADD || n->op == IR_FMUL);

            if (opt_level == 1 && is_commutative && st.stack_depth >= 2) {
                int top0 = st.stack[st.stack_depth - 1];
                int top1 = st.stack[st.stack_depth - 2];
                if ((top0 == src1_id && top1 == src2_id) || (top0 == src2_id && top1 == src1_id)) {
                    /* Commutative swap reduction: top two stack slots already hold operands in either order!
                     * Unoptimized weaver would emit swap1 to force exact order. Opt level 1 omits swap1! */
                    rewrites_applied++;
                } else {
                    if (src2_id >= 0) bring_to_top_opt(&st, src2_id, out, opt_level, &rewrites_applied);
                    if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
                }
            } else {
                if (src2_id >= 0) bring_to_top_opt(&st, src2_id, out, opt_level, &rewrites_applied);
                if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
            }

            if (n->op == IR_ADD) fprintf(out, "    add\n");
            else if (n->op == IR_SUB) fprintf(out, "    sub\n");
            else if (n->op == IR_MUL) fprintf(out, "    mul\n");
            else if (n->op == IR_DIV) fprintf(out, "    div\n");
            else {
                fprintf(out, "    %s\n", yul_lower_float_op(n->op));
            }
            
            if (src1_id >= 0) st.stack_depth--;
            if (src2_id >= 0) st.stack_depth--;
            
            if (dst_id >= 0) stack_push(&st, dst_id, out);
            
        } else if (n->op == IR_CONST || n->op == IR_FCONST) {
            fprintf(out, "    push32 0x%llx\n", (unsigned long long)n->imm);
            int dst_id = find_vreg(&st, n->dst);
            if (dst_id >= 0) stack_push(&st, dst_id, out);
            
        } else if (n->op == IR_STORE) {
            int src1_id = find_vreg(&st, n->src1); /* value */
            int dst_id = find_vreg(&st, n->dst);   /* address */
            
#if defined(FAULT_INJECT_BAD_SWAP)
            if (opt_level == 1) {
                /* P1 + P2: Corrupt storage slot key/value alignment on opt_level==1 ONLY by reversing operand order */
                if (dst_id >= 0) bring_to_top_opt(&st, dst_id, out, opt_level, &rewrites_applied);
                if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
            } else {
                if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
                if (dst_id >= 0) bring_to_top_opt(&st, dst_id, out, opt_level, &rewrites_applied);
            }
#else
            if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
            if (dst_id >= 0) bring_to_top_opt(&st, dst_id, out, opt_level, &rewrites_applied);
#endif
            
            fprintf(out, "    sstore\n");
            if (src1_id >= 0) st.stack_depth--;
            if (dst_id >= 0) st.stack_depth--;
            
        } else if (n->op == IR_LOAD) {
            int src1_id = find_vreg(&st, n->src1);
            int dst_id = find_vreg(&st, n->dst);
            
            if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
            fprintf(out, "    sload\n");
            
            if (src1_id >= 0) st.stack_depth--;
            if (dst_id >= 0) stack_push(&st, dst_id, out);
            
        } else if (n->op == IR_RET) {
            int src1_id = find_vreg(&st, n->src1);
            if (src1_id >= 0) bring_to_top_opt(&st, src1_id, out, opt_level, &rewrites_applied);
            fprintf(out, "    push1 0x00\n");
            fprintf(out, "    mstore\n");
            fprintf(out, "    push1 0x20\n");
            fprintf(out, "    push1 0x00\n");
            fprintf(out, "    return\n");
        }
        
        pop_dead_vregs(&st, line, out);
    }
    fprintf(out, "  }\n}\n");
    return rewrites_applied;
}

/* P4: One-line backward compatibility wrapper preserving main compiler entry point */
void evm_yul_weaver(ir_func_t *fn, FILE *out) {
    (void)evm_yul_weaver_opt(fn, out, 1);
}
