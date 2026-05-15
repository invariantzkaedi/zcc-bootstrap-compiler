// ir_evm_stack.c — EVM STACK DISCIPLINE LAYER
#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int depth;
    int max_depth;
    int warning_threshold;
    char stack[1024][IR_NAME_MAX];  /* symbolic value names */
} evm_stack_t;

void evm_stack_init(evm_stack_t* s) {
    s->depth = 0;
    s->max_depth = 0;
    s->warning_threshold = 900;
    memset(s->stack, 0, sizeof(s->stack));
}

int evm_stack_push(evm_stack_t* s, const char* value) {
    if (s->depth >= 1024) {
        fprintf(stderr, "[EVM_STACK] CRITICAL: Stack overflow (1024 limit)\n");
        return -1;
    }
    strncpy(s->stack[s->depth], value, IR_NAME_MAX-1);
    s->depth++;
    if (s->depth > s->max_depth) s->max_depth = s->depth;
    if (s->depth > s->warning_threshold) {
        fprintf(stderr, "[EVM_STACK] WARNING: depth=%d (approaching 1024)\n", s->depth);
    }
    return 0;
}

const char* evm_stack_pop(evm_stack_t* s) {
    if (s->depth == 0) return "STACK_UNDERFLOW";
    s->depth--;
    return s->stack[s->depth];
}

void evm_stack_dup(evm_stack_t* s, int n) {
    if (s->depth < n) return;
    evm_stack_push(s, s->stack[s->depth - n]);
}

void evm_stack_swap(evm_stack_t* s, int n) {
    if (s->depth < n+1) return;
    char tmp[IR_NAME_MAX];
    strncpy(tmp, s->stack[s->depth-1], IR_NAME_MAX-1);
    strncpy(s->stack[s->depth-1], s->stack[s->depth-n-1], IR_NAME_MAX-1);
    strncpy(s->stack[s->depth-n-1], tmp, IR_NAME_MAX-1);
}
