/*
 * evm_lifter.c — ZCC Defensive EVM Bytecode Lifter Scaffold
 *
 * DEFENSIVE AUDIT SCAFFOLD ONLY.
 * This file implements the EVM→IR lifter described in evm_lifter.h.
 * It emits ZCC IR nodes directly using ir.h / ir.c APIs, bypassing the
 * C AST pipeline in part4.c entirely.
 *
 * Build (standalone, no ZCC required):
 *   gcc -O0 -std=c99 -Wall -Wextra -o evm_lifter_test \
 *       tests/test_evm_lifter.c evm_lifter.c ir.c -lm
 *
 * Build with ZCC project:
 *   gcc -O0 -o evm_lifter_test tests/test_evm_lifter.c \
 *       evm_lifter.c ir.c compiler_passes.c compiler_passes_ir.c -lm
 *
 * Coverage note: scaffold tests cover critical paths.  Full 95%+ opcode
 * coverage requires a production harness — see issue tracker gate requirement.
 */

#include "evm_lifter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ────────────────────────────────────────────────── */

/* Allocate a fresh VReg name aligned with ir_bridge.h convention ("t<N>"). */
static void lifter_fresh_tmp(evm_lifter_t *ls, char *buf) {
    /* ir_bridge.h uses "%t<N>"; ir.c uses "t<N>".  We follow ir.c style
     * since we call ir_emit() directly, not through the ZCC codegen macros. */
    ir_fresh_tmp(ls->func, buf);
}

/* Push a named temporary onto the simulated EVM stack. */
static evm_lift_result_t stack_push(evm_lifter_t *ls, const char *name) {
    int i;
    if (ls->stack.depth >= EVM_STACK_MAX) {
        snprintf(ls->errmsg, sizeof(ls->errmsg),
                 "EVM stack overflow at pc=0x%x (depth=%d)",
                 ls->pc, ls->stack.depth);
        ls->error = 1;
        return EVM_LIFT_STACK_OVER;
    }
    /* copy into the next slot */
    for (i = 0; i < IR_NAME_MAX - 1 && name[i]; i++)
        ls->stack.slots[ls->stack.depth][i] = name[i];
    ls->stack.slots[ls->stack.depth][i] = '\0';
    ls->stack.depth++;
    return EVM_LIFT_OK;
}

/* Pop a named temporary from the simulated EVM stack into `out`. */
static evm_lift_result_t stack_pop(evm_lifter_t *ls, char *out) {
    int i;
    const char *src;
    if (ls->stack.depth <= 0) {
        snprintf(ls->errmsg, sizeof(ls->errmsg),
                 "EVM stack underflow at pc=0x%x", ls->pc);
        ls->error = 1;
        return EVM_LIFT_STACK_UNDER;
    }
    ls->stack.depth--;
    src = ls->stack.slots[ls->stack.depth];
    for (i = 0; i < IR_NAME_MAX - 1 && src[i]; i++)
        out[i] = src[i];
    out[i] = '\0';
    return EVM_LIFT_OK;
}

/* Peek at stack[depth-1-offset] without popping. */
static const char *stack_peek(const evm_lifter_t *ls, int offset) {
    int idx = ls->stack.depth - 1 - offset;
    if (idx < 0 || idx >= ls->stack.depth) return "";
    return ls->stack.slots[idx];
}

/*
 * Emit a node tagged with a security tag.
 * Wraps ir_emit() and sets the returned node's .tag field.
 */
static ir_node_t *tagged_emit(evm_lifter_t *ls,
                               ir_op_t op, ir_type_t ty,
                               const char *dst, const char *src1,
                               const char *src2, const char *label,
                               long imm, int lineno,
                               evm_ir_tag_t tag) {
    ir_node_t *n = ir_emit(ls->func, op, ty, dst, src1, src2, label, imm, lineno);
    if (n && tag != IR_TAG_NONE) {
        n->tag = (int)tag;
        ls->tagged_count++;
    }
    return n;
}

