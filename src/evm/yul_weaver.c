/*
 * yul_weaver.c — ZKAEDI Physical EVM Stack Allocator & Yul Emitter
 * =================================================================
 * Eliminates the EVM "Stack Too Deep" limitation via deterministic
 * def-use liveness analysis and bounded stack/memory spilling.
 */

#include <stdio.h>
#include <string.h>
#include "../../ir.h"
#include "yul_fixed_point.h"

#define MAX_VREGS 1024
#define MAX_EVM_STACK 16

typedef struct {
    char name[64];
    int start;
    int end;
    int slot;
    int spilled;
} vreg_live_t;

typedef struct {
    vreg_live_t vregs[MAX_VREGS];
    int count;
    int stack[MAX_EVM_STACK]; /* Contains vreg index */
    int stack_depth;
    int scratch_ptr;
} weaver_state_t;

static int find_vreg(weaver_state_t *st, const char *name) {
    int i;
    for (i = 0; i < st->count; i++) {
        if (strcmp(st->vregs[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_vreg(weaver_state_t *st, const char *name, int line) {
    if (st->count >= MAX_VREGS) return -1;
    strncpy(st->vregs[st->count].name, name, 63);
    st->vregs[st->count].name[63] = '\0';
    st->vregs[st->count].start = line;
    st->vregs[st->count].end = line;
    st->vregs[st->count].slot = -1;
    st->vregs[st->count].spilled = 0;
    return st->count++;
}

static void update_liveness(weaver_state_t *st, ir_node_t *n, int line) {
    int i;
    if (n->dst[0]) {
        i = find_vreg(st, n->dst);
        if (i < 0) i = add_vreg(st, n->dst, line);
        if (i >= 0 && st->vregs[i].end < line) st->vregs[i].end = line;
    }
    if (n->src1[0]) {
        i = find_vreg(st, n->src1);
        if (i < 0) i = add_vreg(st, n->src1, line);
        if (i >= 0 && st->vregs[i].end < line) st->vregs[i].end = line;
    }
    if (n->src2[0]) {
        i = find_vreg(st, n->src2);
        if (i < 0) i = add_vreg(st, n->src2, line);
        if (i >= 0 && st->vregs[i].end < line) st->vregs[i].end = line;
    }
}

static int stack_find(weaver_state_t *st, int vreg_id) {
    int i;
    for (i = 0; i < st->stack_depth; i++) {
        if (st->stack[i] == vreg_id) return i;
    }
    return -1;
}

static void stack_push(weaver_state_t *st, int vreg_id, FILE *out) {
    if (st->stack_depth >= MAX_EVM_STACK) {
        /* Spill the deepest register (index 0) */
        int spill_vreg = st->stack[0];
        fprintf(out, "    // spill %s to memory 0x%x\n", st->vregs[spill_vreg].name, st->scratch_ptr);
        fprintf(out, "    swap15\n");
        fprintf(out, "    mstore(0x%x, swap15)\n", st->scratch_ptr);
        st->vregs[spill_vreg].spilled = 1;
        st->vregs[spill_vreg].slot = st->scratch_ptr;
        st->scratch_ptr += 32;
        
        /* Shift stack down */
        int i;
        for (i = 0; i < st->stack_depth - 1; i++) {
            st->stack[i] = st->stack[i + 1];
        }
        st->stack_depth--;
    }
    st->stack[st->stack_depth++] = vreg_id;
}

static void bring_to_top(weaver_state_t *st, int vreg_id, FILE *out) {
    int idx = stack_find(st, vreg_id);
    if (idx >= 0) {
        int depth_from_top = (st->stack_depth - 1) - idx;
        if (depth_from_top > 0) {
            fprintf(out, "    swap%d /* %s */\n", depth_from_top, st->vregs[vreg_id].name);
            /* Swap in our tracking array */
            int top_idx = st->stack_depth - 1;
            int tmp = st->stack[top_idx];
            st->stack[top_idx] = st->stack[idx];
            st->stack[idx] = tmp;
        }
    } else if (st->vregs[vreg_id].spilled) {
        /* Restore from spill */
        fprintf(out, "    mload(0x%x) /* %s */\n", st->vregs[vreg_id].slot, st->vregs[vreg_id].name);
        stack_push(st, vreg_id, out);
    }
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

void evm_yul_weaver(ir_func_t *fn, FILE *out) {
    weaver_state_t st;
    ir_node_t *n;
    int line = 0;
    
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
            
            if (src2_id >= 0) bring_to_top(&st, src2_id, out);
            if (src1_id >= 0) bring_to_top(&st, src1_id, out);
            
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
            
            if (src1_id >= 0) bring_to_top(&st, src1_id, out);
            if (dst_id >= 0) bring_to_top(&st, dst_id, out);
            
            fprintf(out, "    sstore\n");
            if (src1_id >= 0) st.stack_depth--;
            if (dst_id >= 0) st.stack_depth--;
            
        } else if (n->op == IR_LOAD) {
            int src1_id = find_vreg(&st, n->src1);
            int dst_id = find_vreg(&st, n->dst);
            
            if (src1_id >= 0) bring_to_top(&st, src1_id, out);
            fprintf(out, "    sload\n");
            
            if (src1_id >= 0) st.stack_depth--;
            if (dst_id >= 0) stack_push(&st, dst_id, out);
            
        } else if (n->op == IR_RET) {
            int src1_id = find_vreg(&st, n->src1);
            if (src1_id >= 0) bring_to_top(&st, src1_id, out);
            fprintf(out, "    push1 0x00\n");
            fprintf(out, "    mstore\n");
            fprintf(out, "    push1 0x20\n");
            fprintf(out, "    push1 0x00\n");
            fprintf(out, "    return\n");
        }
        
        pop_dead_vregs(&st, line, out);
    }
    fprintf(out, "  }\n}\n");
}
