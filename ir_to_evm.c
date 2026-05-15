// ir_to_evm.c — EVM BYTECODE LIFTER v0.1
#include "ir_to_evm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void evm_stack_init(void* s);
extern int evm_stack_push(void* s, const char* value);
extern const char* evm_stack_pop(void* s);

static int evm_gas_cost(uint8_t opcode) { return 3; }

static void evm_emit(evm_builder_t* b, uint8_t opcode) {
    if (b->len + 1 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 1024;
        b->code = (uint8_t*)realloc(b->code, b->cap);
    }
    b->code[b->len++] = opcode;
    b->gas_used += evm_gas_cost(opcode);
}

// Core IR → EVM lowering
void ir_to_evm(ir_func_t* func, evm_builder_t* out) {
    char stack_mem[1024 * 128 + 32];
    void* stack = &stack_mem;
    evm_stack_init(stack);

    // ir_snapshot_state(func->module, "evm_lift_begin"); // assuming ir_snapshot_state exists

    for (ir_node_t* n = func->head; n; n = n->next) {
        switch (n->op) {
            case IR_ADD:
            case IR_MUL:
            case IR_SUB:
            case IR_DIV:
            case IR_MOD:
                evm_stack_pop(stack);  // rhs
                evm_stack_pop(stack);  // lhs
                evm_stack_push(stack, "%t_result");
                evm_emit(out, 0x01 + (n->op - IR_ADD));  // ADD=0x01 ... MOD=0x06
                break;

            case IR_CALL:
                // EVM CALL convention
                evm_emit(out, 0xF1);  // CALL
                break;

            case IR_STORE:
                evm_stack_pop(stack);  // value
                evm_stack_pop(stack);  // key
                evm_emit(out, 0x55);
                break;

            case IR_RET:
                evm_emit(out, 0xF3); // RETURN
                break;
            // ... more opcodes
            default:
                break;
        }
    }

    // ir_snapshot_state(func->module, "evm_lift_complete");
}