/* ── EVM opcode tables ───────────────────────────────────────────────── */

/* Names for the 256 possible byte values.  Most are EVM_INVALID. */
static const char *EVM_OP_NAMES[256] = {
    /* 0x00 */ "STOP",       "ADD",        "MUL",        "SUB",
    /* 0x04 */ "DIV",        "SDIV",       "MOD",        "SMOD",
    /* 0x08 */ "ADDMOD",     "MULMOD",     "EXP",        "SIGNEXTEND",
    /* 0x0c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x10 */ "LT",         "GT",         "SLT",        "SGT",
    /* 0x14 */ "EQ",         "ISZERO",     "AND",        "OR",
    /* 0x18 */ "XOR",        "NOT",        "BYTE",       "SHL",
    /* 0x1c */ "SHR",        "SAR",        "INVALID",    "INVALID",
    /* 0x20 */ "SHA3",       "INVALID",    "INVALID",    "INVALID",
    /* 0x24 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x28 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x2c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x30 */ "ADDRESS",    "BALANCE",    "ORIGIN",     "CALLER",
    /* 0x34 */ "CALLVALUE",  "CALLDATALOAD","CALLDATASIZE","CALLDATACOPY",
    /* 0x38 */ "CODESIZE",   "CODECOPY",   "GASPRICE",   "EXTCODESIZE",
    /* 0x3c */ "EXTCODECOPY","RETURNDATASIZE","RETURNDATACOPY","EXTCODEHASH",
    /* 0x40 */ "BLOCKHASH",  "COINBASE",   "TIMESTAMP",  "NUMBER",
    /* 0x44 */ "PREVRANDAO", "GASLIMIT",   "CHAINID",    "SELFBALANCE",
    /* 0x48 */ "BASEFEE",    "INVALID",    "INVALID",    "INVALID",
    /* 0x4c */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0x50 */ "POP",        "MLOAD",      "MSTORE",     "MSTORE8",
    /* 0x54 */ "SLOAD",      "SSTORE",     "JUMP",       "JUMPI",
    /* 0x58 */ "PC",         "MSIZE",      "GAS",        "JUMPDEST",
    /* 0x5c */ "INVALID",    "INVALID",    "INVALID",    "PUSH0",
    /* 0x60 */ "PUSH1",      "PUSH2",      "PUSH3",      "PUSH4",
    /* 0x64 */ "PUSH5",      "PUSH6",      "PUSH7",      "PUSH8",
    /* 0x68 */ "PUSH9",      "PUSH10",     "PUSH11",     "PUSH12",
    /* 0x6c */ "PUSH13",     "PUSH14",     "PUSH15",     "PUSH16",
    /* 0x70 */ "PUSH17",     "PUSH18",     "PUSH19",     "PUSH20",
    /* 0x74 */ "PUSH21",     "PUSH22",     "PUSH23",     "PUSH24",
    /* 0x78 */ "PUSH25",     "PUSH26",     "PUSH27",     "PUSH28",
    /* 0x7c */ "PUSH29",     "PUSH30",     "PUSH31",     "PUSH32",
    /* 0x80 */ "DUP1",       "DUP2",       "DUP3",       "DUP4",
    /* 0x84 */ "DUP5",       "DUP6",       "DUP7",       "DUP8",
    /* 0x88 */ "DUP9",       "DUP10",      "DUP11",      "DUP12",
    /* 0x8c */ "DUP13",      "DUP14",      "DUP15",      "DUP16",
    /* 0x90 */ "SWAP1",      "SWAP2",      "SWAP3",      "SWAP4",
    /* 0x94 */ "SWAP5",      "SWAP6",      "SWAP7",      "SWAP8",
    /* 0x98 */ "SWAP9",      "SWAP10",     "SWAP11",     "SWAP12",
    /* 0x9c */ "SWAP13",     "SWAP14",     "SWAP15",     "SWAP16",
    /* 0xa0 */ "LOG0",       "LOG1",       "LOG2",       "LOG3",
    /* 0xa4 */ "LOG4",       "INVALID",    "INVALID",    "INVALID",
    /* 0xa8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xac */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xb8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xbc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xc8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xcc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xd8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xdc */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe0 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe4 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xe8 */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xec */ "INVALID",    "INVALID",    "INVALID",    "INVALID",
    /* 0xf0 */ "CREATE",     "CALL",       "CALLCODE",   "RETURN",
    /* 0xf4 */ "DELEGATECALL","CREATE2",   "INVALID",    "INVALID",
    /* 0xf8 */ "INVALID",    "INVALID",    "STATICCALL", "INVALID",
    /* 0xfc */ "INVALID",    "REVERT",     "INVALID",    "SELFDESTRUCT"
};

static const char *EVM_TAG_NAMES[] = {
    "IR_TAG_NONE",
    "IR_TAG_UNTRUSTED_EXTERNAL_CALL",
    "IR_TAG_STATIC_CALL",
    "IR_TAG_SELFDESTRUCT",
    "IR_TAG_SSTORE",
    "IR_TAG_CREATE",
    "IR_TAG_EVM_BARRIER"
};

/* ── Public query functions ──────────────────────────────────────────── */

const char *evm_opcode_name(unsigned int opcode) {
    if (opcode > 0xff) return "INVALID";
    return EVM_OP_NAMES[opcode];
}

const char *evm_tag_name(evm_ir_tag_t tag) {
    int idx = (int)tag;
    if (idx < 0 || idx > (int)IR_TAG_EVM_BARRIER) return "IR_TAG_UNKNOWN";
    return EVM_TAG_NAMES[idx];
}

int evm_is_call_family(unsigned int opcode) {
    return opcode == (unsigned int)EVM_CALL         ||
           opcode == (unsigned int)EVM_CALLCODE     ||
           opcode == (unsigned int)EVM_DELEGATECALL ||
           opcode == (unsigned int)EVM_STATICCALL;
}

/* ── Initialisation ──────────────────────────────────────────────────── */

void evm_lifter_init(evm_lifter_t *ls,
                     const unsigned char *bytecode, int length,
                     ir_module_t *module) {
    int i;
    memset(ls, 0, sizeof(*ls));
    ls->bytecode = bytecode;
    ls->length   = length;
    ls->module   = module;
    /* Create a fresh IR function to represent the lifted contract bytecode.
     * Using IR_TY_I64 for the return type (EVM call result is a success flag). */
    ls->func = ir_func_create(module, "evm_contract", IR_TY_I64, 0);
    /* Warm the stack so all slot strings start NUL-terminated. */
    for (i = 0; i < EVM_STACK_MAX; i++)
        ls->stack.slots[i][0] = '\0';
}

/* ── Step: lift one EVM instruction ─────────────────────────────────── */

/*
 * evm_lift_step() — Lift the single EVM instruction at ls->pc.
 *
 * For the scaffold, we handle:
 *   - PUSH0, PUSH1..PUSH8 with immediate values that fit in long.
 *   - PUSH9..PUSH32 with truncated-to-64-bit immediates (annotated).
 *   - Simple unary/binary arithmetic/bitwise/comparison ops.
 *   - POP, DUP1..DUP16, SWAP1..SWAP16.
 *   - JUMP, JUMPI (flow control — emits IR_BR).
 *   - CALL, CALLCODE, DELEGATECALL, STATICCALL with security tags.
 *   - CREATE, CREATE2 with IR_TAG_CREATE.
 *   - SSTORE with IR_TAG_SSTORE.
 *   - SELFDESTRUCT with IR_TAG_SELFDESTRUCT.
 *   - REVERT, INVALID with IR_TAG_EVM_BARRIER.
 *   - STOP, RETURN with IR_RET.
 *   - JUMPDEST as a labelled IR_LABEL.
 *   - All other opcodes emit an IR_NOP annotated with the opcode name.
 */
evm_lift_result_t evm_lift_step(evm_lifter_t *ls) {
    unsigned int op;
    int push_bytes;
    long imm_val;
    int i;
    char tmp_dst[IR_NAME_MAX];
    char tmp_a[IR_NAME_MAX];
    char tmp_b[IR_NAME_MAX];
    char tmp_c[IR_NAME_MAX];
    char lbl_buf[IR_LABEL_MAX];
    ir_node_t *node;
    evm_lift_result_t res;

    if (ls->pc >= ls->length) {
        /* Normal end-of-stream — not an error. */
        return EVM_LIFT_OK;
    }

    op = (unsigned int)ls->bytecode[ls->pc];
    ls->insn_count++;

    /* ── PUSH0 (EIP-3855) ──────────────────────────────────────────── */
    if (op == (unsigned int)EVM_PUSH0) {
        ls->pc++;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_dst, 0, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);
    }

    /* ── PUSH1..PUSH32 ─────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_PUSH1 && op <= (unsigned int)EVM_PUSH32) {
        push_bytes = (int)(op - (unsigned int)EVM_PUSH1 + 1);
        ls->pc++;  /* advance past the PUSH opcode */

        /* Check bytecode is long enough for the immediate data */
        if (ls->pc + push_bytes > ls->length) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "PUSH%d at pc=0x%x: truncated (need %d bytes, have %d)",
                     push_bytes, ls->pc - 1,
                     push_bytes, ls->length - ls->pc);
            ls->error = 1;
            return EVM_LIFT_TRUNCATED;
        }

        /* Collect up to 8 bytes into a long (EVM words are 256-bit;
         * values wider than 64 bits are silently truncated for this scaffold). */
        imm_val = 0L;
        for (i = 0; i < push_bytes; i++) {
            imm_val = (imm_val << 8) | (long)ls->bytecode[ls->pc + i];
        }
        ls->pc += push_bytes;

        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_dst, 0, 0, 0, imm_val, ls->pc);
        return stack_push(ls, tmp_dst);
    }

    /* ── DUP1..DUP16 ───────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_DUP1 && op <= (unsigned int)EVM_DUP16) {
        int depth = (int)(op - (unsigned int)EVM_DUP1); /* 0 = DUP1 → copies TOS */
        const char *src;
        ls->pc++;
        if (ls->stack.depth <= depth) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "DUP%d at pc=0x%x: stack depth=%d insufficient",
                     depth + 1, ls->pc - 1, ls->stack.depth);
            ls->error = 1;
            return EVM_LIFT_STACK_UNDER;
        }
        src = stack_peek(ls, depth);
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_COPY, IR_TY_I64, tmp_dst, src, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);
    }

    /* ── SWAP1..SWAP16 ─────────────────────────────────────────────── */
    if (op >= (unsigned int)EVM_SWAP1 && op <= (unsigned int)EVM_SWAP16) {
        int depth = (int)(op - (unsigned int)EVM_SWAP1 + 1); /* SWAP1: swap top with [1] */
        int top_idx, tgt_idx;
        char tmp_swap[IR_NAME_MAX];
        ls->pc++;
        if (ls->stack.depth <= depth) {
            snprintf(ls->errmsg, sizeof(ls->errmsg),
                     "SWAP%d at pc=0x%x: stack depth=%d insufficient",
                     depth, ls->pc - 1, ls->stack.depth);
            ls->error = 1;
            return EVM_LIFT_STACK_UNDER;
        }
        top_idx = ls->stack.depth - 1;
        tgt_idx = ls->stack.depth - 1 - depth;
        /* Swap in the stack model */
        memcpy(tmp_swap, ls->stack.slots[top_idx], IR_NAME_MAX);
        memcpy(ls->stack.slots[top_idx], ls->stack.slots[tgt_idx], IR_NAME_MAX);
        memcpy(ls->stack.slots[tgt_idx], tmp_swap, IR_NAME_MAX);
        /* Emit a NOP marker so the IR record shows the swap */
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, 0, 0, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;
    }

    /* ── Everything else: advance PC now, dispatch on opcode ─────── */
    ls->pc++;

    switch (op) {

    /* ── STOP / RETURN / REVERT ──────────────────────────────────── */
    case EVM_STOP:
        ir_emit(ls->func, IR_RET, IR_TY_VOID, 0, "", 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_RETURN:
        res = stack_pop(ls, tmp_a);  /* offset */
        if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b);  /* length */
        if (res != EVM_LIFT_OK) return res;
        ir_emit(ls->func, IR_RET, IR_TY_I64, 0, tmp_a, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_REVERT:
        res = stack_pop(ls, tmp_a);
        if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b);
        if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_RET, IR_TY_VOID, 0, tmp_a, 0, 0, 0L, ls->pc,
                    IR_TAG_EVM_BARRIER);
        return EVM_LIFT_OK;

    case EVM_INVALID:
        tagged_emit(ls, IR_NOP, IR_TY_VOID, 0, 0, 0, 0, 0L, ls->pc,
                    IR_TAG_EVM_BARRIER);
        return EVM_LIFT_OK;

    /* ── POP ─────────────────────────────────────────────────────── */
    case EVM_POP:
        return stack_pop(ls, tmp_a);   /* discard the value */

    /* ── Arithmetic: pops 2, pushes 1 ───────────────────────────── */
    case EVM_ADD:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_ADD, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SUB:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_SUB, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_MUL:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_MUL, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_DIV:
    case EVM_SDIV:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_DIV, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_MOD:
    case EVM_SMOD:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_MOD, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    /* ── Comparison ops: pops 2, pushes i32 0/1 ─────────────────── */
    case EVM_LT:
    case EVM_SLT:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LT, IR_TY_I32, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_GT:
    case EVM_SGT:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_GT, IR_TY_I32, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_EQ:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_EQ, IR_TY_I32, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_ISZERO:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        /* ISZERO: push (a == 0) */
        lifter_fresh_tmp(ls, tmp_b);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_b, 0, 0, 0, 0L, ls->pc);
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_EQ, IR_TY_I32, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    /* ── Bitwise ops ─────────────────────────────────────────────── */
    case EVM_AND:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_AND, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_OR:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_OR, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_XOR:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_XOR, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_NOT:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOT, IR_TY_I64, tmp_dst, tmp_a, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SHL:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_SHL, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SHR:
    case EVM_SAR:
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_SHR, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    /* ── Memory/storage ops ──────────────────────────────────────── */
    case EVM_MLOAD:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_MSTORE:
    case EVM_MSTORE8:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        ir_emit(ls->func, IR_STORE, IR_TY_I64, tmp_a, tmp_b, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_SLOAD:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SSTORE:
        /* Persistent storage write — security-tagged */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_STORE, IR_TY_I64, tmp_a, tmp_b, 0, 0, 0L, ls->pc,
                    IR_TAG_SSTORE);
        return EVM_LIFT_OK;

    /* ── Flow control ────────────────────────────────────────────── */
    case EVM_JUMP:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        /* Indirect jump — we can't resolve the target statically here.
         * Emit a NOP with the target temp as src1 for future CFG pass. */
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, 0, tmp_a, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_JUMPI:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        /* Conditional branch — emit BR_IF with condition in src1 */
        ir_emit(ls->func, IR_BR_IF, IR_TY_VOID, 0, tmp_b, 0, tmp_a, 0L, ls->pc);
        return EVM_LIFT_OK;

    case EVM_JUMPDEST:
        /* Define a labelled entry point in the IR */
        snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%d", ls->pc - 1);
        ir_emit(ls->func, IR_LABEL, IR_TY_VOID, 0, 0, 0, lbl_buf, 0L, ls->pc);
        return EVM_LIFT_OK;

    /* ── Environment queries — zero-operand pushes ───────────────── */
    case EVM_ADDRESS:
    case EVM_ORIGIN:
    case EVM_CALLER:
    case EVM_CALLVALUE:
    case EVM_CALLDATASIZE:
    case EVM_CODESIZE:
    case EVM_GASPRICE:
    case EVM_RETURNDATASIZE:
    case EVM_COINBASE:
    case EVM_TIMESTAMP:
    case EVM_NUMBER:
    case EVM_PREVRANDAO:
    case EVM_GASLIMIT:
    case EVM_CHAINID:
    case EVM_SELFBALANCE:
    case EVM_BASEFEE:
    case EVM_PC:
    case EVM_MSIZE:
    case EVM_GAS:
        /* These push an environment-derived value; model as CONST 0 for now */
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_CONST, IR_TY_I64, tmp_dst, 0, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_BALANCE:
    case EVM_EXTCODESIZE:
    case EVM_EXTCODEHASH:
    case EVM_BLOCKHASH:
    case EVM_CALLDATALOAD:
        /* Pop 1, push 1 (address/key → value) */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_LOAD, IR_TY_I64, tmp_dst, tmp_a, 0, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_SHA3:
    case EVM_CALLDATACOPY:
    case EVM_CODECOPY:
    case EVM_ADDMOD:
    case EVM_MULMOD:
    case EVM_EXP:
    case EVM_SIGNEXTEND:
    case EVM_BYTE:
        /* Pop 2 or 3, push 0 or 1 — emit NOP for scaffold */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        ir_emit(ls->func, IR_NOP, IR_TY_I64, tmp_dst, tmp_a, tmp_b, 0, 0L, ls->pc);
        return stack_push(ls, tmp_dst);

    case EVM_EXTCODECOPY:
    case EVM_RETURNDATACOPY:
        /* Pop 4 */
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_b); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_c); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, tmp_dst); if (res != EVM_LIFT_OK) return res;
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, 0, 0, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;

    /* ── LOG0..LOG4 ──────────────────────────────────────────────── */
    case EVM_LOG0:
    case EVM_LOG1:
    case EVM_LOG2:
    case EVM_LOG3:
    case EVM_LOG4: {
        /* LOG_n pops (2 + n) args: offset, length, topic0..topicN */
        int log_topics = (int)(op - (unsigned int)EVM_LOG0);
        int log_pops   = 2 + log_topics;
        int j;
        for (j = 0; j < log_pops; j++) {
            res = stack_pop(ls, tmp_a);
            if (res != EVM_LIFT_OK) return res;
        }
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, 0, 0, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;
    }

    /* ── CALL-family: security-critical ─────────────────────────── */
    /*
     * EVM CALL stack signature (7 args):
     *   gas, addr, value, argsOffset, argsLength, retOffset, retLength
     * Pops all 7, pushes success flag (0 or 1).
     *
     * DELEGATECALL / STATICCALL (6 args):
     *   gas, addr, argsOffset, argsLength, retOffset, retLength
     *
     * CALLCODE (7 args, same as CALL):
     *   gas, addr, value, argsOffset, argsLength, retOffset, retLength
     *
     * All CALL-family opcodes are tagged IR_TAG_UNTRUSTED_EXTERNAL_CALL
     * except STATICCALL which is tagged IR_TAG_STATIC_CALL.
     */
    case EVM_CALL:
    case EVM_CALLCODE: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX], value[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, value); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);    if (res != EVM_LIFT_OK) return res;

        lifter_fresh_tmp(ls, tmp_dst);
        /* Emit IR_CALL targeting the address held in `addr` temp */
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, 0, 0, addr, 0L, ls->pc,
                           IR_TAG_UNTRUSTED_EXTERNAL_CALL);
        /* Emit ARG nodes for gas, value (key audit parameters) */
        ir_emit(ls->func, IR_ARG, IR_TY_I64, 0, gas,   0, 0, 0L, ls->pc);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, 0, value, 0, 0, 0L, ls->pc);
        (void)node;
        ls->call_count++;
        /* Push the success flag */
        return stack_push(ls, tmp_dst);
    }

    case EVM_DELEGATECALL: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);   if (res != EVM_LIFT_OK) return res;

        lifter_fresh_tmp(ls, tmp_dst);
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, 0, 0, addr, 0L, ls->pc,
                           IR_TAG_UNTRUSTED_EXTERNAL_CALL);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, 0, gas, 0, 0, 0L, ls->pc);
        (void)node;
        ls->call_count++;
        return stack_push(ls, tmp_dst);
    }

    case EVM_STATICCALL: {
        char gas[IR_NAME_MAX], addr[IR_NAME_MAX];
        char ao[IR_NAME_MAX], al[IR_NAME_MAX], ro[IR_NAME_MAX], rl[IR_NAME_MAX];
        res = stack_pop(ls, gas);  if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, addr); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ao);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, al);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, ro);   if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, rl);   if (res != EVM_LIFT_OK) return res;

        lifter_fresh_tmp(ls, tmp_dst);
        node = tagged_emit(ls, IR_CALL, IR_TY_I64,
                           tmp_dst, 0, 0, addr, 0L, ls->pc,
                           IR_TAG_STATIC_CALL);
        ir_emit(ls->func, IR_ARG, IR_TY_I64, 0, gas, 0, 0, 0L, ls->pc);
        (void)node;
        ls->call_count++;
        return stack_push(ls, tmp_dst);
    }

    /* ── CREATE / CREATE2 ────────────────────────────────────────── */
    case EVM_CREATE: {
        char val[IR_NAME_MAX], offset[IR_NAME_MAX], length[IR_NAME_MAX];
        res = stack_pop(ls, val);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, offset); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, length); if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CALL, IR_TY_PTR,
                    tmp_dst, val, offset, "evm_create", 0L, ls->pc,
                    IR_TAG_CREATE);
        return stack_push(ls, tmp_dst);
    }

    case EVM_CREATE2: {
        char val[IR_NAME_MAX], offset[IR_NAME_MAX], length[IR_NAME_MAX];
        char salt[IR_NAME_MAX];
        res = stack_pop(ls, val);    if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, offset); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, length); if (res != EVM_LIFT_OK) return res;
        res = stack_pop(ls, salt);   if (res != EVM_LIFT_OK) return res;
        lifter_fresh_tmp(ls, tmp_dst);
        tagged_emit(ls, IR_CALL, IR_TY_PTR,
                    tmp_dst, val, offset, "evm_create2", 0L, ls->pc,
                    IR_TAG_CREATE);
        return stack_push(ls, tmp_dst);
    }

    /* ── SELFDESTRUCT ────────────────────────────────────────────── */
    case EVM_SELFDESTRUCT:
        res = stack_pop(ls, tmp_a); if (res != EVM_LIFT_OK) return res;
        tagged_emit(ls, IR_RET, IR_TY_VOID, 0, tmp_a, 0, 0, 0L, ls->pc,
                    IR_TAG_SELFDESTRUCT);
        return EVM_LIFT_OK;

    /* ── Unknown / unimplemented ─────────────────────────────────── */
    default:
        ir_emit(ls->func, IR_NOP, IR_TY_VOID, 0, 0, 0, 0, 0L, ls->pc);
        return EVM_LIFT_OK;
    }
}

/* ── Full bytecode lift ──────────────────────────────────────────────── */

evm_lift_result_t evm_lift_bytecode(evm_lifter_t *ls) {
    evm_lift_result_t res;

    while (ls->pc < ls->length) {
        res = evm_lift_step(ls);
        if (res == EVM_LIFT_TRUNCATED || res == EVM_LIFT_OOM) {
            return res;
        }
        if (res == EVM_LIFT_INVALID_OP) {
            /* Record but continue — defensive lifter tolerates unknown opcodes */
            return res;
        }
        if (res == EVM_LIFT_STACK_OVER || res == EVM_LIFT_STACK_UNDER) {
            return res;
        }
        /* EVM_LIFT_OK — proceed to next instruction */
    }
    return EVM_LIFT_OK;
}
